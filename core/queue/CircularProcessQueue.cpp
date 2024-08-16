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

#include "queue/CircularProcessQueue.h"

#include "logger/Logger.h"
#include "queue/QueueKeyManager.h"

using namespace std;

namespace logtail {

bool CircularProcessQueue::Push(unique_ptr<ProcessQueueItem>&& item) {
    size_t newCnt = item->mEventGroup.GetEvents().size();
    while (!mQueue.empty() && mEventCnt + newCnt > mCapacity) {
        mEventCnt -= mQueue.front()->mEventGroup.GetEvents().size();
        mQueue.pop_front();
    }
    if (mEventCnt + newCnt > mCapacity) {
        return false;
    }
    mQueue.push_back(std::move(item));
    mEventCnt += newCnt;
    return true;
}

bool CircularProcessQueue::Pop(unique_ptr<ProcessQueueItem>& item) {
    if (!IsValidToPop()) {
        return false;
    }
    item = std::move(mQueue.front());
    mQueue.pop_front();
    mEventCnt -= item->mEventGroup.GetEvents().size();
    return true;
}

void CircularProcessQueue::Reset(size_t cap) {
    // it seems more reasonable to retain extra items and process them immediately, however this contray to current framework design
    // so we simply discard extra items, considering that it is a rare case to change capacity
    uint32_t cnt = 0;
    while (!mQueue.empty() && mEventCnt > cap) {
        mEventCnt -= mQueue.front()->mEventGroup.GetEvents().size();
        mQueue.pop_front();
        ++cnt;
    }
    if (cnt > 0) {
        LOG_WARNING(sLogger,
                    ("new circular process queue capacity is smaller than old queue size",
                     "discard old data")("discard cnt", cnt)("config", QueueKeyManager::GetInstance()->GetName(mKey)));
    }
    ProcessQueueInterface::Reset();
    QueueInterface::Reset(cap);
}

} // namespace logtail
