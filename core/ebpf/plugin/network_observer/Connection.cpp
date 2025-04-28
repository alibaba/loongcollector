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


#include "Connection.h"

#include <cctype>

#include "common/NetworkUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"
#include "metadata/K8sMetadata.h"

extern "C" {
#include <coolbpf/net.h>
}


namespace logtail::ebpf {

static constexpr StringView kExternalStr = "external";
static constexpr StringView kLocalhostStr = "localhost";
static constexpr StringView kHttpStr = "http";
static constexpr StringView kRpc25Str = "25";
static constexpr StringView kRpc0Str = "0";
static constexpr StringView kHttpClientStr = "http_client";
static constexpr StringView kUnknownStr = "unknown";
static constexpr StringView kZeroAddrStr = "0.0.0.0";
static constexpr StringView kLoopbackStr = "127.0.0.1";

std::regex Connection::mContainerIdRegex = std::regex("[a-f0-9]{64}");

bool Connection::IsLocalhost() const {
    const auto& remoteIp = GetRemoteIp();
    return (remoteIp == kLoopbackStr || remoteIp == kLocalhostStr || remoteIp == kZeroAddrStr);
}

// only called by poller thread ...
void Connection::UpdateConnState(struct conn_ctrl_event_t* event) {
    if (EventClose == event->type) {
        MarkClose();
    } else if (EventConnect == event->type) {
        // a new connection established, do nothing...
    }
}

// only called by poller thread ...
void Connection::UpdateConnStats(struct conn_stats_event_t* event) {
    if (event->conn_events == StatusClose) {
        MarkClose();
    }

    auto eventTs = static_cast<int64_t>(event->ts);
    if (eventTs <= this->mLastUpdateTs) {
        // event comes later ...
        LOG_DEBUG(sLogger, ("event comes later", "skip process"));
        return;
    }

    this->mLastUpdateTs = eventTs;
    if (!IsL4MetaAttachReady()) {
        LOG_DEBUG(sLogger, ("netMeta already attached", ""));
        updateL4Meta(event);
        MarkL4MetaAttached();
        TryAttachPeerMeta(event->si.family, event->si.ap.daddr);
        TryAttachSelfMeta();
    }

    TryAttachL7Meta(event->role, event->protocol);

    mCurrStats.mSendBytes += (event->last_output_wr_bytes == 0) ? 0 : (event->wr_bytes - event->last_output_wr_bytes);
    mCurrStats.mRecvBytes += (event->last_output_rd_bytes == 0) ? 0 : (event->rd_bytes - event->last_output_rd_bytes);
    mCurrStats.mSendPackets += (event->last_output_wr_pkts == 0) ? 0 : (event->wr_pkts - event->last_output_wr_pkts);
    mCurrStats.mRecvPackets += (event->last_output_rd_pkts == 0) ? 0 : (event->rd_pkts - event->last_output_rd_pkts);

    LOG_DEBUG(sLogger,
              ("stage", "updateConnStates")("mSendBytes", event->wr_bytes)("mRecvBytes", event->rd_bytes)(
                  "mSendPackets", event->wr_pkts)("mRecvPackets", event->rd_pkts)("last", "")(
                  "mSendBytes", event->last_output_wr_bytes)("mRecvBytes", event->last_output_rd_bytes)(
                  "mSendPackets", event->last_output_wr_pkts)("mRecvPackets", event->last_output_rd_pkts));

    this->RecordLastUpdateTs(event->ts);
}

bool Connection::GenerateConnStatsRecord(const std::shared_ptr<AbstractRecord>& in) {
    auto* record = static_cast<ConnStatsRecord*>(in.get());
    if (!record) {
        return false;
    }

    record->mRecvPackets = (mCurrStats.mRecvPackets == 0) ? 0 : mCurrStats.mRecvPackets;
    record->mSendPackets = (mCurrStats.mSendPackets == 0) ? 0 : mCurrStats.mSendPackets;
    record->mRecvBytes = (mCurrStats.mRecvBytes == 0) ? 0 : mCurrStats.mRecvBytes;
    record->mSendBytes = (mCurrStats.mSendBytes == 0) ? 0 : mCurrStats.mSendBytes;
    mCurrStats.Clear();

    return true;
}

void Connection::TryAttachL7Meta(support_role_e role, support_proto_e protocol) {
    if (IsL7MetaAttachReady()) {
        return;
    }

    // update role
    if (mRole == IsUnknown && role != IsUnknown) {
        mRole = role;
    }

    if (mProtocol == support_proto_e::ProtoUnknown && protocol != support_proto_e::ProtoUnknown) {
        mProtocol = protocol;
        mTags.Set<kProtocol>(std::string(magic_enum::enum_name(mProtocol)));
    }

    if (mProtocol == support_proto_e::ProtoHTTP) {
        if (mRole == support_role_e::IsClient) {
            mTags.SetNoCopy<kRpcType>(kRpc25Str);
            mTags.SetNoCopy<kCallKind>(kHttpClientStr);
            mTags.SetNoCopy<kCallType>(kHttpClientStr);
            MarkL7MetaAttached();
        } else if (mRole == support_role_e::IsServer) {
            mTags.SetNoCopy<kRpcType>(kRpc0Str);
            mTags.SetNoCopy<kCallKind>(kHttpStr);
            mTags.SetNoCopy<kCallType>(kHttpStr);
            MarkL7MetaAttached();
        }
    }
}

void Connection::updateL4Meta(struct conn_stats_event_t* event) {
    // handle container id ...
    std::string cidTrim;
    if (strlen(event->docker_id) > 0) {
        std::cmatch match;
        if (std::regex_search(event->docker_id, match, mContainerIdRegex)) {
            cidTrim = match.str(0);
        }
    }

    // handle socket info ...
    struct socket_info& si = event->si;
    auto sip = GetAddrString(si.ap.saddr);
    auto dip = GetAddrString(si.ap.daddr);

    auto sport = ntohs(si.ap.sport);
    auto dport = ntohs(si.ap.dport);
    auto saddr = sip + ":" + std::to_string(sport);
    auto daddr = dip + ":" + std::to_string(dport);
    auto netns = si.netns;
    auto family = GetFamilyString(si.family);

    // update attributes ...
    mTags.Set<kFd>(std::to_string(mConnId.fd));
    mTags.Set<kProcessId>(std::to_string(mConnId.tgid));
    mTags.Set<kStartTsNs>(std::to_string(mConnId.start));
    mTags.Set<kContainerId>(cidTrim);
    mTags.Set<kLocalAddr>(saddr);
    mTags.Set<kRemoteAddr>(daddr);
    mTags.Set<kRemotePort>(std::to_string(dport));
    mTags.Set<kNetNs>(std::to_string(netns));
    mTags.Set<kFamily>(family);
    mTags.Set<kTraceRole>(std::string(magic_enum::enum_name(mRole)));
    mTags.Set<kIp>(sip);
    mTags.Set<kRemoteIp>(dip);
}

void Connection::updateSelfPodMeta(const std::shared_ptr<K8sPodInfo>& pod) {
    if (!pod) {
        // no meta info ...
        LOG_WARNING(sLogger, ("no pod info ... cid:", mTags.Get<kContainerId>()));
        return;
    }

    std::string workloadKind = pod->mWorkloadKind;
    if (workloadKind.size()) {
        workloadKind[0] = std::toupper(workloadKind[0]); // upper case
    }

    mTags.Set<kAppId>(pod->mAppId);
    mTags.Set<kAppName>(pod->mAppName);
    mTags.Set<kPodName>(pod->mPodName);
    mTags.Set<kPodIp>(pod->mPodIp);
    mTags.Set<kWorkloadName>(pod->mWorkloadName);
    mTags.Set<kWorkloadKind>(workloadKind);
    mTags.Set<kNamespace>(pod->mNamespace);
    mTags.Set<kHostName>(pod->mPodName);
}

void Connection::updatePeerPodMetaForExternal() {
    mTags.SetNoCopy<kPeerAppName>(kExternalStr);
    mTags.SetNoCopy<kPeerPodName>(kExternalStr);
    mTags.SetNoCopy<kPeerPodIp>(kExternalStr);
    mTags.SetNoCopy<kPeerWorkloadName>(kExternalStr);
    mTags.SetNoCopy<kPeerWorkloadKind>(kExternalStr);
    mTags.SetNoCopy<kPeerNamespace>(kExternalStr);
    mTags.SetNoCopy<kPeerServiceName>(kExternalStr);
    if (mRole == IsClient) {
        auto daddr = mTags.Get<kRemoteAddr>();
        mTags.SetNoCopy<kDestId>(daddr);
        mTags.SetNoCopy<kEndpoint>(daddr);
    }
}

void Connection::updatePeerPodMetaForLocalhost() {
    mTags.SetNoCopy<kPeerAppName>(kLocalhostStr);
    mTags.SetNoCopy<kPeerPodName>(kLocalhostStr);
    mTags.SetNoCopy<kPeerPodIp>(kLocalhostStr);
    mTags.SetNoCopy<kPeerWorkloadName>(kLocalhostStr);
    mTags.SetNoCopy<kPeerWorkloadKind>(kLocalhostStr);
    if (mRole == IsClient) {
        mTags.SetNoCopy<kDestId>(kLocalhostStr);
        mTags.SetNoCopy<kEndpoint>(kLocalhostStr);
    }
}

void Connection::updateSelfPodMetaForUnknown() {
    mTags.SetNoCopy<kAppName>(kUnknownStr);
    mTags.SetNoCopy<kAppId>(kUnknownStr);
    mTags.SetNoCopy<kPodIp>(kUnknownStr);
    mTags.SetNoCopy<kWorkloadName>(kUnknownStr);
    mTags.SetNoCopy<kWorkloadKind>(kUnknownStr);
    mTags.SetNoCopy<kNamespace>(kUnknownStr);
    mTags.SetNoCopy<kHostName>(kUnknownStr);
}

void Connection::updatePeerPodMeta(const std::shared_ptr<K8sPodInfo>& pod) {
    if (!pod) {
        // no meta info ...
        return;
    }

    auto peerWorkloadKind = pod->mWorkloadKind;
    if (peerWorkloadKind.size()) {
        peerWorkloadKind[0] = std::toupper(peerWorkloadKind[0]);
    }

    mTags.Set<kPeerAppName>(pod->mAppName.size() ? pod->mAppName : kUnknownStr);
    mTags.Set<kPeerPodName>(pod->mPodName.size() ? pod->mPodName : kUnknownStr);
    mTags.Set<kPeerPodIp>(pod->mPodIp.size() ? pod->mPodIp : kUnknownStr);
    mTags.Set<kPeerWorkloadName>(pod->mWorkloadName.size() ? pod->mWorkloadName : kUnknownStr);
    mTags.Set<kPeerWorkloadKind>(peerWorkloadKind.size() ? peerWorkloadKind : kUnknownStr);
    mTags.Set<kPeerNamespace>(pod->mNamespace.size() ? pod->mNamespace : kUnknownStr);
    mTags.Set<kPeerServiceName>(pod->mServiceName.size() ? pod->mServiceName : kUnknownStr);

    // set destId and endpoint ...
    if (mRole == IsClient) {
        if (pod->mAppName.size()) {
            mTags.Set<kDestId>(pod->mAppName);
        } else if (pod->mWorkloadName.size()) {
            mTags.Set<kDestId>(pod->mWorkloadName);
        } else if (pod->mServiceName.size()) {
            mTags.Set<kDestId>(pod->mServiceName);
        } else {
            // TODO(qianlu.kk): set to rpc value...
            mTags.Set<kDestId>(kUnknownStr);
        }
        mTags.Set<kEndpoint>(mTags.Get<kRemoteAddr>());
    }
}

void Connection::TryAttachSelfMeta() {
    if (IsSelfMetaAttachReady()) {
        return;
    }
    if (!K8sMetadata::GetInstance().Enable()) {
        // set self metadata ...
        MarkSelfMetaAttached();
        return;
    }
    if (IsL4MetaAttachReady()) {
        const auto& cid = GetContainerId();
        if (cid.empty()) {
            updateSelfPodMetaForUnknown();
            MarkSelfMetaAttached();
            return;
        }

        auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
        if (info) {
            LOG_DEBUG(sLogger, ("get meta from cache", ""));
            updateSelfPodMeta(info);
            MarkSelfMetaAttached();
            return;
        }
        // async query
        K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::ContainerIdInfo, cid);
    }
}

