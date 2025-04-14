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

#pragma once

#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "common/Lock.h"
#include "ebpf/plugin/network_observer/Type.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/StaticDataRow.h"
#include "metadata/ContainerInfo.h"

extern "C" {
#include <coolbpf/net.h>
};

namespace logtail {
namespace ebpf {

class AbstractRecord;
class ConnStatsRecord;

struct ConnStatsData {
public:
    uint64_t mDropCount = 0;
    uint64_t mRttVar = 0;
    uint64_t mRtt = 0;
    uint64_t mRetransCount = 0;
    uint64_t mRecvPackets = 0;
    uint64_t mSendPackets = 0;
    uint64_t mRecvBytes = 0;
    uint64_t mSendBytes = 0;
};

class Connection {
public:
    ~Connection() {}
    Connection(const ConnId& connId) : mConnId(connId) {}
    void UpdateConnStats(struct conn_stats_event_t* event);
    void UpdateConnState(struct conn_ctrl_event_t* event);

    const StaticDataRow<&kConnTrackerTable>& GetConnTrackerAttrs() { return mTags; }

    const ConnId GetConnId() const { return mConnId; };

    bool ReadyToDestroy(const std::chrono::time_point<std::chrono::steady_clock>& now) {
        if (mIsClose && this->mEpoch < 0) {
            return true;
        }
        auto nowTs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return nowTs > mLastActiveTs && (nowTs - mLastActiveTs) > 10000; // 10s
    }

    bool IsClose() const { return mIsClose; }

    int GetEpoch() const { return mEpoch; }

    void CountDown() { this->mEpoch--; }

    uint64_t GetLastUpdateTs() const { return mLastUpdateTs; }
    uint64_t GetLastActiveTs() const { return mLastActiveTs; }

    inline bool IsMetaAttachReadyForAppRecord() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return (flags & sFlagAppRecordAttachReady) == sFlagAppRecordAttachReady;
    }
    inline bool IsMetaAttachReadyForNetRecord() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return (flags & sFlagNetRecordAttachReady) == sFlagNetRecordAttachReady;
    }

    inline bool IsL7MetaAttachReady() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagL7MetaAttached;
    }

    inline bool IsL4MetaAttachReady() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagL4MetaAttached;
    }

    inline bool IsSelfMetaAttachReady() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagSelfMetaAttached;
    }

    inline bool IsPeerMetaAttachReady() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagPeerMetaAttached;
    }

    inline bool IsConnStatsEventReceived() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagConnStatsEventReceived;
    }

    inline bool IsConnDeleted() {
        Flag flags = mMetaFlags.load(std::memory_order_acquire);
        return flags & sFlagConnDeleted;
    }

    std::string DumpConnection() {
        std::string res;
        for (size_t i = 0; i < kConnTrackerElementsTableSize; i++) {
            res += std::string(mTags[i]);
            res += ",";
        }
        res += std::to_string(mIsClose);
        res += ",";
        res += std::to_string(mMetaFlags.load(std::memory_order_acquire));

        return res;
    }

    void RecordActive() {
        this->mEpoch = 4;
        auto now = std::chrono::steady_clock::now();
        mLastActiveTs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }

    const StringView& GetContainerId() const { return mTags.Get<kContainerId>(); }

    const StringView& GetRemoteIp() const { return mTags.Get<kRemoteIp>(); }

    const StringView& GetSourceIp() const { return mTags.Get<kIp>(); }

    bool IsLocalhost() const;

    void TryAttachL7Meta(support_role_e role, support_proto_e protocol);
    void TryAttachSelfMeta(bool enable = true);
    void TryAttachPeerMeta(bool enable = true, int family = -1, uint32_t ip = std::numeric_limits<uint32_t>::max());

    // void UpdateRole(enum support_role_e role);

    // void UpdateProtocol(support_proto_e protocol);

    bool GenerateConnStatsRecord(const std::shared_ptr<AbstractRecord>& record);

    support_role_e GetRole() const { return mRole; }

    unsigned int GetMetaFlags() const { return mMetaFlags.load(); }

    void MarkConnStatsEventReceived() { mMetaFlags.fetch_or(sFlagConnStatsEventReceived, std::memory_order_release); }

    void MarkConnDeleted() { mMetaFlags.fetch_or(sFlagConnDeleted, std::memory_order_release); }

private:
    void UpdateL4Meta(struct conn_stats_event_t* event);
    // peer pod meta
    void UpdatePeerPodMetaForExternal();
    void UpdatePeerPodMeta(const std::shared_ptr<K8sPodInfo>& pod);
    void UpdatePeerPodMetaForLocalhost();

    // self pod meta
    void UpdateSelfPodMeta(const std::shared_ptr<K8sPodInfo>& pod);
    void UpdateSelfPodMetaForUnknown();

    using Flag = unsigned int;


    static constexpr Flag sFlagL4MetaAttached = 0b0001; // Flags[0]
    static constexpr Flag sFlagSelfMetaAttached = 0b0010; // Flags[1]
    static constexpr Flag sFlagPeerMetaAttached = 0b0100; // Flags[2]
    static constexpr Flag sFlagL7MetaAttached = 0b1000; // Flags[3]
    static constexpr Flag sFlagConnStatsEventReceived = 0b10000; // Flags[4]
    static constexpr Flag sFlagConnDeleted = 0b100000; // Flags[5]

    static constexpr Flag sFlagNetRecordAttachReady
        = (sFlagL4MetaAttached | sFlagSelfMetaAttached | sFlagPeerMetaAttached);
    static constexpr Flag sFlagAppRecordAttachReady = (sFlagNetRecordAttachReady | sFlagL7MetaAttached);

    void MarkSelfMetaAttached() { mMetaFlags.fetch_or(sFlagSelfMetaAttached, std::memory_order_release); }
    void MarkPeerMetaAttached() { mMetaFlags.fetch_or(sFlagPeerMetaAttached, std::memory_order_release); }
    void MarkL4MetaAttached() { mMetaFlags.fetch_or(sFlagL4MetaAttached, std::memory_order_release); }
    void MarkL7MetaAttached() { mMetaFlags.fetch_or(sFlagL7MetaAttached, std::memory_order_release); }

    support_proto_e GetProtocol() const { return mProtocol; }

    void MarkClose() {
        this->mIsClose = true;
        this->mMarkCloseTime = std::chrono::steady_clock::now();
    }

    void RecordLastUpdateTs(uint64_t ts) { mLastUpdateTs = ts; }

    ConnId mConnId;

    support_proto_e mProtocol = support_proto_e::ProtoUnknown;
    support_role_e mRole = support_role_e::IsUnknown;

    std::atomic<Flag> mMetaFlags = 0;

    StaticDataRow<&kConnTrackerTable> mTags;

    std::atomic_int mEpoch = 4;
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
