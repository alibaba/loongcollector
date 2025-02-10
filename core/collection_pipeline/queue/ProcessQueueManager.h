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

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "collection_pipeline/queue/BoundedSenderQueueInterface.h"
#include "collection_pipeline/queue/ProcessQueueInterface.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "collection_pipeline/queue/QueueParam.h"
#include "common/FeedbackInterface.h"

namespace logtail {

enum class QueueStatus { OK, QUEUE_FULL, QUEUE_NOT_EXIST };

class ProcessQueueManager : public FeedbackInterface {
public:
    using ProcessQueueIterator = std::list<std::unique_ptr<ProcessQueueInterface>>::iterator;

    enum class QueueType { BOUNDED, CIRCULAR };

    static constexpr uint32_t sMaxPriority = 2;

    ProcessQueueManager(const ProcessQueueManager&) = delete;
    ProcessQueueManager& operator=(const ProcessQueueManager&) = delete;

    static ProcessQueueManager* GetInstance() {
        static ProcessQueueManager instance;
        return &instance;
    }

    void Feedback(QueueKey key) override { Trigger(); }

    bool CreateOrUpdateBoundedQueue(QueueKey key, uint32_t priority, const CollectionPipelineContext& ctx);
    bool
    CreateOrUpdateCircularQueue(QueueKey key, uint32_t priority, size_t capacity, const CollectionPipelineContext& ctx);
    bool DeleteQueue(QueueKey key);
    bool IsValidToPush(QueueKey key) const;
    // 0: success, 1: queue is full, 2: queue not found
    QueueStatus PushQueue(QueueKey key, std::unique_ptr<ProcessQueueItem>&& item);
    bool PopItem(int64_t threadNo, std::unique_ptr<ProcessQueueItem>& item, std::string& configName);
    bool IsAllQueueEmpty() const;
    bool SetDownStreamQueues(QueueKey key, std::vector<BoundedSenderQueueInterface*>&& ques);
    bool SetFeedbackInterface(QueueKey key, std::vector<FeedbackInterface*>&& feedback);
    void DisablePop(const std::string& configName, bool isPipelineRemoving);
    void EnablePop(const std::string& configName);

    bool Wait(uint64_t ms);
    void Trigger();

private:
    ProcessQueueManager();
    ~ProcessQueueManager() = default;

    void CreateBoundedQueue(QueueKey key, uint32_t priority, const CollectionPipelineContext& ctx);
    void CreateCircularQueue(QueueKey key, uint32_t priority, size_t capacity, const CollectionPipelineContext& ctx);
    void AdjustQueuePriority(const ProcessQueueIterator& iter, uint32_t priority);
    void DeleteQueueEntity(const ProcessQueueIterator& iter);
    void ResetCurrentQueueIndex();

    BoundedQueueParam mBoundedQueueParam;

    mutable std::mutex mQueueMux;
    std::unordered_map<QueueKey, std::pair<ProcessQueueIterator, QueueType>> mQueues;
    std::list<std::unique_ptr<ProcessQueueInterface>> mPriorityQueue[sMaxPriority + 1];
    std::pair<uint32_t, ProcessQueueIterator> mCurrentQueueIndex;

    mutable std::mutex mStateMux;
    mutable std::condition_variable mCond;
    bool mValidToPop = false;

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
    friend class ProcessQueueManagerUnittest;
    friend class PipelineUnittest;
    friend class PipelineUpdateUnittest;
    friend class HostMonitorInputRunnerUnittest;
#endif
};

} // namespace logtail
