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

#include "ConnTracker.h"

#include <cctype>

#include "common/magic_enum.hpp"
#include "logger/Logger.h"
extern "C" {
#include <net.h>
}

namespace logtail {
namespace ebpf {

const std::string EXTERNAL_STR = EXTERNAL_STR;
const std::string LOCALHOST_STR = LOCALHOST_STR;
const std::string HTTP_STR = "http";
const std::string RPC_25_STR = "25";
const std::string RPC_0_STR = "0";
const std::string HTTP_CLIENT_STR = "http_client";
const std::string UNKNOWN_STR = UNKNOWN_STR;
const std::string IPV4_STR = "ipv4";
const std::string IPV6_STR = "ipv6";

std::regex ConnTracker::rgx_ = std::regex("[a-f0-9]{64}");

bool ConnTracker::MetaAttachReadyForApp() {
    auto res = net_meta_attached_ && k8s_meta_attached_ && k8s_peer_meta_attached_ && protocol_set_;
    if (!res)
        LOG_WARNING(
            sLogger,
            ("app meta not ready, container_id", container_id_trim_)("k8s_meta_attached", k8s_meta_attached_)(
                "k8s_peer_meta_attached", k8s_peer_meta_attached_)("protocol_set", protocol_set_)(
                "net_meta_attached", net_meta_attached_)("pid", conn_id_.tgid)("fd", conn_id_.fd)(
                "start", conn_id_.start)("protocol", int(protocol_))("role", int(role))("dip", dip_)("daddr", daddr_));
    return res;
}

bool ConnTracker::MetaAttachReadyForNet() {
    auto res = net_meta_attached_ && k8s_meta_attached_ && k8s_peer_meta_attached_;
    if (!res) {
        LOG_WARNING(sLogger,
                    ("net meta not ready, container_id", container_id_trim_)("k8s_meta_attached", k8s_meta_attached_)(
                        "k8s_peer_meta_attached", k8s_peer_meta_attached_)("protocol_set", protocol_set_)(
                        "net_meta_attached", net_meta_attached_)("pid", conn_id_.tgid)("fd", conn_id_.fd)(
                        "start", conn_id_.start)("dip", dip_)("daddr", daddr_));
    }
    return res;
}


std::array<std::string, kConnTrackerElementsTableSize> ConnTracker::GetConnTrackerAttrs() {
    ReadLock lock(mReadWriteLock);
    if (k8s_meta_attached_ && k8s_peer_meta_attached_ && protocol_set_ && net_meta_attached_)
        return attrs_;
    LOG_WARNING(sLogger,
                ("not attach, containerid", container_id_trim_)("k8s_meta_attached", k8s_meta_attached_)(
                    "k8s_peer_meta_attached",
                    k8s_peer_meta_attached_)("protocol_set", protocol_set_)("net_meta_attached", net_meta_attached_)(
                    "pid", conn_id_.tgid)("fd", conn_id_.fd)("start", conn_id_.start)("dip", dip_)("daddr", daddr_));
    LOG_WARNING(sLogger, ("not attach, containerid", container_id_trim_));
    return attrs_;
}

void ConnTracker::SafeUpdateRole(enum support_role_e role) {
    WriteLock lock(mReadWriteLock);
    UpdateRole(role);
}

void ConnTracker::UpdateRole(enum support_role_e role) {
    if (this->role == IsUnknown) {
        this->role = role;
        return;
    }

    if (this->role != role) {
        LOG_WARNING(sLogger, ("role change!! last role", int(this->role))("new role", int(role)));
        this->role = role;
    }
    return;
}

void ConnTracker::SafeUpdateProtocol(ProtocolType protocol) {
    WriteLock lock(mReadWriteLock);
    UpdateProtocol(protocol);
}

void ConnTracker::UpdateProtocol(ProtocolType protocol) {
    if (protocol == ProtocolType::UNKNOWN || protocol_set_) {
        return;
    }
    if (protocol_ != ProtocolType::UNKNOWN && int(protocol_) != int(protocol)) {
        LOG_WARNING(sLogger, ("protocol change!! last protocol", int(this->protocol_))("new protocol", int(protocol)));
    }

    {
        protocol_ = protocol;
        attrs_[kConnTrackerTable.ColIndex(kProtocol.name())] = std::string(magic_enum::enum_name(protocol_));
        if (role == support_role_e::IsClient) {
            attrs_[kConnTrackerTable.ColIndex(kRpcType.name())] = RPC_25_STR;
            attrs_[kConnTrackerTable.ColIndex(kCallKind.name())] = HTTP_CLIENT_STR;
            attrs_[kConnTrackerTable.ColIndex(kCallType.name())] = HTTP_CLIENT_STR;
            protocol_set_ = true;
        } else if (role == support_role_e::IsServer) {
            attrs_[kConnTrackerTable.ColIndex(kRpcType.name())] = RPC_0_STR;
            attrs_[kConnTrackerTable.ColIndex(kCallKind.name())] = HTTP_STR;
            attrs_[kConnTrackerTable.ColIndex(kCallType.name())] = HTTP_STR;
            protocol_set_ = true;
        }

        return;
    }
}

void ConnTracker::UpdateSelfPodMeta(const std::shared_ptr<k8sContainerInfo>& pod) {
    WriteLock lock(mReadWriteLock);
    if (!pod) {
        // no meta info ...
        LOG_WARNING(sLogger, ("no pod info ... cid:", container_id_trim_));
        return;
    }
    arms_app_id_ = pod->appId;
    arms_app_name_ = pod->appName;
    pod_ip_ = pod->podIp;
    pod_name_ = pod->podName;
    workload_kind_ = pod->workloadKind;
    workload_name_ = pod->workloadName;
    namespace_ = pod->k8sNamespace;
    if (workload_kind_.size()) {
        workload_kind_[0] = std::toupper(workload_kind_[0]); // upper case
    }

    attrs_[kConnTrackerTable.ColIndex(kAppId.name())] = arms_app_id_;
    attrs_[kConnTrackerTable.ColIndex(kAppName.name())] = arms_app_name_;
    attrs_[kConnTrackerTable.ColIndex(kPodName.name())] = pod_name_;
    attrs_[kConnTrackerTable.ColIndex(kPodIp.name())] = pod_ip_;
    attrs_[kConnTrackerTable.ColIndex(kWorkloadName.name())] = workload_name_;
    attrs_[kConnTrackerTable.ColIndex(kWorkloadKind.name())] = workload_kind_;
    attrs_[kConnTrackerTable.ColIndex(kNamespace.name())] = namespace_;
    attrs_[kConnTrackerTable.ColIndex(kHost.name())] = pod_name_;
    MarkPodMetaAttached();
}

void ConnTracker::UpdatePeerPodMetaForExternal() {
    WriteLock lock(mReadWriteLock);
    attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = EXTERNAL_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())] = EXTERNAL_STR;
    if (role == IsClient) {
        attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = dip_;
        attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
    }
    MarkPeerPodMetaAttached();
}
void ConnTracker::UpdatePeerPodMetaForLocalhost() {
    WriteLock lock(mReadWriteLock);
    attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = LOCALHOST_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = LOCALHOST_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = LOCALHOST_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = LOCALHOST_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = LOCALHOST_STR;
    if (role == IsClient) {
        attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = LOCALHOST_STR;
        attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
    }
    MarkPeerPodMetaAttached();
}
void ConnTracker::UpdateSelfPodMetaForUnknown() {
    WriteLock lock(mReadWriteLock);
    attrs_[kConnTrackerTable.ColIndex(kAppName.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kAppId.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPodIp.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kWorkloadName.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kWorkloadKind.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kNamespace.name())] = UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kHost.name())] = UNKNOWN_STR;
    MarkPodMetaAttached();
}

