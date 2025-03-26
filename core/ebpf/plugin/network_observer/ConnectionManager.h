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

#include <atomic>
#include <chrono>
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
    static std::unique_ptr<ConnectionManager> Create(int maxConnections = 5000, bool enableMetadata = true) {
        return std::unique_ptr<ConnectionManager>(new ConnectionManager(maxConnections, enableMetadata));
    }

    using ConnStatsHandler = std::function<void(const std::shared_ptr<AbstractRecord>& record)>;

    ~ConnectionManager() {}

    void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
    const std::shared_ptr<Connection> AcceptNetDataEvent(struct conn_data_event_t* event);
    void AcceptNetStatsEvent(struct conn_stats_event_t* event);

    void Iterations(int count);

    void SetConnStatsStatus(bool enable) { mEnableConnStats = enable; }

    void RegisterConnStatsFunc(ConnStatsHandler fn) { mConnStatsHandler = fn; }

    int64_t ConnectionTotal() const { return mConnectionTotal.load(); }
    void UpdateMaxConnectionThreshold(int max) { mMaxConnections = max; }
    void SetMetadataEnableStatus(bool enable) { mEnableMetadata = enable; }

private:
    ConnectionManager(int maxConnections, bool enableMetadata)
        : mMaxConnections(maxConnections), mEnableMetadata(enableMetadata), mConnectionTotal(0) {}

    std::shared_ptr<Connection> GetOrCreateConnection(const ConnId&);
    void DeleteConnection(const ConnId&);
    std::shared_ptr<Connection> GetConnection(const ConnId&);

    int mItIntervalMs;
    int mReportIntervalSec;

    std::atomic_int mMaxConnections;
    std::atomic_bool mEnableMetadata;

    std::atomic_bool mEnableConnStats = false;
    ConnStatsHandler mConnStatsHandler = nullptr;

    std::atomic_int64_t mConnectionTotal;
    // object pool, used for cache some conn_tracker objects
    // lock used to protect conn_trackers map
    // mutable ReadWriteLock mReadWriteLock;
    std::unordered_map<ConnId, std::shared_ptr<Connection>> mConnections;

    int64_t mLastReportTs = -1;
    friend class NetworkObserverManager;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConnectionUnittest;
    friend class ConnectionManagerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
