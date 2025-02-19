// Copyright 2023 iLogtail Authors
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

#include <chrono>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "ConnTracker.h"
#include "Worker.h"
#include "common/Lock.h"
#include "ebpf/plugin/ProcessCacheManager.h"
extern "C" {
#include <net.h>
};

namespace logtail {
namespace ebpf {

class ConnTrackerManager {
public:
    static std::shared_ptr<ConnTrackerManager> Create(int64_t it_interval_ms = 200, int64_t report_interval_sec = 15) {
        return std::shared_ptr<ConnTrackerManager>(new ConnTrackerManager(it_interval_ms, report_interval_sec));
    }

    ~ConnTrackerManager();

    std::shared_ptr<ConnTracker> GetOrCreateConntracker(const ConnId&);
    const std::shared_ptr<ConnTracker> GetConntracker(const ConnId&);
    void DeleteConnTracker(const ConnId&);

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    void AcceptNetDataEvent(struct conn_data_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);

    std::string GetAggregateKey(ConnId&);
    std::array<std::string, kConnTrackerElementsTableSize> GetConnTrackerAttrs(const ConnId&);

    /**
     * Iterations iterates per `interval_ms`
     *   this function is used to remove closed conn tracers ...
     */
    void IterationsInternal(int count_);

    /**
     * Iterations iterates per `interval_sec`
     *   this function is used to report conn_stats events ...
     */
    void Report();

    void Stop();

    void Start();

    enum class AttachStatus {
        WAIT_FOR_EVENT,
        SUCCESS,
        NEED_REMOTE,
    };
    AttachStatus TryAttachPodMeta(std::shared_ptr<ConnTracker> ct);
    AttachStatus TryAttachPeerPodMeta(std::shared_ptr<ConnTracker> ct);

    //  void update_conn_stats();

    //  void export_record();

private:
    ConnTrackerManager(int64_t it_interval_ms, int64_t report_interval_sec)
        : it_interval_ms_(it_interval_ms), report_interval_sec_(report_interval_sec), conn_tracker_total_(0) {}

    volatile bool flag_;
    int64_t it_interval_ms_;
    int64_t report_interval_sec_;
    std::thread scheduler_thread_;
    std::thread reporter_thread_;

    std::queue<ConnId> delete_queue_;

    std::atomic_int64_t conn_tracker_total_;
    // object pool, used for cache some conn_tracker objects
    // lock used to protect conn_trackers map
    //  std::mutex mutex_;
    mutable ReadWriteLock mReadWriteLock;
    std::unordered_map<ConnId, std::shared_ptr<ConnTracker>> conn_trackers_;

    int64_t last_report_ts_ = INT64_MIN;

    std::unique_ptr<WorkerPool<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>>> mWorkerPool;
};

} // namespace ebpf
} // namespace logtail
