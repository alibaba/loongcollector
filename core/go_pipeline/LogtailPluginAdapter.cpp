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

#include "LogtailPluginAdapter.h"

#include <stdio.h>

IsValidToSendFun gAdapterIsValidToSendFun = NULL;
SendPbFun gAdapterSendPbFun = NULL;
SendPbV2Fun gAdapterSendPbV2Fun = NULL;

void RegisterLogtailCallBack(IsValidToSendFun checkFun, SendPbFun sendFun) {
    fprintf(stderr, "[GoPluginAdapter] register fun %p %p\n", checkFun, sendFun);
    gAdapterIsValidToSendFun = checkFun;
    gAdapterSendPbFun = sendFun;
}

void RegisterLogtailCallBackV2(IsValidToSendFun checkFun, SendPbFun sendV1Fun, SendPbV2Fun sendV2Fun) {
    fprintf(stderr, "register fun v2 %p %p %p \n", checkFun, sendV1Fun, sendV2Fun);
    gAdapterIsValidToSendFun = checkFun;
    gAdapterSendPbFun = sendV1Fun;
    gAdapterSendPbV2Fun = sendV2Fun;
}

int LogtailIsValidToSend(long long logstoreKey) {
    if (gAdapterIsValidToSendFun == NULL) {
        return -1;
    }
    return gAdapterIsValidToSendFun(logstoreKey);
}

int LogtailSendPb(const char* configName,
                  int configNameSize,
                  const char* logstore,
                  int logstoreSize,
                  char* pbBuffer,
                  int pbSize,
                  int lines) {
    if (gAdapterSendPbFun == NULL) {
        return -1;
    }
    return gAdapterSendPbFun(configName, configNameSize, logstore, logstoreSize, pbBuffer, pbSize, lines);
}

int LogtailSendPbV2(const char* configName,
                    int configNameSize,
                    const char* logstore,
                    int logstoreSize,
                    char* pbBuffer,
                    int pbSize,
                    int lines,
                    const char* shardHash,
                    int shardHashSize) {
    if (NULL == gAdapterSendPbV2Fun) {
        return -1;
    }
    return gAdapterSendPbV2Fun(
        configName, configNameSize, logstore, logstoreSize, pbBuffer, pbSize, lines, shardHash, shardHashSize);
}

// # 300
//   - Add LogtailSendPbV2.
//   - Update RegisterLogtailCallBack to register LogtailSendPBV2.
int PluginAdapterVersion() {
    return 300;
}
