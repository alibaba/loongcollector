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

std::unique_ptr<NetDataEvent> ConnectionManager::AcceptNetDataEvent(struct conn_data_event_t* event) {
    ConnId connId = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn = GetOrCreateConnection(connId);

    if (nullptr == conn) {
        return nullptr;
    }

    conn->SafeUpdateRole(event->role);
    conn->SafeUpdateProtocol(ProtocolType(event->protocol));
    conn->RecordActive();

    // will do deepcopy
    auto res = std::make_unique<NetDataEvent>(event);
    res->mConnection = conn;
    return res;
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
    // auto nowTs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    LOG_DEBUG(sLogger,
              ("[Iterations] it", count)("conn tracker map size", mConnections.size())("total count",
                                                                                       mConnectionTotal.load()));
    int n = 0;
    std::queue<ConnId> deleteQueue;
    for (const auto& it : mConnections) {
        auto connId = it.first;
        if (!it.second) {
            LOG_WARNING(sLogger, ("no conn tracker??? pid", connId.tgid)("fd", connId.fd)("start", connId.start));
            deleteQueue.push(it.first);
            continue;
        }
        // LOG(INFO) << "Iterations:" << count << " conntracker pid:" << conn_id.tgid << " fd:" << conn_id.fd << "
        // start:" << conn_id.start
        //     << " is_close:" << it.second->IsClose() << " epoch:" << it.second->GetEpoch() << " readyToDestroy:"
        //     << it.second->ReadyToDestroy(now) << " now:" << now_ts << " last_active:" << it.second->GetLastActiveTs()
        //     << " last_update:" << it.second->GetLastUpdateTs()
        //     << " dip:" << it.second->dip_ << " daddr:" << it.second->daddr_
        //     << " containerid:" << it.second->container_id_trim_
        //     << " k8s_meta_attached_: " << it.second->k8s_meta_attached_ << " k8s_peer_meta_attached_: "<<
        //     it.second->k8s_peer_meta_attached_
        //     << " protocol_set_: " << it.second->protocol_set_ << " net_meta_attached_: "<<
        //     it.second->net_meta_attached_;

        if (it.second && it.second->ReadyToDestroy(now)) {
            deleteQueue.push(it.first);
            n++;
            continue;
        }

        it.second->TryAttachPeerMeta();
        it.second->TryAttachSelfMeta();

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
