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

#include "queue/ProcessQueue.h"

using namespace std;

namespace logtail {

bool ProcessQueue::Push(unique_ptr<ProcessQueueItem>&& item) {
    if (!IsValidToPush()) {
        return false;
    }
    mQueue.push(std::move(item));
    ChangeStateIfNeededAfterPush();
    return true;
}

bool ProcessQueue::Pop(unique_ptr<ProcessQueueItem>& item) {
    if (!IsValidToPop()) {
        return false;
    }
    item = std::move(mQueue.front());
    mQueue.pop();
    if (ChangeStateIfNeededAfterPop()) {
        GiveFeedback();
    }
    return true;
}

bool ProcessQueue::IsDownStreamQueuesValidToPush() const {
    // TODO: support other strategy
    for (const auto& q : mDownStreamQueues) {
        // TODO: replace IsValid with IsValidToPush
        if (!q->IsValid()) {
            return false;
        }
    }
    return true;
}

void ProcessQueue::GiveFeedback() {
    for (auto& item : mUpStreamFeedbacks) {
        item->Feedback(mKey);
    }
}

} // namespace logtail
