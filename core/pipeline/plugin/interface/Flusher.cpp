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

#include "pipeline/plugin/interface/Flusher.h"

#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SenderQueueManager.h"
// TODO: temporarily used here
#include "pipeline/PipelineManager.h"

using namespace std;

namespace logtail {

bool Flusher::Start() {
    SenderQueueManager::GetInstance()->ReuseQueue(mQueueKey);
    return true;
}

bool Flusher::Stop(bool isPipelineRemoving) {
    SenderQueueManager::GetInstance()->DeleteQueue(mQueueKey);
    return true;
}

void Flusher::GenerateQueueKey(const std::string& target) {
    mQueueKey = QueueKeyManager::GetInstance()->GetKey((HasContext() ? mContext->GetConfigName() : "") + "-" + Name()
                                                       + "-" + target);
}

bool Flusher::PushToQueue(unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes) {
#ifndef APSARA_UNIT_TEST_MAIN
    // TODO: temporarily set here, should be removed after independent config update refactor
    if (item->mFlusher->HasContext()) {
        item->mPipeline
            = PipelineManager::GetInstance()->FindConfigByName(item->mFlusher->GetContext().GetConfigName());
        if (!item->mPipeline) {
            // should not happen
            return false;
        }
    }
#endif

    const string& str = QueueKeyManager::GetInstance()->GetName(item->mQueueKey);
    for (size_t i = 0; i < retryTimes; ++i) {
        int rst = SenderQueueManager::GetInstance()->PushQueue(item->mQueueKey, std::move(item));
        if (rst == 0) {
            return true;
        }
        if (rst == 2) {
            // should not happen
            LOG_ERROR(sLogger,
                      ("failed to push data to sender queue",
                       "queue not found")("action", "discard data")("config-flusher-dst", str));
            LogtailAlarm::GetInstance()->SendAlarm(
                DISCARD_DATA_ALARM,
                "failed to push data to sender queue: queue not found\taction: discard data\tconfig-flusher-dst" + str);
            return false;
        }
        if (i % 100 == 0) {
            LOG_WARNING(sLogger,
                        ("push attempts to sender queue continuously failed for the past second",
                         "retry again")("config-flusher-dst", str));
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    LOG_WARNING(
        sLogger,
        ("failed to push data to sender queue", "queue full")("action", "discard data")("config-flusher-dst", str));
    LogtailAlarm::GetInstance()->SendAlarm(
        DISCARD_DATA_ALARM,
        "failed to push data to sender queue: queue full\taction: discard data\tconfig-flusher-dst" + str);
    return false;
}

void Flusher::DealSenderQueueItemAfterSend(SenderQueueItem* item, bool keep) {
    if (keep) {
        item->mStatus = SendingStatus::IDLE;
        ++item->mTryCnt;
    } else {
        // TODO: because current profile has a dummy flusher, we have to use item->mQueueKey here
        SenderQueueManager::GetInstance()->RemoveItem(item->mQueueKey, item);
    }
}

} // namespace logtail
