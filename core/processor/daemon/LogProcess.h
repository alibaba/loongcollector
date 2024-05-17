/*
 * Copyright 2022 iLogtail Authors
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
#include <memory>
#include <string>
#include <vector>

#include "common/Lock.h"
#include "common/LogRunnable.h"
#include "common/Thread.h"
#include "log_pb/sls_logs.pb.h"
#include "queue/FeedbackQueueKey.h"
#include "pipeline/Pipeline.h"

namespace logtail {
// forward declaration
class PipelineEventGroup;

class LogProcess : public LogRunnable {
public:
    static LogProcess* GetInstance() {
        static LogProcess* ptr = new LogProcess();
        return ptr;
    }
    void Start();
    void* ProcessLoop(int32_t threadNo);
    // TODO: replace key with configName
    bool PushBuffer(QueueKey key,
                    size_t inputIndex,
                    PipelineEventGroup&& group,
                    uint32_t retryTimes = 1);

    // process thread hold on should after input thread hold on
    // because process hold on will lock mLogFeedbackQueue, if input thread not hold on first,
    // input thread may try to lock mLogFeedbackQueue by call IsValidToReadLog or PushBuffer,
    // this will cause deadlock
    void HoldOn();

    void Resume();

    //
    //************************************
    // Method:    FlushOut
    // FullName:  logtail::LogProcess::FlushOut
    // Access:    public
    // Returns:   bool
    // Qualifier:
    // Parameter: int32_t waitMs
    //************************************
    bool FlushOut(int32_t waitMs);

private:
    LogProcess();
    ~LogProcess();
    /**
     * @retval false if continue processing by C++, true if processed by Go
     */
    bool ProcessBuffer(const std::shared_ptr<Pipeline>& pipeline,
                       std::vector<PipelineEventGroup>& eventGroupList,
                       std::vector<std::unique_ptr<sls_logs::LogGroup>>& resultGroupList);
    void DoFuseHandling();
    void FillLogGroupLogs(const PipelineEventGroup& eventGroup,
                          sls_logs::LogGroup& resultGroup,
                          bool enableTimestampNanosecond) const;
    void FillLogGroupTags(const PipelineEventGroup& eventGroup,
                          const std::string& logstore,
                          sls_logs::LogGroup& resultGroup) const;

    bool mInitialized = false;
    ThreadPtr* mProcessThreads;
    int32_t mThreadCount = 1;
    std::atomic_bool* mThreadFlags; // whether thread is sending data or wait
    ReadWriteLock mAccessProcessThreadRWL;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class EventDispatcherTest;
    friend class LogProcessUnittest;
#endif
};

} // namespace logtail