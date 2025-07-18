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

#include "ebpf/plugin/network_observer/ConnectionManager.h"

#include "TimeKeeper.h"
#include "logger/Logger.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail::ebpf {

std::shared_ptr<Connection> ConnectionManager::getOrCreateConnection(const ConnId& connId) {
    if (mConnections.size() >= static_cast<size_t>(mMaxConnections.load())) {
        // max connections exceeded ...
        LOG_DEBUG(sLogger, ("max connection limit exceeded!", ""));
        return nullptr;
    }

    auto it = mConnections.find(connId);
    if (it != mConnections.end()) {
        return it->second;
    }

    mConnectionTotal.fetch_add(1);

    std::shared_ptr<Connection> conn = std::make_shared<Connection>(connId);
    conn->RecordActive();
    mConnections.insert({connId, conn});
    return conn;
}

std::shared_ptr<Connection> ConnectionManager::getConnection(const ConnId& connId) {
    auto it = mConnections.find(connId);
    if (it != mConnections.end()) {
        return it->second;
    }
    return nullptr;
}

void ConnectionManager::deleteConnection(const ConnId& connId) {
    mConnections.erase(connId);
    mConnectionTotal.fetch_add(-1);
}

void ConnectionManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    // update net stats
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = getOrCreateConnection(connId);
    if (nullptr == conn) {
        return;
    }

    bool isClose = false;
    conn->UpdateConnState(event, isClose);
    if (isClose) {
        mClosedConnections[0].push_back(connId);
    }
    conn->RecordActive();
}

std::shared_ptr<Connection> ConnectionManager::AcceptNetDataEvent(struct conn_data_event_t* event) {
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = getOrCreateConnection(connId);

    if (nullptr == conn) {
        return nullptr;
    }

    // TryAttachL7
    conn->TryAttachL7Meta(event->role, event->protocol);
    conn->RecordActive();
    return conn;
}

void ConnectionManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    if (AF_INET != event->si.family && AF_INET6 != event->si.family) {
        return;
    }
    // udpate conn tracker stats
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = getOrCreateConnection(connId);
    if (conn == nullptr) {
        // log error
        LOG_DEBUG(sLogger,
                  ("GetOrCreateConntracker get null. pid",
                   event->conn_id.tgid)("fd", event->conn_id.fd)("start", event->conn_id.start));
        return;
    }

    // update conn tracker stats
    conn->UpdateConnStats(event);
    conn->RecordActive();
}

void ConnectionManager::cleanClosedConnections() {
    for (const auto& connId : mClosedConnections[kConnectionEpoch - 1]) {
        const auto& it = mConnections.find(connId);
        if (it == mConnections.end()) {
            // connection is already removed
            continue;
        }
        deleteConnection(connId);
        LOG_DEBUG(sLogger,
                  ("delete connections caused by close, pid", connId.tgid)("fd", connId.fd)("start", connId.start));
    }

    for (size_t i = kConnectionEpoch - 1; i >= 1; i--) {
        std::swap(mClosedConnections[i], mClosedConnections[i - 1]);
    }

    mClosedConnections[0].clear();
}

void ConnectionManager::Iterations() {
    cleanClosedConnections();
    auto nowMs = TimeKeeper::GetInstance()->NowMs();
    if (nowMs - mLastGcTimeMs < mGcIntervalMs) {
        return;
    }
    mLastGcTimeMs = nowMs;

    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    LOG_DEBUG(sLogger,
              ("[Iterations] conn tracker map size", mConnections.size())("total count", mConnectionTotal.load()));
    int n = 0;
    std::vector<ConnId> deleteQueue;
    for (const auto& it : mConnections) {
        auto connId = it.first;
        auto connection = it.second;
        if (!connection) {
            // should not happen ...
            LOG_WARNING(sLogger, ("no conn tracker??? pid", connId.tgid)("fd", connId.fd)("start", connId.start));
            deleteQueue.push_back(it.first);
            continue;
        }

        connection->TryAttachPeerMeta();
        connection->TryAttachSelfMeta();

        if (connection && connection->ReadyToDestroy(now)) {
            // push conn stats ...
            deleteQueue.push_back(it.first);
            connection->MarkConnDeleted();
            n++;
            continue;
        }

        // when we query for conn tracker, we record active
        connection->CountDown();
    }

    // clean conn trackers ...
    for (const auto& connId : deleteQueue) {
        deleteConnection(connId);
        LOG_DEBUG(sLogger, ("delete conntrackers pid", connId.tgid)("fd", connId.fd)("start", connId.start));
    }

    LOG_DEBUG(sLogger, ("[Iterations] remove conntrackers", n)("total conntrackers", mConnectionTotal.load()));
}

} // namespace logtail::ebpf
