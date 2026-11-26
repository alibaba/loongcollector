// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <string>

#include "file_server/event_listener/EventListener.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "monitor/metric_models/MetricRecord.h"

namespace logtail {

// WatchManager is a dedicated runner that centralizes access to the underlying
// EventListener (inotify on Linux). Callers acquire and release directory
// watches through it instead of touching EventListener directly, which gives us
// a single place to observe and, later, to manage watch lifecycle.
//
// This is the W0 skeleton: AcquireWatch/ReleaseWatch forward one-to-one to
// EventListener::AddWatch/RemoveWatch with no added dedup, ref-counting or
// lifecycle change (pass-through). Only a runner_name = "watch_manager"
// MetricsRecordRef is registered to establish an observability baseline.
class WatchManager {
public:
    static WatchManager* GetInstance() {
        static WatchManager* ptr = new WatchManager();
        return ptr;
    }

    virtual ~WatchManager() = default;

    WatchManager(const WatchManager&) = delete;
    WatchManager& operator=(const WatchManager&) = delete;

    // Register a watch on path. Returns the underlying watch descriptor
    // (identical to what EventListener::AddWatch would return, including the
    // invalid id on failure).
    int AcquireWatch(const std::string& path);

    // Remove the watch identified by wd. Returns whether the underlying
    // EventListener::RemoveWatch succeeded.
    bool ReleaseWatch(int wd);

    MetricsRecordRef& GetMetricsRecordRef() { return mMetricsRecordRef; }

protected:
    WatchManager();

    // Underlying watch syscalls, forwarded to EventListener by default. Kept
    // virtual so tests can substitute a counting fake and assert pass-through
    // equivalence without depending on a live inotify instance.
    virtual int DoAddWatch(const char* path);
    virtual bool DoRemoveWatch(int wd);

private:
    EventListener* mEventListener = nullptr;
    mutable MetricsRecordRef mMetricsRecordRef;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class WatchManagerUnittest;
#endif
};

} // namespace logtail
