#include "ConnTracker.h"
#include "logger/Logger.h"
#include "common/magic_enum.hpp"

namespace logtail {
namespace ebpf {

std::regex ConnTracker::rgx_ = std::regex("[a-f0-9]{64}");

bool ConnTracker::MetaAttachReadyForApp() {
    auto res = net_meta_attached_ && k8s_meta_attached_ && k8s_peer_meta_attached_ && protocol_set_;
    if (!res) LOG_WARNING(sLogger, ("app meta not ready, container_id", container_id_trim_) 
        ("k8s_meta_attached", k8s_meta_attached_)
        ("k8s_peer_meta_attached", k8s_peer_meta_attached_)
        ("protocol_set", protocol_set_)
        ("net_meta_attached", net_meta_attached_)
        ("pid", conn_id_.tgid)
        ("fd", conn_id_.fd)
        ("start", conn_id_.start)
        ("protocol", int(protocol_))
        ("role", int(role))
        ("dip", dip_) ("daddr", daddr_));
    return res;
}

bool ConnTracker::MetaAttachReadyForNet() {
    auto res = net_meta_attached_ && k8s_meta_attached_ && k8s_peer_meta_attached_;
    if (!res) {
        LOG_WARNING(sLogger, ("net meta not ready, container_id", container_id_trim_) 
            ("k8s_meta_attached", k8s_meta_attached_)
            ("k8s_peer_meta_attached", k8s_peer_meta_attached_)
            ("protocol_set", protocol_set_)
            ("net_meta_attached", net_meta_attached_)
            ("pid", conn_id_.tgid)
            ("fd", conn_id_.fd)
            ("start", conn_id_.start)
            ("dip", dip_) ("daddr", daddr_));
    }
    // if (!res) LOG(WARNING) << "net meta not ready!! containerid" << container_id_trim_ 
    // << " k8s_meta_attached_: " << k8s_meta_attached_ << " k8s_peer_meta_attached_: "<< k8s_peer_meta_attached_
    // << " protocol_set_: " << protocol_set_ << " net_meta_attached_: "<< net_meta_attached_ << " pid:" << conn_id_.tgid  << " dip:" << dip_ << " daddr:" << daddr_;
    return res;
}


std::array<std::string, kConnTrackerElementsTableSize> ConnTracker::GetConnTrackerAttrs() {
    std::shared_lock<std::shared_mutex> lk(mtx_);
    if (k8s_meta_attached_ && k8s_peer_meta_attached_ && protocol_set_ && net_meta_attached_) return attrs_;
    LOG_WARNING(sLogger, ("not attach, containerid", container_id_trim_) 
                ("k8s_meta_attached", k8s_meta_attached_)
                ("k8s_peer_meta_attached", k8s_peer_meta_attached_)
                ("protocol_set", protocol_set_)
                ("net_meta_attached", net_meta_attached_)
                ("pid", conn_id_.tgid)
                ("fd", conn_id_.fd)
                ("start", conn_id_.start)
                ("dip", dip_) ("daddr", daddr_));
    LOG_WARNING(sLogger, ("not attach, containerid", container_id_trim_));
    return attrs_;
}

void ConnTracker::SafeUpdateRole(enum support_role_e role) {
    std::unique_lock<std::shared_mutex> lk(mtx_);
    UpdateRole(role);
}

void ConnTracker::UpdateRole(enum support_role_e role) {
    if (this->role == IsUnknown) {
        this->role = role;
        return;
    }
    
    if (this->role != role) {
        LOG_WARNING(sLogger, ("role change!! last role", magic_enum::enum_name(this->role)) ("new role", magic_enum::enum_name(role)));
        this->role = role;
    }
    return;
}

void ConnTracker::SafeUpdateProtocol(ProtocolType protocol) {
    std::unique_lock<std::shared_mutex> lk(mtx_);
    UpdateProtocol(protocol);
}

void ConnTracker::UpdateProtocol(ProtocolType protocol) {
    if (protocol == ProtocolType::UNKNOWN || protocol_set_) {
        return;
    }
    if (protocol_ != ProtocolType::UNKNOWN && int(protocol_) != int(protocol)) {
        LOG_WARNING(sLogger, ("protocol change!! last protocol", magic_enum::enum_name(this->protocol_)) ("new protocol", magic_enum::enum_name(protocol)));
    }
    
    {
        protocol_ = protocol;
        attrs_[kConnTrackerTable.ColIndex(kProtocol.name())] = std::string(magic_enum::enum_name(protocol_));
        if (role == support_role_e::IsClient) {
        attrs_[kConnTrackerTable.ColIndex(kRpcType.name())] = "25";
        attrs_[kConnTrackerTable.ColIndex(kCallKind.name())] = "http_client";
        attrs_[kConnTrackerTable.ColIndex(kCallType.name())] = "http_client";
        protocol_set_ = true;
        } else if (role == support_role_e::IsServer) {
        attrs_[kConnTrackerTable.ColIndex(kRpcType.name())] = "0";
        attrs_[kConnTrackerTable.ColIndex(kCallKind.name())] = "http";
        attrs_[kConnTrackerTable.ColIndex(kCallType.name())] = "http";
        protocol_set_ = true;
        }
        
        return;
    }
}

// void ConnTracker::UpdateSelfPodMeta(std::unique_ptr<PodMeta>& pod) {
//   std::unique_lock<std::shared_mutex> lk(mtx_);
//   if (!pod) {
//     // no meta info ...
//     LOG(WARNING) << "no pod info ... cid:" << container_id_trim_;
//     return;
//   }
//   arms_app_id_ = pod->app_id_;
//   arms_app_name_ = pod->app_name_;
//   pod_ip_ = pod->pod_ip_;
//   pod_name_ = pod->pod_name_;
//   workload_kind_ = pod->workload_kind_;
//   workload_name_ = pod->workload_name_;
//   namespace_ = pod->namespace_;

//   attrs_[kConnTrackerTable.ColIndex(kAppId.name())] = arms_app_id_;
//   attrs_[kConnTrackerTable.ColIndex(kAppName.name())] = arms_app_name_;
//   attrs_[kConnTrackerTable.ColIndex(kPodName.name())] = pod_name_;
//   attrs_[kConnTrackerTable.ColIndex(kPodIp.name())] = pod_ip_;
//   attrs_[kConnTrackerTable.ColIndex(kWorkloadName.name())] = workload_name_;
//   attrs_[kConnTrackerTable.ColIndex(kWorkloadKind.name())] = workload_kind_;
//   attrs_[kConnTrackerTable.ColIndex(kNamespace.name())] = namespace_;
//   attrs_[kConnTrackerTable.ColIndex(kHost.name())] = pod_name_;
//   MarkPodMetaAttached();
// }

void ConnTracker::UpdatePeerPodMetaForExternal() {
  std::unique_lock<std::shared_mutex> lk(mtx_);
  attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = "external";
  attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())] = "external";
  if (role == IsClient) {
    attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = dip_;
    attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
  }
  MarkPeerPodMetaAttached();
}
void ConnTracker::UpdatePeerPodMetaForLocalhost() {
  std::unique_lock<std::shared_mutex> lk(mtx_);
  attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = "localhost";
  attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = "localhost";
  attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = "localhost";
  attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = "localhost";
  attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = "localhost";
  if (role == IsClient) {
    attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = "localhost";
    attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
  }
  MarkPeerPodMetaAttached();
}
void ConnTracker::UpdateSelfPodMetaForUnknown() {
  std::unique_lock<std::shared_mutex> lk(mtx_);
  attrs_[kConnTrackerTable.ColIndex(kAppName.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kAppId.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kPodIp.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kWorkloadName.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kWorkloadKind.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kNamespace.name())] = "unknown";
  attrs_[kConnTrackerTable.ColIndex(kHost.name())] = "unknown";
  MarkPodMetaAttached();
}

