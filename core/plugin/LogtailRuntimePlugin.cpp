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

#include "LogtailRuntimePlugin.h"
#if defined(__linux__)
#include <dlfcn.h>
#endif
#include "config_manager/ConfigManager.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "logger/Logger.h"

using namespace std;
using namespace logtail;

LogtailRuntimePlugin* LogtailRuntimePlugin::s_instance = NULL;

LogtailRuntimePlugin::LogtailRuntimePlugin() {
    mPluginAdapterPtr = NULL;
    mPluginBasePtr = NULL;
    mPluginValid = false;
    mSendPbFun = NULL;
    mInitFun = NULL;
    mDestroyFun = NULL;
}

LogtailRuntimePlugin::~LogtailRuntimePlugin() {
    // TODO: Add implementation for Windows.
#if defined(__linux__)
    if (mPluginBasePtr != NULL) {
        dlclose(mPluginBasePtr);
    }
    if (mPluginAdapterPtr != NULL) {
        dlclose(mPluginAdapterPtr);
    }
#endif
}


void LogtailRuntimePlugin::LogtailSendPb(const std::string& project,
                                         const std::string& logstore,
                                         const char* lz4PbBuffer,
                                         int32_t lz4BufferSize,
                                         int32_t pbSize,
                                         int32_t lines) {
    if (mPluginValid && mSendPbFun != NULL) {
        GoString goProject;
        GoString goLogstore;
        GoSlice goLz4Buffer;

        goProject.n = project.size();
        goProject.p = project.c_str();

        goLogstore.n = logstore.size();
        goLogstore.p = logstore.c_str();
        goLz4Buffer.len = lz4BufferSize;
        goLz4Buffer.cap = lz4BufferSize;
        goLz4Buffer.data = (void*)lz4PbBuffer;
        GoInt rst = mSendPbFun(goProject, goLogstore, goLz4Buffer, GoInt(lines), GoInt(pbSize));
        if (rst != (GoInt)0) {
            LOG_WARNING(sLogger, ("send log error, project", project)("logstore", logstore)("result", rst));
        }
    }
}

int LogtailRuntimePlugin::UpdateLogtailConfig(const char* configJSON, int configJSONSize) {
    printf("LogtailRuntimePlugin");
    if (!LogtailRuntimePlugin::GetInstance()->mPluginValid)
        return 255;
    printf("   LogtailRuntimePlugin\n");
    return ConfigManager::GetInstance()->UpdateConfigJson(string(configJSON, configJSONSize));
}

void LogtailRuntimePlugin::UnLoadPluginBase() {
    if (!mPluginValid || mDestroyFun == NULL)
        return;
    mDestroyFun();
    return;
}

bool LogtailRuntimePlugin::LoadPluginBase() {
    if (mPluginValid)
        return true;
    LOG_INFO(sLogger, ("load plugin base dl file", (GetProcessExecutionDir() + "libRuntimePluginAdapter.so")));
    // TODO: Add implementation for Windows.
#if defined(__linux__)
    mPluginAdapterPtr = dlopen((GetProcessExecutionDir() + "libRuntimePluginAdapter.so").c_str(), RTLD_LAZY);
    if (mPluginAdapterPtr == NULL) {
        LOG_ERROR(sLogger, ("open adapter dl error, Message", dlerror()));
        return false;
    }

    RegisterLogtailCallBackFun registerFun
        = (RegisterLogtailCallBackFun)dlsym(mPluginAdapterPtr, "RegisterLogtailCallBack");
    char* szError = dlerror();
    if (szError != NULL) {
        LOG_ERROR(sLogger, ("load function error, Message", szError));
        dlclose(mPluginBasePtr);
        mPluginBasePtr = NULL;
        return false;
    }
    registerFun(LogtailRuntimePlugin::UpdateLogtailConfig);

    mPluginBasePtr = dlopen((GetProcessExecutionDir() + "libRuntimePluginBase.so").c_str(), RTLD_LAZY);
    if (mPluginBasePtr == NULL) {
        LOG_ERROR(sLogger, ("open plugin base dl error, Message", dlerror()));
        return false;
    }

    mInitFun = (InitLogtailRuntimeFun)dlsym(mPluginBasePtr, "InitLogtailRuntime");
    szError = dlerror();
    if (szError != NULL) {
        LOG_ERROR(sLogger, ("load function error, Message", szError));
        dlclose(mPluginBasePtr);
        mPluginBasePtr = NULL;
        return false;
    }
    mDestroyFun = (DestroyLogtailRuntimeFun)dlsym(mPluginBasePtr, "DestroyLogtailRuntime");
    szError = dlerror();
    if (szError != NULL) {
        LOG_ERROR(sLogger, ("load function error, Message", szError));
        dlclose(mPluginBasePtr);
        mPluginBasePtr = NULL;
        return false;
    }
    mSendPbFun = (LogtailSendPbFun)dlsym(mPluginBasePtr, "LogtailSendPb");
    szError = dlerror();
    if (szError != NULL) {
        LOG_ERROR(sLogger, ("load function error, Message", szError));
        dlclose(mPluginBasePtr);
        mPluginBasePtr = NULL;
        return false;
    }
#endif

    GoInt initRst = mInitFun();
    if (initRst != 0) {
        LOG_ERROR(sLogger, ("init plugin base error", initRst));
        mPluginValid = false;
    } else {
        mPluginValid = true;
    }
    return mPluginValid;
}