void ConnTracker::UpdatePeerPodMeta(const std::shared_ptr<k8sContainerInfo>& pod) {
    WriteLock lock(mReadWriteLock);
    if (!pod) {
        // no meta info ...
        attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = EXTERNAL_STR;
        attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())] = EXTERNAL_STR;
        MarkPeerPodMetaAttached();
        return;
    }
    // set workloadKind
    peer_service_name_ = pod->serviceName;
    peer_arms_app_name_ = pod->appName;
    peer_pod_ip_ = pod->podIp;
    peer_pod_name_ = pod->podName;
    peer_workload_kind_ = pod->workloadKind;
    peer_workload_name_ = pod->workloadName;
    peer_namespace_ = pod->k8sNamespace;
    if (peer_workload_kind_.size()) {
        peer_workload_kind_[0] = std::toupper(peer_workload_kind_[0]);
    }

    attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())]
        = peer_arms_app_name_.size() ? peer_arms_app_name_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = peer_pod_name_.size() ? peer_pod_name_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = peer_pod_ip_.size() ? peer_pod_ip_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())]
        = peer_workload_name_.size() ? peer_workload_name_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())]
        = peer_workload_kind_.size() ? peer_workload_kind_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = peer_namespace_.size() ? peer_namespace_ : UNKNOWN_STR;
    attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())]
        = peer_service_name_.size() ? peer_service_name_ : UNKNOWN_STR;

    if (role == IsClient) {
        if (peer_arms_app_name_.size()) {
            attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_arms_app_name_;
        } else if (peer_workload_name_.size()) {
            attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_workload_name_;
        } else if (peer_service_name_.size()) {
            attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_service_name_;
        } else {
            attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = UNKNOWN_STR;
        }

        attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
    }

    MarkPeerPodMetaAttached();
}

