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

const std::string EXTERNAL_STR = "external";
const std::string LOCALHOST_STR = "localhost";
const std::string HTTP_STR = "http";
const std::string RPC_25_STR = "25";
const std::string RPC_0_STR = "0";
const std::string HTTP_CLIENT_STR = "http_client";
const std::string UNKNOWN_STR = "unknown";
const std::string IPV4_STR = "ipv4";
const std::string IPV6_STR = "ipv6";
const std::string ZERO_ADDR_STR = "0.0.0.0";
const std::string LOOPBACK_STR = "127.0.0.1";

std::regex Connection::mContainerIdRegex = std::regex("[a-f0-9]{64}");

bool Connection::MetaAttachReadyForApp() {
    return mNetMetaAttached && mK8sMetaAttached && mK8sPeerMetaAttached && mProtocolAttached;
}

bool Connection::MetaAttachReadyForNet() {
    return mNetMetaAttached && mK8sMetaAttached && mK8sPeerMetaAttached;
}

bool Connection::IsLocalhost() const {
    auto& remoteIp = GetRemoteIp();
    return (remoteIp == LOOPBACK_STR || remoteIp == LOCALHOST_STR || remoteIp == ZERO_ADDR_STR);
}

// only called by poller thread ...
void Connection::UpdateConnState(struct conn_ctrl_event_t* event) {
    if (EventClose == event->type) {
        UnsafeMarkClose();
    } else if (EventConnect == event->type) {
        // a new connection established, do nothing...
    }
}

void Connection::SafeUpdateRole(enum support_role_e role) {
    if (!mProtocolAttached) {
        WriteLock lock(mProtocolAndRoleLock);
        if (mRole != IsUnknown && mRole != role) {
            LOG_WARNING(
                sLogger,
                ("role change!! last role", magic_enum::enum_name(mRole))("new role", magic_enum::enum_name(role)));
        } else {
            mRole = role;
            TrySafeUpdateProtocolAttr();
        }
    }
}

void Connection::SafeUpdateProtocol(support_proto_e protocol) {
    if (!mProtocolAttached) {
        WriteLock lock(mProtocolAndRoleLock);
        if (mProtocol != support_proto_e::ProtoUnknown && mProtocol != protocol) {
            LOG_WARNING(sLogger,
                        ("protocol change!! last protocol",
                         magic_enum::enum_name(mProtocol))("new protocol", magic_enum::enum_name(protocol)));
        } else {
            mProtocol = protocol;
            TrySafeUpdateProtocolAttr();
        }
    }
}

// only called by poller thread ...
void Connection::UpdateConnStats(struct conn_stats_event_t* event) {
    if (event->conn_events == StatusClose) {
        UnsafeMarkClose();
    }

    int64_t eventTs = static_cast<int64_t>(event->ts);
    if (eventTs <= this->mLastUpdateTs) {
        // event comes later ...
        LOG_DEBUG(sLogger, ("event comes later", "skip process"));
        return;
    }

    this->mLastUpdateTs = eventTs;

    SafeUpdateRole(event->role);

    SafeUpdateProtocol(event->protocol);

    SafeUpdateNetMetaAttr(event);

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

    this->UnsafeRecordLastUpdateTs(event->ts);
}

