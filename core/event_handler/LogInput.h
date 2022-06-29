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

#ifndef __LOG_ILOGTAIL_LOG_INPUT_H__
#define __LOG_ILOGTAIL_LOG_INPUT_H__

#include <string>
#include <queue>
#include <vector>
#include <unordered_set>
#include "common/Lock.h"
#include "common/LogRunnable.h"

namespace logtail {

class Event;
class EventDispatcher;

class LogInput : public LogRunnable {
public:
    static LogInput* GetInstance() {
        static LogInput* ptr = new LogInput();
        return ptr;
    }

    void Resume() {} // TODO: Refine interface to avoid ambiguous.
    void Resume(bool addCheckPointEventFlag = false);
    void Start();
    void HoldOn();
    void PushEventQueue(std::vector<Event*>& eventVec);
    void PushEventQueue(Event* ev);
    void TryReadEvents(bool forceRead);
    void FlowControl();
    bool IsInterupt() { return mInteruptFlag; }

    /**
     * @brief read local event data
     * @return
     */
    bool ReadLocalEvents();

    void SetForceClearFlag(bool flag) { mForceClearFlag = flag; }

    bool GetForceClearFlag() const { return mForceClearFlag; }

    int32_t GetLastReadEventTime() { return mLastReadEventTime; }

private:
    LogInput();
    ~LogInput();
    void* ProcessLoop();
    void ProcessEvent(EventDispatcher* dispatcher, Event* ev);
    Event* PopEventQueue();
    void CheckAndUpdateCriticalMetric(int32_t curTime);

    std::queue<Event*> mInotifyEventQueue;
    std::unordered_set<int64_t> mModifyEventSet;
    ReadWriteLock mAccessMainThreadRWL;
    int32_t mCheckBaseDirInterval;
    int32_t mCheckSymbolicLinkInterval;
    int64_t mLastReadEventMicroSeconds;
    volatile bool mInteruptFlag;
    volatile bool mForceClearFlag;
    volatile bool mIdleFlag;
    int32_t mEventProcessCount;
    int32_t mLastUpdateMetricTime;

    std::atomic_int mLastReadEventTime{0};

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogInputUnittest;
    friend class EventDispatcherTest;
    friend class ConfigUpdatorUnittest;
    friend class SymlinkInotifyTest;
    friend class SenderUnittest;
    friend class FuxiSceneUnittest;
    friend class ConfigMatchUnittest;
    friend class FuseFileUnittest;

    void CleanEnviroments();
#endif
};

} // namespace logtail
#endif //__LOG_ILOGTAIL_LOG_INPUT_H__