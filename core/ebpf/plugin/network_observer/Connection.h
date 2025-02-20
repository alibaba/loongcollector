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

#pragma once

#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "common/Lock.h"
#include "ebpf/plugin/network_observer/Type.h"
#include "ebpf/type/table/AppTable.h"
#include "metadata/ContainerInfo.h"

extern "C" {
#include <coolbpf/net.h>
};

namespace logtail {
namespace ebpf {

struct ConnStatsData {
public:
    uint64_t mDropCount;
    uint64_t mConnSum;
    uint64_t mRttVar;
    uint64_t mRtt;
    uint64_t mRetransCount;
    uint64_t mRecvPackets;
    uint64_t mSendPackets;
    uint64_t mRecvBytes;
    uint64_t mSendBytes;
};

enum class MetadataAttachStatus {
    WAIT_FOR_KERNEL_EVENT, // need receive kernel event to trigger attach process
    KERNEL_EVENT_RECEIVED, // need to try to attach metadata from cache ...
    WAIT_QUERY_REMOTE_SERVER, // cache miss, need query metadata server
};

class Connection {
public:
    ~Connection() {}
    Connection(const ConnId& connId) : mConnId(connId) {}
    void UpdateConnStats(struct conn_stats_event_t* event);
    void UpdateConnState(struct conn_ctrl_event_t* event);

    const std::array<std::string, kConnTrackerElementsTableSize>& GetConnTrackerAttrs() {
        ReadLock lock(mAttrLock);
        return mAttrs;
    }

    const ConnId GetConnId() const { return mConnId; };

    bool ReadyToDestroy(const std::chrono::time_point<std::chrono::steady_clock>& now) {
        if (mIsClose && this->mEpoch < 0) {
            return true;
        }
        auto now_ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return now_ts > mLastActiveTs && (now_ts - mLastActiveTs) > 120000; // 120s
    }

    bool IsClose() const { return mIsClose; }

    int GetEpoch() const { return mEpoch; }

    void CountDown() { this->mEpoch--; }

    uint64_t GetLastUpdateTs() const { return mLastUpdateTs; }
    uint64_t GetLastActiveTs() const { return mLastActiveTs; }

    bool MetaAttachReadyForApp();
    bool MetaAttachReadyForNet();

    std::string DumpConnection() {
        std::string res;
        ReadLock lock(mAttrLock);
        for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
            res += mAttrs[i];
            res += ",";
        }

        return res;
    }

    void RecordActive() {
        this->mEpoch = 10;
        auto now = std::chrono::steady_clock::now();
        mLastActiveTs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }

    MetadataAttachStatus GetSelfMetadataAttachStatus() const { return mSelfMetadataAttachStatus; }

    MetadataAttachStatus GetPeerMetadataAttachStatus() const { return mPeerMetadataAttachStatus; }

    const std::string& GetContainerId() const {
        ReadLock lock(mAttrLock);
        return mAttrs[kConnTrackerTable.ColIndex(kContainerId.Name())];
    }

    const std::string& GetRemoteIp() const {
        ReadLock lock(mAttrLock);
        return mAttrs[kConnTrackerTable.ColIndex(kRemoteIp.Name())];
    }

    const std::string& GetSourceIp() const {
        ReadLock lock(mAttrLock);
        return mAttrs[kConnTrackerTable.ColIndex(kIp.Name())];
    }

    bool IsLocalhost() const;

    void TryAttachSelfMeta();
    void TryAttachPeerMeta();

    std::atomic_bool mNetMetaAttached = false;
    std::atomic_bool mK8sMetaAttached = false;
    std::atomic_bool mK8sPeerMetaAttached = false;
    std::atomic_bool mProtocolAttached = false;


    void SafeUpdateRole(enum support_role_e role);

    void SafeUpdateProtocol(ProtocolType protocol);

    support_role_e GetRole() const {
        ReadLock lock(mProtocolAndRoleLock);
        return mRole;
    }

private:
    void SafeUpdateNetMetaAttr(struct conn_stats_event_t* event);
    void TrySafeUpdateProtocolAttr();
    // peer pod meta
    void SafeUpdatePeerPodMetaForExternal();
    void UnsafeUpdatePeerPodMetaForExternal();
    void UpdatePeerPodMeta(const std::shared_ptr<k8sContainerInfo>& pod);
    void UpdatePeerPodMetaForLocalhost();

    // self pod meta
    void UpdateSelfPodMeta(const std::shared_ptr<k8sContainerInfo>& pod);
    void UpdateSelfPodMetaForUnknown();

    void MarkPodMetaAttached() { mK8sMetaAttached = true; }
    void MarkPeerPodMetaAttached() { mK8sPeerMetaAttached = true; }

    ProtocolType GetProtocol() const {
        ReadLock lock(mProtocolAndRoleLock);
        return mProtocol;
    }

    void UnsafeMarkClose() {
        this->mIsClose = true;
        this->mMarkCloseTime = std::chrono::steady_clock::now();
    }

    void UnsafeRecordLastUpdateTs(uint64_t ts) { mLastUpdateTs = ts; }

    ConnId mConnId;
    mutable ReadWriteLock mProtocolAndRoleLock;
    ProtocolType mProtocol = ProtocolType::UNKNOWN;
    // accessed by at least 2 threads ...
    support_role_e mRole = support_role_e::IsUnknown;

    MetadataAttachStatus mSelfMetadataAttachStatus = MetadataAttachStatus::WAIT_FOR_KERNEL_EVENT;
    MetadataAttachStatus mPeerMetadataAttachStatus = MetadataAttachStatus::WAIT_FOR_KERNEL_EVENT;

    mutable ReadWriteLock mAttrLock;
    // accessed by multiple threads ...
    std::array<std::string, kConnTrackerElementsTableSize> mAttrs;

    std::atomic_int mEpoch = 10;
    std::atomic_bool mIsClose = false;
    std::chrono::time_point<std::chrono::steady_clock> mMarkCloseTime;
    int64_t mLastUpdateTs = 0;
    int64_t mLastActiveTs = INT64_MAX;

    static std::regex mContainerIdRegex;

    ConnStatsData mCurrStats;
    ConnStatsData mLastStats;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConnectionUnittest;
    friend class ConnectionManagerUnittest;
    friend class NetworkObserverManagerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
