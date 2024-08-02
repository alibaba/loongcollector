// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/PipelineContext.h"

#include "flusher/sls/FlusherSLS.h"
#include "queue/QueueKeyManager.h"

using namespace std;

namespace logtail {

const string PipelineContext::sEmptyString = "";

const string& PipelineContext::GetProjectName() const {
    return mSLSInfo ? mSLSInfo->mProject : sEmptyString;
}

const string& PipelineContext::GetLogstoreName() const {
    return mSLSInfo ? mSLSInfo->mLogstore : sEmptyString;
}

const string& PipelineContext::GetRegion() const {
    return mSLSInfo ? mSLSInfo->mRegion : sEmptyString;
}

QueueKey PipelineContext::GetLogstoreKey() const {
    if (mSLSInfo) {
        return mSLSInfo->GetQueueKey();
    }
    static QueueKey key = QueueKeyManager::GetInstance()->GetKey(mConfigName + "-flusher_sls-");
    return key;
}

} // namespace logtail
