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
#include <boost/regex.hpp>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/Lock.h"
#include "common/LogRunnable.h"
#include "common/LogstoreFeedbackQueue.h"
#include "common/Thread.h"
#include "log_pb/sls_logs.pb.h"
#include "pipeline/PipelineContext.h"
#include "queue/ProcessQueueItem.h"
#include "reader/LogFileReader.h"

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
    bool PushBuffer(LogstoreFeedBackKey key,
                    const std::string& configName,
                    size_t inputIndex,
                    PipelineEventGroup&& group,
                    uint32_t retryTimes = 1);

    //************************************
    // Method:    IsValidToReadLog
    // FullName:  logtail::LogProcess::IsValidToReadLog
    // Access:    public
    // Returns:   bool true, can read log and push buffer, false cann't read
    // Qualifier:
    // Parameter: const LogstoreFeedBackKey & logstoreKey
    //************************************
    bool IsValidToReadLog(const LogstoreFeedBackKey& logstoreKey);

    void SetFeedBack(LogstoreFeedBackInterface* pInterface);

    // call it after holdon or processor not started
    // must not call this when processer is working
    void SetPriorityWithHoldOn(const LogstoreFeedBackKey& logstoreKey, int32_t priority);

    // call it after holdon or processor not started
    // must not call this when processer is working
    void DeletePriorityWithHoldOn(const LogstoreFeedBackKey& logstoreKey);

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

    LogstoreFeedbackQueue<std::unique_ptr<ProcessQueueItem>>& GetQueue() { return mLogFeedbackQueue; }

private:
    LogProcess();
    ~LogProcess();
    /**
     * @retval false if continue processing by C++, true if processed by Go
     */
    bool ProcessBuffer(const std::shared_ptr<Pipeline>& pipeline,
                       std::vector<PipelineEventGroup>& eventGroupList,
                       std::vector<std::unique_ptr<sls_logs::LogGroup>>& resultGroupList);
    /**
     * @retval 0 if continue processing by C++, 1 if processed by Go
     */
    // int ProcessBufferLegacy(std::shared_ptr<LogBuffer>& logBuffer,
    //                         LogFileReaderPtr& logFileReader,
    //                         sls_logs::LogGroup& logGroup,
    //                         ProcessProfile& profile,
    //                         Config& config);
    void DoFuseHandling();
    // void FillEventGroupMetadata(LogBuffer& logBuffer, PipelineEventGroup& eventGroup) const;
    void FillLogGroupLogs(const PipelineEventGroup& eventGroup,
                          sls_logs::LogGroup& resultGroup,
                          bool enableTimestampNanosecond) const;
    void FillLogGroupTags(const PipelineEventGroup& eventGroup,
                          const std::string& logstore,
                          sls_logs::LogGroup& resultGroup) const;

    bool mInitialized;
    // int mLocalTimeZoneOffsetSecond;
    ThreadPtr* mProcessThreads;
    int32_t mThreadCount;
    LogstoreFeedbackQueue<std::unique_ptr<ProcessQueueItem>> mLogFeedbackQueue;
    std::atomic_bool* mThreadFlags; // whether thread is sending data or wait
    // int32_t mBufferCountLimit;
    ReadWriteLock mAccessProcessThreadRWL;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class EventDispatcherTest;
    friend class LogProcessUnittest;
    friend class FuseFileUnittest;

    void CleanEnviroments();
#endif
};

} // namespace logtail