// void ConnTracker::UpdatePeerPodMeta(std::unique_ptr<PodMeta>& pod) {
//   std::unique_lock<std::shared_mutex> lk(mtx_);
//   if (!pod) {
//     // no meta info ...
//     attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = "external";
//     attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())] = "external";
//     MarkPeerPodMetaAttached();
//     return;
//   }
//   // set workloadKind
//   peer_service_name_ = pod->service_name_;
//   peer_arms_app_name_ = pod->app_name_;
//   peer_pod_ip_ = pod->pod_ip_;
//   peer_pod_name_ = pod->pod_name_;
//   peer_workload_kind_ = pod->workload_kind_;
//   peer_workload_name_ = pod->workload_name_;
//   peer_namespace_ = pod->namespace_;

//   attrs_[kConnTrackerTable.ColIndex(kPeerAppName.name())] = peer_arms_app_name_.size() ? peer_workload_name_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerPodName.name())] = peer_pod_name_.size() ? peer_workload_name_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerPodIp.name())] = peer_pod_ip_.size() ? peer_workload_name_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadName.name())] = peer_workload_name_.size() ? peer_workload_name_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerWorkloadKind.name())] = peer_workload_kind_.size() ? peer_workload_kind_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerNamespace.name())] = peer_namespace_.size() ? peer_workload_name_ : "unknown";
//   attrs_[kConnTrackerTable.ColIndex(kPeerServiceName.name())] = peer_service_name_.size() ? peer_workload_name_ : "unknown";

