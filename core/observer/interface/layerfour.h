/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include "network.h"
#include "xxhash/xxhash.h"
#include "metas/ServiceMetaCache.h"
#include "helper.h"
namespace logtail {
struct NetStatisticsKey {
    uint32_t PID;
    uint32_t SockHash;
    ConnectionAddrInfo AddrInfo;
    SocketCategory SockCategory;
    PacketRoleType RoleType;

    void ToPB(sls_logs::Log* log) const;
};

struct NetStatisticsBase {
    int64_t SendBytes{0};
    int64_t RecvBytes{0};
    int64_t SendPackets{0};
    int64_t RecvPackets{0};

    inline void Merge(const struct NetStatisticsBase& o) {
        SendBytes += o.SendBytes;
        RecvBytes += o.RecvBytes;
        SendPackets += o.SendPackets;
        RecvPackets += o.RecvPackets;
    }
    void ToPB(sls_logs::Log* log) const;
};

struct NetStatisticsTCP {
    struct NetStatisticsBase Base {};
    int64_t SendTotalLatency{0};
    int64_t RecvTotalLatency{0};
    int64_t SendRetranCount{0};
    int64_t RecvRetranCount{0};
    int64_t SendZeroWinCount{0};
    int64_t RecvZeroWinCount{0};

    inline void Merge(const struct NetStatisticsTCP& o){
        this->Base.Merge(o.Base);
        SendTotalLatency += o.SendTotalLatency;
        RecvTotalLatency += o.RecvTotalLatency;
        SendRetranCount += o.SendRetranCount;
        RecvRetranCount += o.RecvRetranCount;
        SendZeroWinCount += o.SendZeroWinCount;
        RecvZeroWinCount += o.RecvZeroWinCount;
    }
    void ToPB(sls_logs::Log* log) const;
};


struct NetStatisticsKeyHash {
    size_t operator()(const NetStatisticsKey& key) const {
        return size_t(((uint64_t)key.PID << 32) | (uint64_t)key.SockHash);
    }
};

struct NetStatisticsKeyEqual {
    bool operator()(const NetStatisticsKey& a, const NetStatisticsKey& b) const {
        return a.PID == b.PID && a.SockHash == b.SockHash;
    }
};

// hash by connection
typedef std::unordered_map<NetStatisticsKey, NetStatisticsTCP, NetStatisticsKeyHash, NetStatisticsKeyEqual>
    NetStatisticsHashMap;


struct NetStaticticsMap {
    NetStatisticsHashMap mHashMap;

    NetStatisticsTCP& GetStatisticsItem(const NetStatisticsKey& key);
    inline void Clear() { mHashMap.clear(); }
};


struct MergedNetStatisticsKeyHash {
    size_t operator()(const logtail::NetStatisticsKey& key) const {
        uint32_t hash = XXH32(&key.AddrInfo.RemoteAddr, sizeof(key.AddrInfo.RemoteAddr), 0);
        hash = XXH32(&key.AddrInfo.RemotePort, sizeof(key.AddrInfo.RemotePort), hash);
        hash = XXH32(&key.PID, sizeof(key.PID), hash);
        return hash;
    }
};
struct MergedNetStatisticsKeyEqual {
    bool operator()(const logtail::NetStatisticsKey& a, const logtail::NetStatisticsKey& b) const {
        return a.PID == b.PID && a.AddrInfo.RemotePort == b.AddrInfo.RemotePort && a.RoleType == b.RoleType
            && a.AddrInfo.RemoteAddr == b.AddrInfo.RemoteAddr;
    }
};

// hash by process and remote addr
typedef std::unordered_map<logtail::NetStatisticsKey,
                           logtail::NetStatisticsTCP,
                           MergedNetStatisticsKeyHash,
                           MergedNetStatisticsKeyEqual>
    MergedNetStatisticsHashMap;


} // namespace logtail