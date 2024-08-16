/*
 * Copyright 2024 iLogtail Authors
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

#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "checkpoint/RangeCheckpoint.h"
#include "common/FeedbackInterface.h"
#include "queue/BoundedProcessQueue.h"
#include "queue/ExactlyOnceSenderQueue.h"
#include "queue/ProcessQueueItem.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKey.h"
#include "queue/QueueParam.h"
#include "queue/SenderQueueItem.h"

namespace logtail {

class ExactlyOnceQueueManager {
    friend class ProcessQueueManager;

public:
    ExactlyOnceQueueManager(const ExactlyOnceQueueManager&) = delete;
    ExactlyOnceQueueManager& operator=(const ExactlyOnceQueueManager&) = delete;

    static ExactlyOnceQueueManager* GetInstance() {
        static ExactlyOnceQueueManager instance;
        return &instance;
    }

    bool CreateOrUpdateQueue(QueueKey key,
                             uint32_t priority,
                             const std::string& config,
                             const std::vector<RangeCheckpointPtr>& checkpoints);
    bool DeleteQueue(QueueKey key);

    bool IsValidToPushProcessQueue(QueueKey key) const;
    // 0: success, 1: queue is full, 2: queue not found
    int PushProcessQueue(QueueKey key, std::unique_ptr<ProcessQueueItem>&& item);
    bool IsAllProcessQueueEmpty() const;
    void InvalidatePopProcessQueue(const std::string& configName);
    void ValidatePopProcessQueue(const std::string& configName);

    // 0: success, 1: queue is full, 2: queue not found
    int PushSenderQueue(QueueKey key, std::unique_ptr<SenderQueueItem>&& item);
    void GetAllAvailableSenderQueueItems(std::vector<SenderQueueItem*>& item, bool withLimits = true);
    bool RemoveSenderQueueItem(QueueKey key, SenderQueueItem* item);
    bool IsAllSenderQueueEmpty() const;

    void ClearTimeoutQueues();

    // TODO: should be removed when self-telemetry is refactored
    uint32_t GetInvalidProcessQueueCnt() const;
    uint32_t GetProcessQueueCnt() const;

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    ExactlyOnceQueueManager();
    ~ExactlyOnceQueueManager() = default;

    BoundedQueueParam mProcessQueueParam;

    mutable std::mutex mProcessQueueMux;
    std::unordered_map<QueueKey, std::list<BoundedProcessQueue>::iterator> mProcessQueues;
    std::list<BoundedProcessQueue> mProcessPriorityQueue[ProcessQueueManager::sMaxPriority + 1];

    mutable std::mutex mSenderQueueMux;
    std::unordered_map<QueueKey, ExactlyOnceSenderQueue> mSenderQueues;

    mutable std::mutex mGCMux;
    std::unordered_map<QueueKey, time_t> mQueueDeletionTimeMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ExactlyOnceQueueManagerUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class SenderQueueManagerUnittest;
#endif
};

} // namespace logtail
