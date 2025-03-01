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

#include "Connection.h"
#include "common/Lock.h"
#include "ebpf/plugin/ProcessCacheManager.h"
extern "C" {
#include <coolbpf/net.h>
};

namespace logtail {
namespace ebpf {

// hold by one thread
class ConnectionManager {
public:
    static std::unique_ptr<ConnectionManager> Create(int64_t it_interval_ms = 200, int64_t report_interval_sec = 15) {
        return std::unique_ptr<ConnectionManager>(new ConnectionManager(it_interval_ms, report_interval_sec));
    }

    ~ConnectionManager() {}

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    const std::shared_ptr<Connection> AcceptNetDataEvent(struct conn_data_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);

    void Iterations(int count);

private:
    ConnectionManager(int64_t itIntervalMs, int64_t reportIntervalSec)
        : mItIntervalMs(itIntervalMs), mReportIntervalSec(reportIntervalSec), mConnectionTotal(0) {}


    const std::shared_ptr<Connection> GetOrCreateConnection(const ConnId&);
    void DeleteConnection(const ConnId&);
    const std::shared_ptr<Connection> GetConnection(const ConnId&);

    int mItIntervalMs;
    int mReportIntervalSec;

    std::atomic_int64_t mConnectionTotal;
    // object pool, used for cache some conn_tracker objects
    // lock used to protect conn_trackers map
    // mutable ReadWriteLock mReadWriteLock;
    std::unordered_map<ConnId, std::shared_ptr<Connection>> mConnections;

    int64_t mLastReportTs = INT64_MIN;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConnectionUnittest;
    friend class ConnectionManagerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
