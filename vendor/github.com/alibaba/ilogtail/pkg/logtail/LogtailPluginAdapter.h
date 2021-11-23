/*
 * Copyright 2021 iLogtail Authors
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

#ifndef LOGTAILPLUGINADAPTER_H__
#define LOGTAILPLUGINADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

    typedef int(*IsValidToSendFun)(long long logstoreKey);

    typedef int(*SendPbFun)(const char * configName, int configNameSize, const char * logstore, int logstoreSize, char * pbBuffer, int pbSize, int lines);

    typedef int(*SendPbV2Fun)(const char *configName, int configNameSize,
        const char *logstore, int logstoreSize,
        char *pbBuffer, int pbSize,
        int lines,
        const char *shardHash, int shardHashSize);

    typedef int(*PluginCtlCmdFun)(const char * configName, int configNameSize, int optId, const char * params, int paramsLen);

    void RegisterLogtailCallBack(IsValidToSendFun checkFun, SendPbFun sendFun, PluginCtlCmdFun cmdFun);

    void RegisterLogtailCallBackV2(IsValidToSendFun checkFun,
        SendPbFun sendV1Fun,
        SendPbV2Fun sendV2Fun,
        PluginCtlCmdFun cmdFun);

    int LogtailIsValidToSend(long long logstoreKey);

    int LogtailSendPb(const char * configName, int configNameSize, const char * logstore, int logstoreSize, char * pbBuffer, int pbSize, int lines);

    int LogtailSendPbV2(const char *configName, int configNameSize,
        const char *logstore, int logstoreSize,
        char *pbBuffer, int pbSize,
        int lines,
        const char *shardHash, int shardHashSize);

    int LogtailCtlCmd(const char * configName, int configNameSize, int cmdId, const char * params, int paramsLen);

    // version for logtail plugin adapter, used for check plugin adapter version
    int PluginAdapterVersion();

#ifdef __cplusplus
}
#endif


#endif // LOGTAILPLUGINADAPTER_H__
