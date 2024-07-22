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
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "common/FeedbackInterface.h"
#include "queue/FeedbackQueue.h"
#include "queue/ProcessQueueItem.h"
#include "queue/SenderQueueInterface.h"

namespace logtail {

// not thread-safe, should be protected explicitly by queue manager
class ProcessQueue : public FeedbackQueue<std::unique_ptr<ProcessQueueItem>> {
public:
    ProcessQueue(size_t cap, size_t low, size_t high, int64_t key, uint32_t priority, const std::string& config)
        : FeedbackQueue<std::unique_ptr<ProcessQueueItem>>(key, cap, low, high),
          mPriority(priority),
          mConfigName(config) {}

    bool Push(std::unique_ptr<ProcessQueueItem>&& item) override;
    bool Pop(std::unique_ptr<ProcessQueueItem>& item) override;

    void SetPriority(uint32_t priority) { mPriority = priority; }
    uint32_t GetPriority() const { return mPriority; }

    void SetConfigName(const std::string& config) { mConfigName = config; }
    const std::string& GetConfigName() const { return mConfigName; }

    void SetDownStreamQueues(std::vector<SenderQueueInterface*>&& ques);
    void SetUpStreamFeedbacks(std::vector<FeedbackInterface*>&& feedbacks);

    void Reset(size_t cap, size_t low, size_t high);

private:
    size_t Size() const override { return mQueue.size(); }

    void GiveFeedback() const;
    bool IsDownStreamQueuesValidToPush() const override;

    std::queue<std::unique_ptr<ProcessQueueItem>> mQueue;
    uint32_t mPriority;
    std::string mConfigName;

    std::vector<SenderQueueInterface*> mDownStreamQueues;
    std::vector<FeedbackInterface*> mUpStreamFeedbacks;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class PipelineUnittest;
#endif
};

} // namespace logtail
