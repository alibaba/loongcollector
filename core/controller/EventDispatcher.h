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
#include <sys/types.h>
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <stddef.h>
#include <time.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "profiler/LogFileProfiler.h"
#include "polling/PollingModify.h"
#include "polling/PollingDirFile.h"
#include "event_listener/EventListener.h"
#include "checkpoint/CheckPointManager.h"
#include "EventDispatcherBase.h"

namespace logtail {

class Config;
class TimeoutHandler;
class EventHandler;
class Event;

class EventDispatcher : public EventDispatcherBase {
public:
    static EventDispatcher* GetInstance() {
        static EventDispatcher* ptr = new EventDispatcher();
        return ptr;
    }

#if defined(_MSC_VER)
    void InitWindowsSignalObject() override;
    void SyncWindowsSignalObject() override;
    void ReleaseWindowsSignalObject() override;
#endif
#if defined(__linux__)
    void InitShennong() override;
    void CheckShennong() override;
#endif
    void ExtraWork() override;

    /** Enter the event loop, dispatch to the approiate handler when an event occurs.
     * Propagate timeout.
     *
     * @return true on success; false on failure
     */
    bool IsInterupt() override;

private:
    EventDispatcher();
    ~EventDispatcher();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcherTest;
    friend class ConfigUpdatorUnittest;
    friend class ConfigMatchUnittest;
    friend class FuxiSceneUnittest;
    friend class LogtailWriterTest;
    friend class SymlinkInotifyTest;
    friend class SenderUnittest;
    friend class FuseFileUnittest;
    friend class MultiServerConfigUpdatorUnitest;
#endif
};

} //namespace logtail
