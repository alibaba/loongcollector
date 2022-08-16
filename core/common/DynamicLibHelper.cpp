// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "DynamicLibHelper.h"
#include "Logger.h"

#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#endif

namespace logtail {

namespace glibc {
    glibc_setns_func g_setns_func = nullptr;
    DynamicLibLoader* g_loader = nullptr;
} // namespace glibc


std::string DynamicLibLoader::GetError() {
#if defined(__linux__)
    auto dlErr = dlerror();
    return (dlErr != NULL) ? dlErr : "";
#elif defined(_MSC_VER)
    return std::to_string(GetLastError());
#endif
}

void DynamicLibLoader::CloseLib(void* libPtr) {
    if (nullptr == libPtr)
        return;
#if defined(__linux__)
    dlclose(libPtr);
#elif defined(_MSC_VER)
    FreeLibrary((HINSTANCE)libPtr);
#endif
}

// Release releases the ownership of @mLibPtr to caller.
void* DynamicLibLoader::Release() {
    auto ptr = mLibPtr;
    mLibPtr = nullptr;
    return ptr;
}

DynamicLibLoader::~DynamicLibLoader() {
    CloseLib(mLibPtr);
}

// LoadDynLib loads dynamic library named @libName from current working dir.
// For linux, the so name is 'lib+@libName.so'.
// For Windows, the dll name is '@libName.dll'.
// @return a non-NULL ptr to indicate lib handle, otherwise nullptr is returned.
bool DynamicLibLoader::LoadDynLib(const std::string& libName,
                                  std::string& error,
                                  const std::string dlPrefix,
                                  const std::string dlSuffix) {
    error.clear();

#if defined(__linux__)
    mLibPtr = dlopen((dlPrefix + "lib" + libName + ".so" + dlSuffix).c_str(), RTLD_LAZY);
#elif defined(_MSC_VER)
    mLibPtr = LoadLibrary((dlPrefix + libName + ".dll" + dlSuffix).c_str());
#endif
    if (mLibPtr != NULL)
        return true;
    error = GetError();
    return false;
}

// LoadMethod loads method named @methodName from opened lib.
// If error is found, @error param will be set, otherwise empty.
void* DynamicLibLoader::LoadMethod(const std::string& methodName, std::string& error) {
    error.clear();

#if defined(__linux__)
    dlerror(); // Clear last error.
    auto methodPtr = dlsym(mLibPtr, methodName.c_str());
    error = GetError();
    return methodPtr;
#elif defined(_MSC_VER)
    auto methodPtr = GetProcAddress((HINSTANCE)mLibPtr, methodName.c_str());
    if (methodPtr != NULL)
        return methodPtr;
    error = std::to_string(GetLastError());
    return NULL;
#endif
}

bool glibc::LoadGlibcFunc() {
    if (g_loader == nullptr) {
        LOG_INFO(sLogger, ("load glibc dynamic library", "begin"));
        g_loader = new DynamicLibLoader;
        std::string loadErr;
        if (!g_loader->LoadDynLib("c", loadErr, "", ".6")) {
            LOG_ERROR(sLogger, ("load glibc dynamic library", "failed")("error", loadErr));
            return false;
        }

        void* setNsFunc = g_loader->LoadMethod("setns", loadErr);
        if (setNsFunc == nullptr) {
            LOG_ERROR(sLogger, ("load glibc method", "failed")("method", "setns")("error", loadErr));
            return false;
        }
        g_setns_func = glibc_setns_func(setNsFunc);
        LOG_INFO(sLogger, ("load glibc dynamic library", "success"));
    }
    return g_setns_func != nullptr;
}

} // namespace logtail