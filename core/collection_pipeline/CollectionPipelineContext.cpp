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

#include "collection_pipeline/CollectionPipelineContext.h"

#include "collection_pipeline/queue/QueueKeyManager.h"
#include "plugin/flusher/sls/FlusherSLS.h"

using namespace std;

namespace logtail {

const string CollectionPipelineContext::sEmptyString = "";

const string& CollectionPipelineContext::GetProjectName() const {
    return mSLSInfo ? mSLSInfo->mProject : sEmptyString;
}

const string& CollectionPipelineContext::GetLogstoreName() const {
    return mSLSInfo ? mSLSInfo->mLogstore : sEmptyString;
}

const string& CollectionPipelineContext::GetRegion() const {
    return mSLSInfo ? mSLSInfo->mRegion : sEmptyString;
}

QueueKey CollectionPipelineContext::GetLogstoreKey() const {
    if (mSLSInfo) {
        return mSLSInfo->GetQueueKey();
    }
    static QueueKey key = QueueKeyManager::GetInstance()->GetKey(mConfigName + "-flusher_sls-");
    return key;
}

} // namespace logtail
