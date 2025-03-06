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
        mAttrs[kConnTrackerTable.ColIndex(kProtocol.Name())] = std::string(magic_enum::enum_name(mProtocol));
        if (mRole == support_role_e::IsClient) {
            mAttrs[kConnTrackerTable.ColIndex(kRpcType.Name())] = RPC_25_STR;
            mAttrs[kConnTrackerTable.ColIndex(kCallKind.Name())] = HTTP_CLIENT_STR;
            mAttrs[kConnTrackerTable.ColIndex(kCallType.Name())] = HTTP_CLIENT_STR;
            mProtocolAttached = true;
        } else if (mRole == support_role_e::IsServer) {
            mAttrs[kConnTrackerTable.ColIndex(kRpcType.Name())] = RPC_0_STR;
            mAttrs[kConnTrackerTable.ColIndex(kCallKind.Name())] = HTTP_STR;
            mAttrs[kConnTrackerTable.ColIndex(kCallType.Name())] = HTTP_STR;
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
        mAttrs[kConnTrackerTable.ColIndex(kFd.Name())] = std::to_string(mConnId.fd);
        mAttrs[kConnTrackerTable.ColIndex(kProcessId.Name())] = std::to_string(mConnId.tgid);
        mAttrs[kConnTrackerTable.ColIndex(kStartTsNs.Name())] = std::to_string(mConnId.start);
        mAttrs[kConnTrackerTable.ColIndex(kContainerId.Name())] = cidTrim;
        mAttrs[kConnTrackerTable.ColIndex(kLocalAddr.Name())] = saddr;
        mAttrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())] = daddr;
        mAttrs[kConnTrackerTable.ColIndex(kRemotePort.Name())] = std::to_string(dport);
        mAttrs[kConnTrackerTable.ColIndex(kNetNs.Name())] = std::to_string(netns);
        mAttrs[kConnTrackerTable.ColIndex(kFamily.Name())] = family;
        mAttrs[kConnTrackerTable.ColIndex(kTraceRole.Name())] = std::string(magic_enum::enum_name(mRole));
        mAttrs[kConnTrackerTable.ColIndex(kIp.Name())] = sip;
        mAttrs[kConnTrackerTable.ColIndex(kRemoteIp.Name())] = dip;
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
        LOG_WARNING(sLogger, ("no pod info ... cid:", mAttrs[kConnTrackerTable.ColIndex(kContainerId.Name())]));
        return;
    }

    std::string workloadKind = pod->workloadKind;
    if (workloadKind.size()) {
        workloadKind[0] = std::toupper(workloadKind[0]); // upper case
    }

    {
        WriteLock lock(mAttrLock);
        mAttrs[kConnTrackerTable.ColIndex(kAppId.Name())] = pod->appId;
        mAttrs[kConnTrackerTable.ColIndex(kAppName.Name())] = pod->appName;
        mAttrs[kConnTrackerTable.ColIndex(kPodName.Name())] = pod->podName;
        mAttrs[kConnTrackerTable.ColIndex(kPodIp.Name())] = pod->podIp;
        mAttrs[kConnTrackerTable.ColIndex(kWorkloadName.Name())] = pod->workloadName;
        mAttrs[kConnTrackerTable.ColIndex(kWorkloadKind.Name())] = workloadKind;
        mAttrs[kConnTrackerTable.ColIndex(kNamespace.Name())] = pod->k8sNamespace;
        mAttrs[kConnTrackerTable.ColIndex(kHostName.Name())] = pod->podName;
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
    mAttrs[kConnTrackerTable.ColIndex(kPeerAppName.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodName.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodIp.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadName.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadKind.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerNamespace.Name())] = EXTERNAL_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerServiceName.Name())] = EXTERNAL_STR;
    if (mRole == IsClient) {
        auto daddr = mAttrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())];
        mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = daddr;
        mAttrs[kConnTrackerTable.ColIndex(kEndpoint.Name())] = daddr;
    }
    MarkPeerPodMetaAttached();
}

void Connection::UpdatePeerPodMetaForLocalhost() {
    if (mK8sPeerMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    mAttrs[kConnTrackerTable.ColIndex(kPeerAppName.Name())] = LOCALHOST_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodName.Name())] = LOCALHOST_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodIp.Name())] = LOCALHOST_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadName.Name())] = LOCALHOST_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadKind.Name())] = LOCALHOST_STR;
    if (mRole == IsClient) {
        mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = LOCALHOST_STR;
        mAttrs[kConnTrackerTable.ColIndex(kEndpoint.Name())] = LOCALHOST_STR;
    }
    MarkPeerPodMetaAttached();
}

void Connection::UpdateSelfPodMetaForUnknown() {
    if (mK8sMetaAttached) {
        return;
    }
    WriteLock lock(mAttrLock);
    mAttrs[kConnTrackerTable.ColIndex(kAppName.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kAppId.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPodIp.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kWorkloadName.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kWorkloadKind.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kNamespace.Name())] = UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kHostName.Name())] = UNKNOWN_STR;
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

    mAttrs[kConnTrackerTable.ColIndex(kPeerAppName.Name())] = pod->appName.size() ? pod->appName : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodName.Name())] = pod->podName.size() ? pod->podName : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerPodIp.Name())] = pod->podIp.size() ? pod->podIp : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadName.Name())]
        = pod->workloadName.size() ? pod->workloadName : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerWorkloadKind.Name())]
        = peerWorkloadKind.size() ? peerWorkloadKind : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerNamespace.Name())]
        = pod->k8sNamespace.size() ? pod->k8sNamespace : UNKNOWN_STR;
    mAttrs[kConnTrackerTable.ColIndex(kPeerServiceName.Name())]
        = pod->serviceName.size() ? pod->serviceName : UNKNOWN_STR;

    // set destId and endpoint ...
    if (mRole == IsClient) {
        if (pod->appName.size()) {
            mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = pod->appName;
        } else if (pod->workloadName.size()) {
            mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = pod->workloadName;
        } else if (pod->serviceName.size()) {
            mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = pod->serviceName;
        } else {
            // TODO set to rpc value...
            mAttrs[kConnTrackerTable.ColIndex(kDestId.Name())] = UNKNOWN_STR;
        }
        mAttrs[kConnTrackerTable.ColIndex(kEndpoint.Name())] = mAttrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())];
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
