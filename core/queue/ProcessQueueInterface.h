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
#include <string>
#include <vector>

#include "queue/ProcessQueueItem.h"
#include "queue/QueueInterface.h"

namespace logtail {

class BoundedSenderQueueInterface;

// not thread-safe, should be protected explicitly by queue manager
class ProcessQueueInterface : virtual public QueueInterface<std::unique_ptr<ProcessQueueItem>> {
public:
    ProcessQueueInterface(int64_t key, size_t cap, uint32_t priority, const std::string& config)
        : QueueInterface(key, cap), mPriority(priority), mConfigName(config) {}
    virtual ~ProcessQueueInterface() = default;

    void SetPriority(uint32_t priority) { mPriority = priority; }
    uint32_t GetPriority() const { return mPriority; }

    void SetConfigName(const std::string& config) { mConfigName = config; }
    const std::string& GetConfigName() const { return mConfigName; }

    void SetDownStreamQueues(std::vector<BoundedSenderQueueInterface*>&& ques);

    void InvalidatePop() { mValidToPop = false; }
    void ValidatePop() { mValidToPop = true; }

    void Reset() {
        mDownStreamQueues.clear();
    }

protected:
    bool IsValidToPop() const;

private:
    bool IsDownStreamQueuesValidToPush() const;

    uint32_t mPriority;
    std::string mConfigName;

    std::vector<BoundedSenderQueueInterface*> mDownStreamQueues;
    bool mValidToPop = true;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BoundedProcessQueueUnittest;
    friend class CircularProcessQueueUnittest;
    friend class ExactlyOnceSenderQueueUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class PipelineUnittest;
#endif
};

} // namespace logtail
