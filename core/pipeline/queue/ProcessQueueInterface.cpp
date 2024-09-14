// Copyright 2024 iLogtail Authors
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

#include "pipeline/queue/ProcessQueueInterface.h"

#include "pipeline/queue/BoundedSenderQueueInterface.h"

using namespace std;

namespace logtail {

ProcessQueueInterface::ProcessQueueInterface(int64_t key, size_t cap, uint32_t priority, const PipelineContext& ctx)
    : QueueInterface(key, cap, ctx), mPriority(priority), mConfigName(ctx.GetConfigName()) {
    mMetricsRecordRef.AddLabels({{METRIC_LABEL_KEY_COMPONENT_NAME, "process_queue"}});
}

void ProcessQueueInterface::SetDownStreamQueues(vector<BoundedSenderQueueInterface*>&& ques) {
    mDownStreamQueues.clear();
    for (auto& item : ques) {
        if (item == nullptr) {
            // should not happen
            continue;
        }
        mDownStreamQueues.emplace_back(item);
    }
}

bool ProcessQueueInterface::IsValidToPop() const {
    return mValidToPop && !Empty() && IsDownStreamQueuesValidToPush();
}

bool ProcessQueueInterface::IsDownStreamQueuesValidToPush() const {
    // TODO: support other strategy
    for (const auto& q : mDownStreamQueues) {
        if (!q->IsValidToPush()) {
            return false;
        }
    }
    return true;
}

} // namespace logtail