void Connection::TryAttachPeerMeta(int family, uint32_t ip) {
    if (IsPeerMetaAttachReady()) {
        return;
    }
    if (!K8sMetadata::GetInstance().Enable()) {
        // k8smetadata not enable, mark attached ...
        MarkPeerMetaAttached();
        return;
    }

    if (IsLocalhost()) {
        updatePeerPodMetaForLocalhost();
        MarkPeerMetaAttached();
        return;
    }

    // not cluster ip
    if (family == AF_INET && !K8sMetadata::GetInstance().IsClusterIpForIPv4(ip)) {
        updatePeerPodMetaForExternal();
        MarkPeerMetaAttached();
        return;
    }

    if (IsL4MetaAttachReady()) {
        const auto& dip = GetRemoteIp();
        if (dip.empty()) {
            LOG_WARNING(sLogger, ("dip is empty, conn", DumpConnection()));
            updatePeerPodMetaForExternal();
            MarkPeerMetaAttached();
            return;
        }
        auto info = K8sMetadata::GetInstance().GetInfoByIpFromCache(dip);
        if (info) {
            updatePeerPodMeta(info);
            MarkPeerMetaAttached();
            return; // fill by cache
        }

        // if we don't find metadata info from cache,
        // we need to find out whether is an external ip ...
        if (K8sMetadata::GetInstance().IsExternalIp(dip)) {
            updatePeerPodMetaForExternal();
            MarkPeerMetaAttached();
            return;
        }

        // neither in cache nor external ip
        // start an async task
        K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::IpInfo, dip);
    }
}

} // namespace logtail::ebpf