/**
 * TODO @qianlu.kk
 *  @param event
 */

void ConnTracker::UpdateConnState(struct conn_ctrl_event_t* event) {
    WriteLock lock(mReadWriteLock);
    if (EventClose == event->type) {
        this->MarkClose();
    } else if (EventConnect == event->type) {
        // a new connection established
    }
}

void ConnTracker::UpdateConnStats(struct conn_stats_event_t* event) {
    WriteLock lock(mReadWriteLock);
    if (event->conn_events == StatusClose) {
        MarkClose();
    }

    int64_t tt = static_cast<int64_t>(event->ts);
    if (tt <= this->last_update_timestamp) {
        return;
    }

    this->last_update_timestamp = tt;

    if (this->role != IsUnknown && this->role != event->role) {
        LOG_WARNING(sLogger, ("role change!! last role", int(this->role))("new role", int(event->role)));
    } else {
        // set role
        this->role = event->role;
        // LOG(WARNING) << " last role:" << int(this->role) << " new role:" << int(event->role) << " tgid:" <<
        // this->conn_id_.tgid << " fd:" << this->conn_id_.fd << " start:" << this->conn_id_.start;
    }

    if (event->protocol != support_proto_e::ProtoUnknown) {
        if (this->protocol_ != ProtocolType::UNKNOWN && int(protocol_) != int(event->protocol)) {
            LOG_WARNING(
                sLogger,
                ("protocol change!! last protocol", int(this->protocol_))("new protocol", int(event->protocol)));
        } else {
            // LOG(WARNING) << " last protocol:" << int(protocol_) << " new role:" << int(event->protocol) << " tgid:"
            // << this->conn_id_.tgid << " fd:" << this->conn_id_.fd << " start:" << this->conn_id_.start;
            UpdateProtocol(ProtocolType(event->protocol));
        }
    }

    UpdateSocketInfo(event);

    // update conn stats
    // TODO @qianlu.kk
    //   this->curr_conn_stats_record_->send_bytes_ = event->wr_bytes;
    //   this->curr_conn_stats_record_->recv_bytes_ = event->rd_bytes;
    //   this->curr_conn_stats_record_->send_packets_ = event->wr_pkts;
    //   this->curr_conn_stats_record_->recv_packets_ = event->rd_pkts;

    // this->role = event->role;
    this->RecordLastUpdateTs(event->ts);
}

