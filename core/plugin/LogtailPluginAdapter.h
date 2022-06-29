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
#if defined(_MSC_VER) || defined(_WIN32)
#if defined(PLUGIN_ADAPTER_EXPORTS)
#define PLUGIN_ADAPTER_API __declspec(dllexport)
#else
#define PLUGIN_ADAPTER_API __declspec(dllimport)
#endif
#else
#define PLUGIN_ADAPTER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*IsValidToSendFun)(long long logstoreKey);
typedef int (*SendPbFun)(const char* configName,
                         int configNameSize,
                         const char* logstore,
                         int logstoreSize,
                         char* pbBuffer,
                         int pbSize,
                         int lines);
typedef int (*SendPbV2Fun)(const char* configName,
                           int configNameSize,
                           const char* logstore,
                           int logstoreSize,
                           char* pbBuffer,
                           int pbSize,
                           int lines,
                           const char* shardHash,
                           int shardHashSize);
typedef int (*PluginCtlCmdFun)(
    const char* configName, int configNameSize, int optId, const char* params, int paramsLen);

PLUGIN_ADAPTER_API void RegisterLogtailCallBack(IsValidToSendFun checkFun, SendPbFun sendFun, PluginCtlCmdFun cmdFun);

PLUGIN_ADAPTER_API void RegisterLogtailCallBackV2(IsValidToSendFun checkFun,
                                                  SendPbFun sendV1Fun,
                                                  SendPbV2Fun sendV2Fun,
                                                  PluginCtlCmdFun cmdFun);

PLUGIN_ADAPTER_API int LogtailIsValidToSend(long long logstoreKey);

PLUGIN_ADAPTER_API int LogtailSendPb(const char* configName,
                                     int configNameSize,
                                     const char* logstore,
                                     int logstoreSize,
                                     char* pbBuffer,
                                     int pbSize,
                                     int lines);

PLUGIN_ADAPTER_API int LogtailSendPbV2(const char* configName,
                                       int configNameSize,
                                       const char* logstore,
                                       int logstoreSize,
                                       char* pbBuffer,
                                       int pbSize,
                                       int lines,
                                       const char* shardHash,
                                       int shardHashSize);

PLUGIN_ADAPTER_API int
LogtailCtlCmd(const char* configName, int configNameSize, int cmdId, const char* params, int paramsLen);

// version for logtail plugin adapter, used for check plugin adapter version
PLUGIN_ADAPTER_API int PluginAdapterVersion();

#ifdef __cplusplus
}
#endif