//   if (role == IsClient) {
//     if (peer_arms_app_name_.size()) {
//       attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_arms_app_name_;
//     } else if (peer_workload_name_.size()) {
//       attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_workload_name_;
//     } else if (peer_service_name_.size()) {
//       attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = peer_service_name_;
//     } else {
//       attrs_[kConnTrackerTable.ColIndex(kDestId.name())] = "unknown";
//     }

//     attrs_[kConnTrackerTable.ColIndex(kEndpoint.name())] = dip_;
//   }

//   MarkPeerPodMetaAttached();
// }

/**
 * TODO @qianlu.kk
 *  @param event
 */

void ConnTracker::UpdateConnState(struct conn_ctrl_event_t *event) {
  std::unique_lock<std::shared_mutex> lk(mtx_);
  if (EventClose == event->type) {
    this->MarkClose();
  } else if (EventConnect == event->type) {
    // a new connection established
    
  }
}

void ConnTracker::UpdateConnStats(struct conn_stats_event_t *event) {
  std::unique_lock<std::shared_mutex> lk(mtx_);
  if (event->conn_events == StatusClose) {
    MarkClose();
  }

  if (event->ts <= this->last_update_timestamp) {
    return;
  }

  this->last_update_timestamp = event->ts;

  if (this->role != IsUnknown && this->role != event->role) {
    LOG_WARNING(sLogger, ("role change!! last role", magic_enum::enum_name(this->role)) ("new role", magic_enum::enum_name(event->role)));
  } else {
    // set role
    this->role = event->role;
    // LOG(WARNING) << " last role:" << int(this->role) << " new role:" << int(event->role) << " tgid:" << this->conn_id_.tgid << " fd:" << this->conn_id_.fd << " start:" << this->conn_id_.start;
  }

  if (event->protocol != support_proto_e::ProtoUnknown) {
    if (this->protocol_ != ProtocolType::UNKNOWN && int(protocol_) != int(event->protocol)) {
        LOG_WARNING(sLogger, ("protocol change!! last protocol", magic_enum::enum_name(this->protocol_)) ("new protocol", magic_enum::enum_name(event->protocol)));
    } else {
      // LOG(WARNING) << " last protocol:" << int(protocol_) << " new role:" << int(event->protocol) << " tgid:" << this->conn_id_.tgid << " fd:" << this->conn_id_.fd << " start:" << this->conn_id_.start;
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
    LOG_WARNING(sLogger, ("failed to parse addr string for addr", addr) ("family",family));
    return "";
  }
  return std::string(addrStr);
}

std::string ConnTracker::GetSourceIp() {
  std::shared_lock<std::shared_mutex> lk(mtx_);
  return sip_;
}

std::string ConnTracker::GetRemoteIp() {
  std::shared_lock<std::shared_mutex> lk(mtx_);
  return dip_;
}

std::string ConnTracker::GetSourceAddr() {
  std::shared_lock<std::shared_mutex> lk(mtx_);
  return saddr_;
}

std::string ConnTracker::GetRemoteAddr() {
  std::shared_lock<std::shared_mutex> lk(mtx_);
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

void ConnTracker::UpdateSocketInfo(struct conn_stats_event_t *event) {
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
    // LOG(INFO) << "origin container_id:" << container_id_ << " trim:" << container_id_trim_ << " match pos:" << match.position();
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
    family_ = "ipv4";
  } else if (si.family == AF_INET6) {
    family_ = "ipv6";
  }

  if (!net_meta_attached_) {
    InitEventCommonAttrs();
    net_meta_attached_ = true;
  }
  return;
}


}
}
