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

namespace {
IsValidToSendFun gIsValidToSend = nullptr;
SendPipelineEventGroupFun gSendPipelineEventGroup = nullptr;
} // namespace

void RegisterIsValidToSend(IsValidToSendFun checkFun) {
    gIsValidToSend = checkFun;
}

void RegisterSendPipelineEventGroup(SendPipelineEventGroupFun sendFun) {
    gSendPipelineEventGroup = sendFun;
}

int LogtailIsValidToSend(long long logstoreKey) {
    if (gIsValidToSend == nullptr) {
        return -1;
    }
    return gIsValidToSend(logstoreKey);
}

int LogtailSendPipelineEventGroup(const char* configName,
                                 int configNameSize,
                                 const char* logstore,
                                 int logstoreSize,
                                 char* pbBuffer,
                                 int pbSize,
                                 int eventCount,
                                 const char* shardHash,
                                 int shardHashSize) {
    if (gSendPipelineEventGroup == nullptr) {
        return -1;
    }
    return gSendPipelineEventGroup(
        configName, configNameSize, logstore, logstoreSize, pbBuffer, pbSize, eventCount, shardHash, shardHashSize);
}

int PluginAdapterVersion() {
    return 500;
}
