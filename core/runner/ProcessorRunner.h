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

#include <atomic>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "models/PipelineEventGroup.h"
#include "monitor/LogtailMetric.h"
#include "pipeline/queue/QueueKey.h"

namespace logtail {

class ProcessorRunner {
public:
    ProcessorRunner(const ProcessorRunner&) = delete;
    ProcessorRunner& operator=(const ProcessorRunner&) = delete;

    static ProcessorRunner* GetInstance() {
        static ProcessorRunner instance;
        return &instance;
    }

    void Init();
    void Stop();

    bool PushQueue(QueueKey key, size_t inputIndex, PipelineEventGroup&& group, uint32_t retryTimes = 1);

private:
    ProcessorRunner();
    ~ProcessorRunner() = default;

    void Run(uint32_t threadNo);

    bool Serialize(const PipelineEventGroup& group,
                   bool enableNanosecond,
                   const std::string& logstore,
                   std::string& res,
                   std::string& errorMsg);

    uint32_t mThreadCount = 1;
    std::vector<std::future<void>> mThreadRes;
    std::atomic_bool mIsFlush = false;

    thread_local static MetricsRecordRef sMetricsRecordRef;
    thread_local static CounterPtr sInGroupsCnt;
    thread_local static CounterPtr sInEventsCnt;
    thread_local static CounterPtr sInGroupDataSizeBytes;
    thread_local static IntGaugePtr sLastRunTime;
};

} // namespace logtail
