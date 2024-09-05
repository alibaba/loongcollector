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
#include <vector>

#include "common/FeedbackInterface.h"
#include "pipeline/queue/BoundedQueueInterface.h"
#include "pipeline/queue/ProcessQueueInterface.h"

namespace logtail {

// not thread-safe, should be protected explicitly by queue manager
class BoundedProcessQueue : public BoundedQueueInterface<std::unique_ptr<ProcessQueueItem>>,
                            public ProcessQueueInterface {
public:
    BoundedProcessQueue(size_t cap, size_t low, size_t high, int64_t key, uint32_t priority, const std::string& config)
        : QueueInterface(key, cap),
          BoundedQueueInterface(key, cap, low, high),
          ProcessQueueInterface(key, cap, priority, config) {}

    bool Push(std::unique_ptr<ProcessQueueItem>&& item) override;
    bool Pop(std::unique_ptr<ProcessQueueItem>& item) override;

    void SetUpStreamFeedbacks(std::vector<FeedbackInterface*>&& feedbacks);

private:
    size_t Size() const override { return mQueue.size(); }

    void GiveFeedback() const override;

    std::queue<std::unique_ptr<ProcessQueueItem>> mQueue;
    std::vector<FeedbackInterface*> mUpStreamFeedbacks;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BoundedProcessQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class PipelineUnittest;
#endif
};

} // namespace logtail
