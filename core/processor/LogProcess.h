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
#include <boost/regex.hpp>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include "common/LogstoreFeedbackQueue.h"
#include "common/LogRunnable.h"
#include "common/Thread.h"
#include "common/Lock.h"
#include "log_pb/sls_logs.pb.h"

namespace logtail {
// forward declaration
struct LogBuffer;
class Config;

class LogProcess : public LogRunnable {
public:
    static LogProcess* GetInstance() {
        static LogProcess* ptr = new LogProcess();
        return ptr;
    }
    void Start();
    void* ProcessLoop(int32_t threadNo);
    bool PushBuffer(LogBuffer* logBuffer, int32_t retryTimes = 1);

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

    LogstoreFeedbackQueue<LogBuffer*>& GetQueue() { return mLogFeedbackQueue; }

private:
    LogProcess();
    ~LogProcess();

    void DoFuseHandling();

    bool mInitialized;
    ThreadPtr* mProcessThreads;
    int32_t mThreadCount;
    LogstoreFeedbackQueue<LogBuffer*> mLogFeedbackQueue;
    volatile bool* mThreadFlags; //whether thread is sending data or wait
    //int32_t mBufferCountLimit;
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