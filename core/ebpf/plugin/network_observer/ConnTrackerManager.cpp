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

#include "ConnTrackerManager.h"

#include "logger/Logger.h"
#include "metadata/ContainerInfo.h"
#include "metadata/K8sMetadata.h"

extern "C" {
#include <net.h>
}

namespace logtail {
namespace ebpf {

const std::shared_ptr<ConnTracker> ConnTrackerManager::GetConntracker(const ConnId& conn_id) {
    {
        ReadLock lock(mReadWriteLock);
        auto it = conn_trackers_.find(conn_id);
        if (it == conn_trackers_.end()) {
            return nullptr;
        } else {
            it->second->RecordActive();
            return it->second;
        }
    }
}

std::shared_ptr<ConnTracker> ConnTrackerManager::GetOrCreateConntracker(const ConnId& conn_id) {
    LOG_DEBUG(sLogger, ("[ConnTrackerManager][GetOrCreateConntracker] begin to lock ...", ""));
    {
        ReadLock lock(mReadWriteLock);
        if (conn_trackers_.count(conn_id)) {
            conn_trackers_[conn_id]->RecordActive();
            return conn_trackers_[conn_id];
        }
    }

    {
        WriteLock lock(mReadWriteLock);
        if (conn_trackers_.count(conn_id)) {
            conn_trackers_[conn_id]->RecordActive();
            return conn_trackers_[conn_id];
        }
        conn_tracker_total_.fetch_add(1);
        conn_trackers_.insert(std::make_pair(conn_id, std::make_shared<ConnTracker>(conn_id)));
        return conn_trackers_[conn_id];
    }
}

void ConnTrackerManager::DeleteConnTracker(const ConnId& conn_id) {
    {
        WriteLock lock(mReadWriteLock);
        conn_trackers_.erase(conn_id);
    }
    conn_tracker_total_.fetch_add(-1);
}

void ConnTrackerManager::AcceptNetDataEvent(struct conn_data_event_t* event) {
    // Nothing to do
}

void ConnTrackerManager::AcceptNetStatsEvent(struct conn_stats_event_t* event) {
    if (AF_INET != event->si.family && AF_INET6 != event->si.family)
        return;
    // udpate conn tracker stats
    ConnId conn_id = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn_tracker = GetOrCreateConntracker(conn_id);
    if (conn_tracker == nullptr) {
        // log error
        LOG_WARNING(sLogger,
                    ("GetOrCreateConntracker get null. pid",
                     event->conn_id.tgid)("fd", event->conn_id.fd)("start", event->conn_id.start));
        return;
    }

    // update conn tracker stats
    conn_tracker->UpdateConnStats(event);
    conn_tracker->RecordActive();
    TryAttachPeerPodMeta(conn_tracker);
    TryAttachPodMeta(conn_tracker);
}


ConnTrackerManager::AttachStatus ConnTrackerManager::TryAttachPodMeta(std::shared_ptr<ConnTracker> ct) {
    if (ct->k8s_meta_attached_)
        return ConnTrackerManager::AttachStatus::SUCCESS;
    if (!ct->net_meta_attached_)
        return ConnTrackerManager::AttachStatus::WAIT_FOR_EVENT;
    if (ct->container_id_trim_.empty()) {
        ct->UpdateSelfPodMetaForUnknown();
        return ConnTrackerManager::AttachStatus::SUCCESS;
    }

    //
    if (!K8sMetadata::GetInstance().Enable()) {
        ct->UpdateSelfPodMetaForUnknown();
        return ConnTrackerManager::AttachStatus::SUCCESS;
    }

    auto meta = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(ct->container_id_trim_);

    if (meta) {
        ct->UpdateSelfPodMeta(meta);
        return ConnTrackerManager::AttachStatus::SUCCESS;
    }
    return ConnTrackerManager::AttachStatus::NEED_REMOTE;
}

ConnTrackerManager::AttachStatus ConnTrackerManager::TryAttachPeerPodMeta(std::shared_ptr<ConnTracker> ct) {
    if (ct->k8s_peer_meta_attached_)
        return ConnTrackerManager::AttachStatus::SUCCESS;
    if (!ct->net_meta_attached_)
        return ConnTrackerManager::AttachStatus::WAIT_FOR_EVENT;
    if (ct->dip_ == "127.0.0.1" || ct->dip_ == "localhost" || ct->dip_ == "0.0.0.0") {
        ct->UpdatePeerPodMetaForLocalhost();
        // auto conn_id = ct->GetConnId();
        // LOG(INFO) << "attached peer localhost for conntracker===>"
        //   << " pid:" << conn_id.tgid << " fd:" << conn_id.fd << " start:" << conn_id.start << " dip:" << ct->dip_ <<
        //   " daddr:" << ct->daddr_ << " query dip:" << ct->dip_;
    }

    if (!K8sMetadata::GetInstance().Enable()) {
        ct->UpdatePeerPodMetaForExternal();
        return ConnTrackerManager::AttachStatus::SUCCESS;
    }

    auto meta = K8sMetadata::GetInstance().GetInfoByIpFromCache(ct->dip_);
    if (meta) {
        ct->UpdatePeerPodMeta(meta);
        // auto conn_id = ct->GetConnId();
        // LOG(INFO) << "attached peer podmeta for conntracker===>"
        //   << " pid:" << conn_id.tgid << " fd:" << conn_id.fd << " start:" << conn_id.start << " dip:" << ct->dip_ <<
        //   " daddr:" << ct->daddr_ << " query dip:" << ct->dip_;
        return ConnTrackerManager::AttachStatus::SUCCESS;
    }
    return ConnTrackerManager::AttachStatus::NEED_REMOTE;
}

std::array<std::string, kConnTrackerElementsTableSize> ConnTrackerManager::GetConnTrackerAttrs(const ConnId& conn_id) {
    auto tracker = GetConntracker(conn_id);
    if (tracker == nullptr) {
        LOG_WARNING(sLogger, ("cannot find conn_tracker, pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
        return std::array<std::string, kConnTrackerElementsTableSize>();
    } else {
        LOG_DEBUG(sLogger,
                  ("successfully find conn_tracker, pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
    }

    return tracker->GetConnTrackerAttrs();
}

void ConnTrackerManager::AcceptNetCtrlEvent(struct conn_ctrl_event_t* event) {
    // update net stats
    ConnId conn_id = ConnId(event->conn_id.fd, event->conn_id.tgid, event->conn_id.start);
    auto conn_tracker = GetOrCreateConntracker(conn_id);
    if (nullptr == conn_tracker) {
        return;
    }

    // {
    //   std::lock_guard<std::mutex> lock(mtx_);
    //   delete_queue_.push(conn_id);
    // }
    // update conn tracker lifecycle
    conn_tracker->UpdateConnState(event);
    conn_tracker->RecordActive();
}

void ConnTrackerManager::IterationsInternal(int count_) {
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    auto now_ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    LOG_DEBUG(sLogger,
              ("[Iterations] it", count_)("conn tracker map size", conn_trackers_.size())("total count",
                                                                                          conn_tracker_total_.load()));
    // std::this_thread::sleep_for(std::chrono::milliseconds (it_interval_ms_));
    int n = 0;
    std::queue<ConnId> tmp;
    std::map<std::string_view, std::vector<std::shared_ptr<ConnTracker>>> selfs; // containerId ==> ct
    std::map<std::string_view, std::vector<std::shared_ptr<ConnTracker>>> peers; // destIp ===> ct
    std::vector<std::string> cids;
    std::vector<std::string> dips;
    for (const auto& it : conn_trackers_) {
        auto conn_id = it.first;
        if (!it.second) {
            LOG_WARNING(sLogger, ("no conn tracker??? pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
            tmp.push(it.first);
            continue;
        }
        // LOG(INFO) << "Iterations:" << count_ << " conntracker pid:" << conn_id.tgid << " fd:" << conn_id.fd << "
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
            // force update
            // Aggregator::GetInstance().Aggregate(it.second->GetConnStatsRecord());
            it.second->UpdateReportTs(now_ts);
            tmp.push(it.first);
            n++;
            continue;
        }

        auto status = TryAttachPodMeta(it.second);
        if (status == ConnTrackerManager::AttachStatus::NEED_REMOTE) {
            if (!selfs.count(it.second->container_id_trim_)) {
                cids.push_back(it.second->container_id_trim_);
                selfs[it.second->container_id_trim_] = std::vector<std::shared_ptr<ConnTracker>>();
            }
            selfs[it.second->container_id_trim_].push_back(it.second);
        }
        status = TryAttachPeerPodMeta(it.second);
        if (status == ConnTrackerManager::AttachStatus::NEED_REMOTE) {
            // LOG(INFO) << "insert dip to query list ===>"
            //   << " pid:" << conn_id.tgid << " fd:" << conn_id.fd << " start:" << conn_id.start << " dip:" <<
            //   it.second->dip_ << " daddr:" << it.second->daddr_ << " query dip:" << it.second->dip_;
            // insert into peer
            if (!peers.count(it.second->dip_)) {
                dips.push_back(it.second->dip_);
                peers[it.second->dip_] = std::vector<std::shared_ptr<ConnTracker>>();
            }
            peers[it.second->dip_].push_back(it.second);
        }

        // if (it.second->MetaAttachReadyForNet() && (now_ts - it.second->GetLastReportTs()) > 15000) {
        // TODO @qianlu.kk
        // Aggregator::GetInstance().Aggregate(it.second->GetConnStatsRecord());
        // it.second->UpdateReportTs(now_ts);
        // }

        // when we query for conn tracker, we record active
        it.second->CountDownLatch();
    }

    // TODO @qianlu.kk do call backs and enrich k8s metadata ...
    // std::vector<std::shared_ptr<k8sContainerInfo>> meta;
    bool status = false;
    // LOG(INFO) << "begin to call container cb, cid size:" << cids.size();
    if (K8sMetadata::GetInstance().Enable()) {
        // meta.resize(cids.size());
        auto meta = K8sMetadata::GetInstance().SyncGetPodMetadataByContainerIds(cids, status);
        // ret = container_id_cb_(cids, meta);
        // LOG(INFO) << "begin to call cid cb, cid size:" << cids.size() << " meta size:" << meta.size();
        for (size_t i = 0; i < cids.size(); i++) {
            auto cid = cids[i];
            auto cts = selfs[cid];
            if (meta[i]) {
                for (auto ct : cts) {
                    ct->UpdateSelfPodMeta(meta[i]);
                    // auto conn_id = ct->GetConnId();
                    // LOG(INFO) << "attached self pod meta for conntracker===>" << " pid:" << conn_id.tgid
                    //   << " fd:" << conn_id.fd << " start:" << conn_id.start
                    //   << " containerid:" << ct->container_id_trim_ << " workloadName:" << meta[i]->workload_name_
                    //   << " arms_appname:" << meta[i]->app_name_ << " arms_appid:" << meta[i]->app_id_;
                }
            } else if (status) {
                for (auto ct : cts) {
                    ct->UpdateSelfPodMetaForUnknown();
                    // auto conn_id = ct->GetConnId();
                    // LOG(INFO) << "attached unkown for conntracker===>" << " pid:" << conn_id.tgid << " fd:" <<
                    // conn_id.fd << " start:" << conn_id.start << " containerid:" << ct->container_id_trim_;
                }
            } else {
                // failed to attach , ++ retry count ...
            }
        }
        meta.clear();
    }

    std::string dipStr;
    for (auto& dip : dips) {
        dipStr += dip + ",";
    }

    if (K8sMetadata::GetInstance().Enable()) {
        // meta.resize(dips.size());
        auto meta = K8sMetadata::GetInstance().SyncGetPodMetadataByIps(dips, status);
        // LOG(INFO) << "begin to call ip cb, dip size:" << dips.size() << " dips:" << dipStr << " meta size:" <<
        // meta.size();
        for (size_t i = 0; i < dips.size(); i++) {
            auto dip = dips[i];
            auto cts = peers[dip];

            if (meta[i]) {
                for (auto ct : cts) {
                    ct->UpdatePeerPodMeta(meta[i]);
                    // auto conn_id = ct->GetConnId();
                }

            } else if (status) {
                for (auto ct : cts) {
                    ct->UpdatePeerPodMetaForExternal();
                    // auto conn_id = ct->GetConnId();
                }
            } else {
                // retry ... @qianlu.kk

                // for (auto ct : cts) {
                //   auto conn_id = ct->GetConnId();
                // }
            }
            // meta.clear();
        }
    }

    // clean conn trackers ...
    while (tmp.size()) {
        auto conn_id = tmp.front();
        DeleteConnTracker(conn_id);
        LOG_DEBUG(sLogger, ("delete conntrackers pid", conn_id.tgid)("fd", conn_id.fd)("start", conn_id.start));
        tmp.pop();
    }
    LOG_DEBUG(sLogger,
              ("Iterations", count_)("remove conntrackers", n)("total conntrackers", conn_tracker_total_.load()));
    return;
}

ConnTrackerManager::~ConnTrackerManager() {
    if (flag_) {
        Stop();
    }
}

void ConnTrackerManager::Start() {
    flag_ = true;
}

void ConnTrackerManager::Stop() {
    flag_ = false;
}

} // namespace ebpf
} // namespace logtail
