/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <cstdint>

extern "C" {
typedef long long GoInt64;
typedef GoInt64 GoInt;
typedef struct {
    const char* p;
    GoInt64 n;
} GoString;
typedef struct {
    void* data;
    GoInt len;
    GoInt cap;
} GoSlice;
typedef GoInt (*LogtailSendPbFun)(GoString project, GoString logstore, GoSlice pbData, GoInt lines, GoInt rawBytes);

typedef GoInt (*InitLogtailRuntimeFun)();

typedef GoInt (*DestroyLogtailRuntimeFun)();


typedef void (*DestroyPbFun)(void* loggroupPb);
typedef int (*UpdateLogtailConfigFun)(const char* configJSON, int configJSONSize);

typedef void (*RegisterLogtailCallBackFun)(UpdateLogtailConfigFun updateFun);
}

// Create by david zhang. 2017/09/02 22:22:12
class LogtailRuntimePlugin {
public:
    LogtailRuntimePlugin();
    ~LogtailRuntimePlugin();


    static LogtailRuntimePlugin* GetInstance() {
        if (s_instance == NULL) {
            s_instance = new LogtailRuntimePlugin;
        }
        return s_instance;
    }

    static void FinalizeInstance() {
        if (s_instance != NULL) {
            delete s_instance;
            s_instance = NULL;
        }
    }

    static int UpdateLogtailConfig(const char* configJSON, int configJSONSize);

    bool LoadPluginBase();

    void UnLoadPluginBase();

    void LogtailSendPb(const std::string& project,
                       const std::string& logstore,
                       const char* lz4PbBuffer,
                       int32_t lz4BufferSize,
                       int32_t pbSize,
                       int32_t lines);

private:
    void* mPluginBasePtr;
    void* mPluginAdapterPtr;

    LogtailSendPbFun mSendPbFun;
    InitLogtailRuntimeFun mInitFun;
    DestroyLogtailRuntimeFun mDestroyFun;
    bool mPluginValid;

private:
    static LogtailRuntimePlugin* s_instance;
};