bool Connection::GenerateConnStatsRecord(const std::shared_ptr<AbstractRecord>& in) {
    ConnStatsRecord* record = static_cast<ConnStatsRecord*>(in.get());
    if (!record)
        return false;
    record->mDropCount = mCurrStats.mDropCount - mLastStats.mDropCount;
    record->mRetransCount = mCurrStats.mRetransCount - mLastStats.mRetransCount;
    record->mRecvPackets = mCurrStats.mRecvPackets - mLastStats.mRecvPackets;
    record->mSendPackets = mCurrStats.mSendPackets - mLastStats.mSendPackets;
    record->mRecvBytes = mCurrStats.mRecvBytes - mLastStats.mRecvBytes;
    record->mSendBytes = mCurrStats.mSendBytes - mLastStats.mSendBytes;
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

void Connection::TrySafeUpdateProtocolAttr() {
    if (mProtocol == support_proto_e::ProtoUnknown || mProtocolAttached) {
        return;
    }

    {
        WriteLock lock(mAttrLock);
        mTags.Set<kProtocol>(std::string(magic_enum::enum_name(mProtocol)));
        if (mRole == support_role_e::IsClient) {
            mTags.Set<kRpcType>(RPC_25_STR);
            mTags.Set<kCallKind>(HTTP_CLIENT_STR);
            mTags.Set<kCallType>(HTTP_CLIENT_STR);
            mProtocolAttached = true;
        } else if (mRole == support_role_e::IsServer) {
            mTags.Set<kRpcType>(RPC_0_STR);
            mTags.Set<kCallKind>(HTTP_STR);
            mTags.Set<kCallType>(HTTP_STR);
            mProtocolAttached = true;
        }

        return;
    }
}

void Connection::SafeUpdateNetMetaAttr(struct conn_stats_event_t* event) {
    if (mNetMetaAttached) {
        LOG_DEBUG(sLogger, ("netMeta already attached", ""));
        return;
    }

    if (mSelfMetadataAttachStatus == MetadataAttachStatus::WAIT_FOR_KERNEL_EVENT) {
        mSelfMetadataAttachStatus = MetadataAttachStatus::KERNEL_EVENT_RECEIVED;
    }
    if (mPeerMetadataAttachStatus == MetadataAttachStatus::WAIT_FOR_KERNEL_EVENT) {
        mPeerMetadataAttachStatus = MetadataAttachStatus::KERNEL_EVENT_RECEIVED;
    }

    // handle container id ...
    std::string cid = event->docker_id;
    std::string cidTrim;
    if (!cid.empty()) {
        std::smatch match;
        if (std::regex_search(cid, match, mContainerIdRegex)) {
            cidTrim = match.str(0);
        }
        LOG_DEBUG(sLogger, ("origin container_id", cid)("trim", cidTrim)("match pos", match.position()));
        TryAttachSelfMeta();
    } else {
        LOG_DEBUG(sLogger, ("no containerid", "attach unknown for self pod meta"));
        UpdateSelfPodMetaForUnknown();
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
    {
        WriteLock lock(mAttrLock);
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

    if (IsLocalhost()) {
        LOG_DEBUG(sLogger, ("remote ip is localhost", "attach localhost for peer pod meta"));
        UpdatePeerPodMetaForLocalhost();
    } else {
        LOG_DEBUG(sLogger, ("try attach peer meta", GetRemoteIp()));
        TryAttachPeerMeta();
    }

    if (cidTrim.empty()) {
        LOG_DEBUG(sLogger, ("no containerid", "attach unknown for self pod meta"));
        UpdateSelfPodMetaForUnknown();
    } else {
        LOG_DEBUG(sLogger, ("try attach self meta", GetContainerId()));
        TryAttachSelfMeta();
    }

    mNetMetaAttached = true;
}


void Connection::UpdateSelfPodMeta(const std::shared_ptr<k8sContainerInfo>& pod) {
    if (mK8sMetaAttached) {
        return;
    }

    if (!pod) {
        // no meta info ...
        LOG_WARNING(sLogger, ("no pod info ... cid:", mTags.Get<kContainerId>()));
        return;
    }

    std::string workloadKind = pod->workloadKind;
    if (workloadKind.size()) {
        workloadKind[0] = std::toupper(workloadKind[0]); // upper case
    }

    {
        WriteLock lock(mAttrLock);
        mTags.Set<kAppId>(pod->appId);
        mTags.Set<kAppName>(pod->appName);
        mTags.Set<kPodName>(pod->podName);
        mTags.Set<kPodIp>(pod->podIp);
        mTags.Set<kWorkloadName>(pod->workloadName);
        mTags.Set<kWorkloadKind>(workloadKind);
        mTags.Set<kNamespace>(pod->k8sNamespace);
        mTags.Set<kHostName>(pod->podName);
        MarkPodMetaAttached();
    }
}

void Connection::SafeUpdatePeerPodMetaForExternal() {
    if (mK8sPeerMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    UnsafeUpdatePeerPodMetaForExternal();
}

void Connection::UnsafeUpdatePeerPodMetaForExternal() {
    if (mK8sPeerMetaAttached) {
        return;
    }
    mTags.Set<kPeerAppName>(EXTERNAL_STR);
    mTags.Set<kPeerPodName>(EXTERNAL_STR);
    mTags.Set<kPeerPodIp>(EXTERNAL_STR);
    mTags.Set<kPeerWorkloadName>(EXTERNAL_STR);
    mTags.Set<kPeerWorkloadKind>(EXTERNAL_STR);
    mTags.Set<kPeerNamespace>(EXTERNAL_STR);
    mTags.Set<kPeerServiceName>(EXTERNAL_STR);
    if (mRole == IsClient) {
        auto daddr = mTags.Get<kRemoteAddr>();
        mTags.SetNoCopy<kDestId>(daddr);
        mTags.SetNoCopy<kEndpoint>(daddr);
    }
    MarkPeerPodMetaAttached();
}

void Connection::UpdatePeerPodMetaForLocalhost() {
    if (mK8sPeerMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    mTags.Set<kPeerAppName>(LOCALHOST_STR);
    mTags.Set<kPeerPodName>(LOCALHOST_STR);
    mTags.Set<kPeerPodIp>(LOCALHOST_STR);
    mTags.Set<kPeerWorkloadName>(LOCALHOST_STR);
    mTags.Set<kPeerWorkloadKind>(LOCALHOST_STR);
    if (mRole == IsClient) {
        mTags.Set<kDestId>(LOCALHOST_STR);
        mTags.Set<kEndpoint>(LOCALHOST_STR);
    }
    MarkPeerPodMetaAttached();
}

void Connection::UpdateSelfPodMetaForUnknown() {
    if (mK8sMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    mTags.Set<kAppName>(UNKNOWN_STR);
    // mTags.Set<kAppName>(UNKNOWN_STR);
    mTags.Set<kAppId>(UNKNOWN_STR);
    mTags.Set<kPodIp>(UNKNOWN_STR);
    mTags.Set<kWorkloadName>(UNKNOWN_STR);
    mTags.Set<kWorkloadKind>(UNKNOWN_STR);
    mTags.Set<kNamespace>(UNKNOWN_STR);
    mTags.Set<kHostName>(UNKNOWN_STR);
    MarkPodMetaAttached();
}

void Connection::UpdatePeerPodMeta(const std::shared_ptr<k8sContainerInfo>& pod) {
    if (mK8sPeerMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    if (!pod) {
        // no meta info ...
        UnsafeUpdatePeerPodMetaForExternal();
        return;
    }

    auto peerWorkloadKind = pod->workloadKind;
    if (peerWorkloadKind.size()) {
        peerWorkloadKind[0] = std::toupper(peerWorkloadKind[0]);
    }

    mTags.Set<kPeerAppName>(pod->appName.size() ? pod->appName : UNKNOWN_STR);
    mTags.Set<kPeerPodName>(pod->podName.size() ? pod->podName : UNKNOWN_STR);
    mTags.Set<kPeerPodIp>(pod->podIp.size() ? pod->podIp : UNKNOWN_STR);
    mTags.Set<kPeerWorkloadName>(pod->workloadName.size() ? pod->workloadName : UNKNOWN_STR);
    mTags.Set<kPeerWorkloadKind>(peerWorkloadKind.size() ? peerWorkloadKind : UNKNOWN_STR);
    mTags.Set<kPeerNamespace>(pod->k8sNamespace.size() ? pod->k8sNamespace : UNKNOWN_STR);
    mTags.Set<kPeerServiceName>(pod->serviceName.size() ? pod->serviceName : UNKNOWN_STR);

    // set destId and endpoint ...
    if (mRole == IsClient) {
        if (pod->appName.size()) {
            mTags.Set<kDestId>(pod->appName);
        } else if (pod->workloadName.size()) {
            mTags.Set<kDestId>(pod->workloadName);
        } else if (pod->serviceName.size()) {
            mTags.Set<kDestId>(pod->serviceName);
        } else {
            // TODO set to rpc value...
            mTags.Set<kDestId>(UNKNOWN_STR);
        }
        mTags.Set<kEndpoint>(mTags.Get<kRemoteAddr>());
    }

    MarkPeerPodMetaAttached();
}

void Connection::TryAttachSelfMeta() {
    if (mK8sMetaAttached) {
        return;
    }
    if (!K8sMetadata::GetInstance().Enable()) {
        MarkPeerPodMetaAttached();
        return;
    }

    switch (mSelfMetadataAttachStatus) {
        case MetadataAttachStatus::KERNEL_EVENT_RECEIVED: {
            // query cache ...
            auto& cid = GetContainerId();
            auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
            if (info) {
                UpdateSelfPodMeta(info);
                break;
            }
            // async task
            K8sMetadata::GetInstance().AsyncQueryMetadata(containerInfoType::ContainerIdInfo, cid);
            mSelfMetadataAttachStatus = MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER;
            break;
        }
        case MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER: {
            // query cache ...
            auto& cid = GetContainerId();
            auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(cid);
            if (info) {
                UpdateSelfPodMeta(info);
            }

            break;
        }
        default:
            break;
    }
}

void Connection::TryAttachPeerMeta() {
    if (mK8sPeerMetaAttached) {
        return;
    }

    if (!K8sMetadata::GetInstance().Enable()) {
        MarkPeerPodMetaAttached();
        return;
    }

    switch (mPeerMetadataAttachStatus) {
        case MetadataAttachStatus::KERNEL_EVENT_RECEIVED: {
            // query cache ...
            auto& dip = GetRemoteIp();
            if (dip.empty()) {
                LOG_DEBUG(sLogger, ("dip is empty, conn", DumpConnection()));
                break;
            }
            auto info = K8sMetadata::GetInstance().GetInfoByIpFromCache(dip);
            if (info) {
                UpdatePeerPodMeta(info);
                break;
            }

            // if we don't find metadata info from cache,
            // we need to find out whether is an external ip ...
            if (K8sMetadata::GetInstance().IsExternalIp(dip)) {
                SafeUpdatePeerPodMetaForExternal();
                break;
            }

            // neither in cache nor external ip
            // start an async task
            K8sMetadata::GetInstance().AsyncQueryMetadata(containerInfoType::IpInfo, dip);
            mPeerMetadataAttachStatus = MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER;
            break;
        }
        case MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER: {
            // query cache ...
            auto& dip = GetRemoteIp();
            // if we don't find metadata info from cache,
            // we need to find out whether is an external ip ...
            if (K8sMetadata::GetInstance().IsExternalIp(dip)) {
                SafeUpdatePeerPodMetaForExternal();
                break;
            }
            auto info = K8sMetadata::GetInstance().GetInfoByIpFromCache(dip);
            if (info) {
                UpdatePeerPodMeta(info);
            }
            break;
        }
        default:
            break;
    }
    return;
}

} // namespace ebpf
} // namespace logtail
