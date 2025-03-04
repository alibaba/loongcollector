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

#include "ConnectionManager.h"

#include "logger/Logger.h"
#include "metadata/ContainerInfo.h"
#include "metadata/K8sMetadata.h"

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail {
namespace ebpf {

const std::shared_ptr<Connection> ConnectionManager::GetOrCreateConnection(const ConnId& connId) {
    if (!mConnections.count(connId)) {
        mConnectionTotal.fetch_add(1);
        mConnections.insert(std::make_pair(connId, std::make_shared<Connection>(connId)));
    }
    mConnections[connId]->RecordActive();
    return mConnections[connId];
}

const std::shared_ptr<Connection> ConnectionManager::GetConnection(const ConnId& connId) {
    if (mConnections.count(connId)) {
        return mConnections[connId];
    } else {
        return nullptr;
    }
}

void ConnectionManager::DeleteConnection(const ConnId& connId) {
    mConnections.erase(connId);
    mConnectionTotal.fetch_add(-1);
}

void ConnectionManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    // update net stats
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = GetOrCreateConnection(connId);
    if (nullptr == conn) {
        return;
    }

    conn->UpdateConnState(event);
    conn->RecordActive();
}

const std::shared_ptr<Connection> ConnectionManager::AcceptNetDataEvent(struct conn_data_event_t* event) {
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = GetOrCreateConnection(connId);

    if (nullptr == conn) {
        return nullptr;
    }

    conn->SafeUpdateRole(event->role);
    conn->SafeUpdateProtocol(event->protocol);
    conn->RecordActive();
    return conn;
}

void ConnectionManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    if (AF_INET != event->si.family && AF_INET6 != event->si.family)
        return;
    // udpate conn tracker stats
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = GetOrCreateConnection(connId);
    if (conn == nullptr) {
        // log error
        LOG_WARNING(sLogger,
                    ("GetOrCreateConntracker get null. pid",
                     event->conn_id.tgid)("fd", event->conn_id.fd)("start", event->conn_id.start));
        return;
    }

    // update conn tracker stats
    conn->UpdateConnStats(event);
    conn->RecordActive();
}

void ConnectionManager::Iterations(int count) {
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    auto nowTs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    // report every seconds ...
    bool needGenRecord = (nowTs - mLastReportTs > 1);
    LOG_DEBUG(sLogger,
              ("[Iterations] it", count)("conn tracker map size", mConnections.size())("total count",
                                                                                       mConnectionTotal.load()));
    int n = 0;
    std::queue<ConnId> deleteQueue;
    for (const auto& it : mConnections) {
        auto connId = it.first;
        if (!it.second) {
            // should not happen ...
            LOG_WARNING(sLogger, ("no conn tracker??? pid", connId.tgid)("fd", connId.fd)("start", connId.start));
            deleteQueue.push(it.first);
            continue;
        }

        it.second->TryAttachPeerMeta();
        it.second->TryAttachSelfMeta();

        bool forceGenRecord = false;
        if (it.second && it.second->ReadyToDestroy(now)) {
            forceGenRecord = true;
            // push conn stats ...
            deleteQueue.push(it.first);
            n++;
            continue;
        }

        if ((needGenRecord || forceGenRecord) && mEnableConnStats) {
            std::shared_ptr<AbstractRecord> record = std::make_shared<ConnStatsRecord>(it.second);
            LOG_DEBUG(sLogger,
                      ("needGenRecord", needGenRecord)("mEnableConnStats", mEnableConnStats)("forceGenRecord",
                                                                                             forceGenRecord));
            bool res = it.second->GenerateConnStatsRecord(record);
            if (res && mConnStatsHandler)
                mConnStatsHandler(record);
            if (needGenRecord)
                mLastReportTs = nowTs; // update report ts
        }

        // when we query for conn tracker, we record active
        it.second->CountDown();
    }

    // clean conn trackers ...
    while (deleteQueue.size()) {
        auto connId = deleteQueue.front();
        DeleteConnection(connId);
        LOG_DEBUG(sLogger, ("delete conntrackers pid", connId.tgid)("fd", connId.fd)("start", connId.start));
        deleteQueue.pop();
    }

    LOG_DEBUG(sLogger, ("Iterations", count)("remove conntrackers", n)("total conntrackers", mConnectionTotal.load()));
    return;
}

} // namespace ebpf
} // namespace logtail