std::string AddrString(int family, uint32_t addr) {
    char addrStr[16];
    if (family != AF_INET && family != AF_INET6) {
        // LOG(INFO) << "[AddrString] family [" << family << "] neither ipv4 nor ipv6! skip!" << " addr:" << addr ;
        return "";
    }
    auto res = inet_ntop(AF_INET, &addr, addrStr, sizeof(addrStr));
    if (res == NULL) {
        LOG_WARNING(sLogger, ("failed to parse addr string for addr", addr)("family", family));
        return "";
    }
    return std::string(addrStr);
}

std::string ConnTracker::GetSourceIp() {
    ReadLock lock(mReadWriteLock);
    return sip_;
}

std::string ConnTracker::GetRemoteIp() {
    ReadLock lock(mReadWriteLock);
    return dip_;
}

std::string ConnTracker::GetSourceAddr() {
    ReadLock lock(mReadWriteLock);
    return saddr_;
}

std::string ConnTracker::GetRemoteAddr() {
    ReadLock lock(mReadWriteLock);
    return daddr_;
}

void ConnTracker::InitEventCommonAttrs() {
    attrs_[kConnTrackerTable.ColIndex(kFd.name())] = std::to_string(conn_id_.fd);
    attrs_[kConnTrackerTable.ColIndex(kPid.name())] = std::to_string(conn_id_.tgid);
    attrs_[kConnTrackerTable.ColIndex(kStartTsNs.name())] = std::to_string(conn_id_.start);
    attrs_[kConnTrackerTable.ColIndex(kContainerId.name())] = container_id_trim_;

    attrs_[kConnTrackerTable.ColIndex(kLocalAddr.name())] = saddr_;
    attrs_[kConnTrackerTable.ColIndex(kRemoteAddr.name())] = daddr_;
    attrs_[kConnTrackerTable.ColIndex(kRemotePort.name())] = std::to_string(dport_);
    attrs_[kConnTrackerTable.ColIndex(kNetNs.name())] = family_;
    attrs_[kConnTrackerTable.ColIndex(kFamily.name())] = std::to_string(net_ns_);
    attrs_[kConnTrackerTable.ColIndex(kTraceRole.name())] = std::string(magic_enum::enum_name(role));
    attrs_[kConnTrackerTable.ColIndex(kIp.name())] = sip_;
}

void ConnTracker::UpdateSocketInfo(struct conn_stats_event_t* event) {
    if (net_meta_attached_) {
        return;
    }

    struct socket_info& si = event->si;

    container_id_ = event->docker_id;
    if (!container_id_.empty()) {
        std::smatch match;
        if (std::regex_search(container_id_, match, rgx_)) {
            container_id_trim_ = match.str(0);
        }
        LOG_DEBUG(sLogger,
                  ("origin container_id", container_id_)("trim", container_id_trim_)("match pos", match.position()));
    }

    if (si.family == 0 && si.netns == 0 && si.ap.sport == 0 && si.ap.dport == 0) {
        LOG_WARNING(sLogger, ("[UpdateSocketInfo] incoming socket info is empty ", ""));
        return;
    }

    // init socket info
    sip_ = AddrString(si.family, si.ap.saddr);
    dip_ = AddrString(si.family, si.ap.daddr);
    saddr_ori_ = si.ap.saddr;
    daddr_ori_ = si.ap.daddr;
    sport_ = ntohs(si.ap.sport);
    dport_ = ntohs(si.ap.dport);
    saddr_ = sip_ + ":" + std::to_string(sport_);
    daddr_ = dip_ + ":" + std::to_string(dport_);
    net_ns_ = si.netns;
    if (si.family == AF_INET) {
        family_ = IPV4_STR;
    } else if (si.family == AF_INET6) {
        family_ = IPV6_STR;
    }

    if (!net_meta_attached_) {
        InitEventCommonAttrs();
        net_meta_attached_ = true;
    }
    return;
}

} // namespace ebpf
} // namespace logtail
