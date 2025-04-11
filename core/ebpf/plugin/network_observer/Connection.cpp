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

#include "Connection.h"

#include <cctype>

#include "common/NetworkUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/type/table/DataTable.h"
#include "logger/Logger.h"
#include "type/table/BaseElements.h"

extern "C" {
#include <coolbpf/net.h>
}

#include "metadata/K8sMetadata.h"

namespace logtail {
namespace ebpf {

static constexpr StringView EXTERNAL_STR = "external";
static constexpr StringView LOCALHOST_STR = "localhost";
static constexpr StringView HTTP_STR = "http";
static constexpr StringView RPC_25_STR = "25";
static constexpr StringView RPC_0_STR = "0";
static constexpr StringView HTTP_CLIENT_STR = "http_client";
static constexpr StringView UNKNOWN_STR = "unknown";
static constexpr StringView IPV4_STR = "ipv4";
static constexpr StringView IPV6_STR = "ipv6";
static constexpr StringView ZERO_ADDR_STR = "0.0.0.0";
static constexpr StringView LOOPBACK_STR = "127.0.0.1";

std::regex Connection::mContainerIdRegex = std::regex("[a-f0-9]{64}");

bool Connection::IsLocalhost() const {
    const auto& remoteIp = GetRemoteIp();
    return (remoteIp == LOOPBACK_STR || remoteIp == LOCALHOST_STR || remoteIp == ZERO_ADDR_STR);
}

// only called by poller thread ...
void Connection::UpdateConnState(struct conn_ctrl_event_t* event) {
    if (EventClose == event->type) {
        MarkClose();
    } else if (EventConnect == event->type) {
        // a new connection established, do nothing...
    }
}

void Connection::UpdateRole(enum support_role_e role) {
    if (!IsL7MetaAttachReady()) {
        // WriteLock lock(mProtocolAndRoleLock);
        if (mRole != IsUnknown && mRole != role) {
            LOG_WARNING(
                sLogger,
                ("role change!! last role", magic_enum::enum_name(mRole))("new role", magic_enum::enum_name(role)));
        } else {
            mRole = role;
            TryUpdateProtocolAttr();
        }
    }
}

void Connection::UpdateProtocol(support_proto_e protocol) {
    if (!IsL7MetaAttachReady()) {
        // WriteLock lock(mProtocolAndRoleLock);
        if (mProtocol != support_proto_e::ProtoUnknown && mProtocol != protocol) {
            LOG_WARNING(sLogger,
                        ("protocol change!! last protocol",
                         magic_enum::enum_name(mProtocol))("new protocol", magic_enum::enum_name(protocol)));
        } else {
            mProtocol = protocol;
            TryUpdateProtocolAttr();
        }
    }
}

// only called by poller thread ...
void Connection::UpdateConnStats(struct conn_stats_event_t* event) {
    if (event->conn_events == StatusClose) {
        MarkClose();
    }

    int64_t eventTs = static_cast<int64_t>(event->ts);
    if (eventTs <= this->mLastUpdateTs) {
        // event comes later ...
        LOG_DEBUG(sLogger, ("event comes later", "skip process"));
        return;
    }

    this->mLastUpdateTs = eventTs;

    UpdateRole(event->role);

    UpdateProtocol(event->protocol);

    UpdateNetMetaAttr(event);

    mCurrStats.mSendBytes = event->wr_bytes;
    mCurrStats.mRecvBytes = event->rd_bytes;
    mCurrStats.mSendPackets = event->wr_pkts;
    mCurrStats.mRecvPackets = event->rd_pkts;

    mLastStats.mSendBytes = event->last_output_wr_bytes;
    mLastStats.mRecvBytes = event->last_output_rd_bytes;
    mLastStats.mSendPackets = event->last_output_wr_pkts;
    mLastStats.mRecvPackets = event->last_output_rd_pkts;
    LOG_DEBUG(sLogger,
              ("stage", "updateConnStates")("mSendBytes", event->wr_bytes)("mRecvBytes", event->rd_bytes)(
                  "mSendPackets", event->wr_pkts)("mRecvPackets", event->rd_pkts)("last", "")(
                  "mSendBytes", event->last_output_wr_bytes)("mRecvBytes", event->last_output_rd_bytes)(
                  "mSendPackets", event->last_output_wr_pkts)("mRecvPackets", event->last_output_rd_pkts));

    this->RecordLastUpdateTs(event->ts);
}

bool Connection::GenerateConnStatsRecord(const std::shared_ptr<AbstractRecord>& in) {
    ConnStatsRecord* record = static_cast<ConnStatsRecord*>(in.get());
    if (!record) {
        return false;
    }

    record->mDropCount = (mLastStats.mDropCount == 0) ? 0 : mCurrStats.mDropCount - mLastStats.mDropCount;
    record->mRetransCount = (mLastStats.mRetransCount == 0) ? 0 : mCurrStats.mRetransCount - mLastStats.mRetransCount;
    record->mRecvPackets = (mLastStats.mRecvPackets == 0) ? 0 : mCurrStats.mRecvPackets - mLastStats.mRecvPackets;
    record->mSendPackets = (mLastStats.mSendPackets == 0) ? 0 : mCurrStats.mSendPackets - mLastStats.mSendPackets;
    record->mRecvBytes = (mLastStats.mRecvBytes == 0) ? 0 : mCurrStats.mRecvBytes - mLastStats.mRecvBytes;
    record->mSendBytes = (mLastStats.mSendBytes == 0) ? 0 : mCurrStats.mSendBytes - mLastStats.mSendBytes;
    record->mRtt = mCurrStats.mRtt;
    LOG_DEBUG(
        sLogger,
        ("stage", "GenerateConnStatsRecord")("mSendBytes", mCurrStats.mSendBytes)("mRecvBytes", mCurrStats.mRecvBytes)(
            "mSendPackets", mCurrStats.mSendPackets)("mRecvPackets", mCurrStats.mRecvPackets)("last", "")(
            "mSendBytes", mLastStats.mSendBytes)("mRecvBytes", mLastStats.mRecvBytes)(
            "mSendPackets", mLastStats.mSendPackets)("mRecvPackets", mLastStats.mRecvPackets)("record", "")(
            "dropCount", record->mDropCount)("retrans", record->mRetransCount)("sendPkts", record->mSendPackets)(
            "sendBytes", record->mSendBytes)("recvPkts", record->mRecvPackets)("recvBytes", record->mRecvPackets));

    return true;
}

void Connection::TryUpdateProtocolAttr() {
    if (mProtocol == support_proto_e::ProtoUnknown || IsL7MetaAttachReady()) {
        return;
    }

    mTags.Set<kProtocol>(std::string(magic_enum::enum_name(mProtocol)));
    if (mRole == support_role_e::IsClient) {
        mTags.SetNoCopy<kRpcType>(RPC_25_STR);
        mTags.SetNoCopy<kCallKind>(HTTP_CLIENT_STR);
        mTags.SetNoCopy<kCallType>(HTTP_CLIENT_STR);
    } else if (mRole == support_role_e::IsServer) {
        mTags.SetNoCopy<kRpcType>(RPC_0_STR);
        mTags.SetNoCopy<kCallKind>(HTTP_STR);
        mTags.SetNoCopy<kCallType>(HTTP_STR);
    }
    MarkL7MetaAttached();
}

void Connection::UpdateNetMetaAttr(struct conn_stats_event_t* event) {
    if (IsL4MetaAttachReady()) {
        LOG_DEBUG(sLogger, ("netMeta already attached", ""));
        return;
    }

    MarkConnStatsEventReceived();

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

    MarkL4MetaAttached();


    // for peer meta
    LOG_DEBUG(sLogger, ("try attach peer meta", GetRemoteIp()));
    TryAttachPeerMeta(true, si.family, si.ap.daddr);

    // for self meta
    LOG_DEBUG(sLogger, ("try attach self meta", GetContainerId()));
    TryAttachSelfMeta();
}

void Connection::UpdateSelfPodMeta(const std::shared_ptr<K8sPodInfo>& pod) {
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

void Connection::UpdatePeerPodMetaForExternal() {
    mTags.SetNoCopy<kPeerAppName>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerPodName>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerPodIp>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerWorkloadName>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerWorkloadKind>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerNamespace>(EXTERNAL_STR);
    mTags.SetNoCopy<kPeerServiceName>(EXTERNAL_STR);
    if (mRole == IsClient) {
        auto daddr = mTags.Get<kRemoteAddr>();
        mTags.SetNoCopy<kDestId>(daddr);
        mTags.SetNoCopy<kEndpoint>(daddr);
    }
}

void Connection::UpdatePeerPodMetaForLocalhost() {
    mTags.SetNoCopy<kPeerAppName>(LOCALHOST_STR);
    mTags.SetNoCopy<kPeerPodName>(LOCALHOST_STR);
    mTags.SetNoCopy<kPeerPodIp>(LOCALHOST_STR);
    mTags.SetNoCopy<kPeerWorkloadName>(LOCALHOST_STR);
    mTags.SetNoCopy<kPeerWorkloadKind>(LOCALHOST_STR);
    if (mRole == IsClient) {
        mTags.SetNoCopy<kDestId>(LOCALHOST_STR);
        mTags.SetNoCopy<kEndpoint>(LOCALHOST_STR);
    }
}

void Connection::UpdateSelfPodMetaForUnknown() {
    mTags.SetNoCopy<kAppName>(UNKNOWN_STR);
    mTags.SetNoCopy<kAppId>(UNKNOWN_STR);
    mTags.SetNoCopy<kPodIp>(UNKNOWN_STR);
    mTags.SetNoCopy<kWorkloadName>(UNKNOWN_STR);
    mTags.SetNoCopy<kWorkloadKind>(UNKNOWN_STR);
    mTags.SetNoCopy<kNamespace>(UNKNOWN_STR);
    mTags.SetNoCopy<kHostName>(UNKNOWN_STR);
}

void Connection::UpdatePeerPodMeta(const std::shared_ptr<K8sPodInfo>& pod) {
    if (!pod) {
        // no meta info ...
        return;
    }

    auto peerWorkloadKind = pod->mWorkloadKind;
    if (peerWorkloadKind.size()) {
        peerWorkloadKind[0] = std::toupper(peerWorkloadKind[0]);
    }

    mTags.Set<kPeerAppName>(pod->mAppName.size() ? pod->mAppName : UNKNOWN_STR);
    mTags.Set<kPeerPodName>(pod->mPodName.size() ? pod->mPodName : UNKNOWN_STR);
    mTags.Set<kPeerPodIp>(pod->mPodIp.size() ? pod->mPodIp : UNKNOWN_STR);
    mTags.Set<kPeerWorkloadName>(pod->mWorkloadName.size() ? pod->mWorkloadName : UNKNOWN_STR);
    mTags.Set<kPeerWorkloadKind>(peerWorkloadKind.size() ? peerWorkloadKind : UNKNOWN_STR);
    mTags.Set<kPeerNamespace>(pod->mNamespace.size() ? pod->mNamespace : UNKNOWN_STR);
    mTags.Set<kPeerServiceName>(pod->mServiceName.size() ? pod->mServiceName : UNKNOWN_STR);

    // set destId and endpoint ...
    if (mRole == IsClient) {
        if (pod->mAppName.size()) {
            mTags.Set<kDestId>(pod->mAppName);
        } else if (pod->mWorkloadName.size()) {
            mTags.Set<kDestId>(pod->mWorkloadName);
        } else if (pod->mServiceName.size()) {
            mTags.Set<kDestId>(pod->mServiceName);
        } else {
            // TODO set to rpc value...
            mTags.Set<kDestId>(UNKNOWN_STR);
        }
        mTags.Set<kEndpoint>(mTags.Get<kRemoteAddr>());
    }
}

void Connection::TryAttachSelfMeta(bool enable) {
    if (IsSelfMetaAttachReady()) {
        return;
    }
    if (!enable || !K8sMetadata::GetInstance().Enable()) {
        // set self metadata ...
        LOG_DEBUG(sLogger, ("not enable", ""));
        MarkSelfMetaAttached();
    } else if (IsConnStatsEventReceived()) {
        const auto& cid = GetContainerId();
        if (cid.empty()) {
            UpdateSelfPodMetaForUnknown();
            MarkSelfMetaAttached();
            return;
        }

        auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
        if (info) {
            LOG_DEBUG(sLogger, ("get meta from cache", ""));
            UpdateSelfPodMeta(info);
            MarkSelfMetaAttached();
            return;
        }
        // async query
        K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::ContainerIdInfo, cid);
    }
}

void Connection::TryAttachPeerMeta(bool enable, int family, uint32_t ip) {
    if (IsPeerMetaAttachReady()) {
        return;
    }
    if (!enable || !K8sMetadata::GetInstance().Enable()) {
        // k8smetadata not enable, mark attached ...
        MarkPeerMetaAttached();
        return;
    }

    if (IsLocalhost()) {
        UpdatePeerPodMetaForLocalhost();
        MarkPeerMetaAttached();
        return;
    } else {
        // not cluster ip
        if (family == AF_INET && !K8sMetadata::GetInstance().IsClusterIpForIPv4(ip)) {
            UpdatePeerPodMetaForExternal();
            MarkPeerMetaAttached();
            return;
        }
    }

    if (IsConnStatsEventReceived()) {
        const auto& dip = GetRemoteIp();
        if (dip.empty()) {
            LOG_WARNING(sLogger, ("dip is empty, conn", DumpConnection()));
            UpdatePeerPodMetaForExternal();
            MarkPeerMetaAttached();
            return;
        }
        auto info = K8sMetadata::GetInstance().GetInfoByIpFromCache(dip);
        if (info) {
            UpdatePeerPodMeta(info);
            MarkPeerMetaAttached();
            return; // fill by cache
        }

        // if we don't find metadata info from cache,
        // we need to find out whether is an external ip ...
        if (K8sMetadata::GetInstance().IsExternalIp(dip)) {
            UpdatePeerPodMetaForExternal();
            MarkPeerMetaAttached();
            return;
        }

        // neither in cache nor external ip
        // start an async task
        K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::IpInfo, dip);
    }
}

} // namespace ebpf
} // namespace logtail
