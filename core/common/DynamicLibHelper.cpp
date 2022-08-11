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

#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#endif

namespace logtail {


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
bool DynamicLibLoader::LoadDynLib(const std::string& libName, std::string& error, const std::string dlPrefix) {
    error.clear();

#if defined(__linux__)
    mLibPtr = dlopen((dlPrefix + "lib" + libName + ".so").c_str(), RTLD_LAZY);
#elif defined(_MSC_VER)
    mLibPtr = LoadLibrary((dlPrefix + libName + ".dll").c_str());
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


} // namespace logtail