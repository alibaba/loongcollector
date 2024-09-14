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

#include "plugin/flusher/blackhole/FlusherBlackHole.h"

#include "pipeline/queue/SenderQueueManager.h"

using namespace std;

namespace logtail {

const string FlusherBlackHole::sName = "flusher_blackhole";

bool FlusherBlackHole::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    static uint32_t cnt = 0;
    GenerateQueueKey(to_string(++cnt));
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mNodeID, *mContext);
    return true;
}

bool FlusherBlackHole::Send(PipelineEventGroup&& g) {
    return PushToQueue(make_unique<SenderQueueItem>("", 0, this, mQueueKey));
}

} // namespace logtail
