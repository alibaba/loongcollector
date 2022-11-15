/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once


#include "observer/network/sources/ebpf/include/syscall.h"
#include "DynamicLibHelper.h"
#include "network/NetworkConfig.h"
#include <atomic>
#include "observer/interface/helper.h"
#include "metas/ConnectionMetaManager.h"
#include "common/StringPiece.h"

// ::ffff:127.0.0.1
const uint64_t local_addr_bind_inet6[2] = {0, 72058143793676288};
#define EBPF_CONNECTION_FILTER(socketType, socketAddr, connectionId, connectionAddr) \
    { \
        if (this->mConfig->mDropUnixSocket && (socketType) == SocketCategory::UnixSocket) { \
            return; \
        } \
        if (this->mConfig->mDropLocalConnections && (socketType) == SocketCategory::InetSocket) { \
            if ((socketAddr).Type == SockAddressType_IPV4 \
                && ((socketAddr).Addr.IPV4 == INADDR_ANY || (socketAddr).Addr.IPV4 == htonl(INADDR_LOOPBACK))) { \
                g_ebpf_update_conn_addr_func(&(connectionId), &(connectionAddr), 0, true); \
                return; \
            } else if ((socketAddr).Type == SockAddressType_IPV6 && (socketAddr).Addr.IPV6[0] == 0 \
                       && (memcmp((socketAddr).Addr.IPV6, &in6addr_any, 16) == 0 \
                           || memcmp((socketAddr).Addr.IPV6, &in6addr_loopback, 16) == 0 \
                           || memcmp((socketAddr).Addr.IPV6, local_addr_bind_inet6, 16) == 0)) { \
                g_ebpf_update_conn_addr_func(&(connectionId), &(connectionAddr), 0, true); \
                return; \
            } \
        } \
        if (this->mConfig->mDropUnknownSocket && (socketType) == SocketCategory::UnknownSocket) { \
            return; \
        } \
    }
namespace logtail {
class EBPFWrapper {
public:
    explicit EBPFWrapper(NetworkConfig* config) : mConfig(config) {
        mPacketDataBuffer.resize(CONN_DATA_MAX_SIZE + 4096);
    }

    ~EBPFWrapper() { Stop(); }

    static EBPFWrapper* GetInstance() {
        static auto* sWrapper = new EBPFWrapper(NetworkConfig::GetInstance());
        sWrapper->mConnectionMetaManager = ConnectionMetaManager::GetInstance();
        return sWrapper;
    }

    bool Init(std::function<int(StringPiece)> processor);

    bool Start();

    bool Stop();

    void HoldOn(bool exitFlag);

    void Resume() { holdOnFlag = 0; }

    int32_t ProcessPackets(int32_t maxProcessPackets, int32_t maxProcessDurationMs);

    NetStaticticsMap& GetStatistics() { return mStatistics; }

    void OnData(struct conn_data_event_t* event);
    void OnCtrl(struct conn_ctrl_event_t* event);
    void OnStat(struct conn_stats_event_t* event);
    void OnLost(enum callback_type_e type, uint64_t count);

    void GetAllConnections(std::vector<struct connect_id_t>& connIds);
    void DeleteInvalidConnections(const std::vector<struct connect_id_t>& connIds);

    void DisableProcess(uint32_t pid);
    static uint32_t ConvertConnIdToSockHash(struct connect_id_t* id);
    void ProbeProcessStat();
    void CleanAllDisableProcesses();
    int32_t GetDisablesProcessCount();

protected:
    SocketCategory ConvertDataToPacketHeader(struct conn_data_event_t* event, PacketEventHeader* header);
    bool ConvertCtrlToPacketHeader(struct conn_ctrl_event_t* event, PacketEventHeader* header);
    SocketCategory
    ConvertSockAddress(union sockaddr_t& from, struct connect_id_t& connectId, SockAddress& addr, uint16_t& port);
    PacketRoleType
    DetectRole(enum support_role_e& kernelDetectRole, struct connect_id_t& connectId, uint16_t& remotePort) const;

private:
    static uint64_t readStat(uint64_t pid);

private:
    NetworkConfig* mConfig;
    DynamicLibLoader* mEBPFLib = NULL;
    std::function<int(StringPiece)> mPacketProcessor;
    std::int32_t holdOnFlag{0};
    NetStaticticsMap mStatistics;
    std::string mPacketDataBuffer;
    bool mInitSuccess = false;
    bool mStartSuccess = false;
    uint64_t mDeltaTimeNs = 0;
    std::unordered_map<uint32_t, uint64_t> mDisabledProcesses;

    ConnectionMetaManager* mConnectionMetaManager;

    bool isAppendMySQLMsg(conn_data_event_t* pEvent);
    /**
     * @param kernelVersion
     * @param kernelRelease
     * @return true if kernel version >= sls_observer_ebpf_min_kernel_version or kernel version equals to 3.10.x in
     * centos 7.6+.
     */
    bool isSupportedOS(int64_t& kernelVersion, std::string& kernelRelease);
    bool loadEbpfLib(int64_t kernelVersion, std::string& soPath);
};


} // namespace logtail
