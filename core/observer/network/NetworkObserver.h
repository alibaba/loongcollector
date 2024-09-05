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

#include "interface/network.h"
#include "interface/helper.h"
#include "NetworkConfig.h"
#include <unordered_map>
#include <ostream>
#include "protobuf/sls/sls_logs.pb.h"
#include "common/Thread.h"
#include "common/Lock.h"
#include "common/TimeUtil.h"
#include "common/StringPiece.h"
#include "metas/ContainerProcessGroup.h"
#include "ConnectionObserver.h"
#include "metas/ConnectionMetaManager.h"
#include "interface/layerfour.h"

namespace logtail {
class ProcessObserver;
class PCAPWrapper;
class EBPFWrapper;


class NetworkObserver {
public:
    static NetworkObserver* GetInstance() {
        static auto* sObserver = new NetworkObserver();
        return sObserver;
    }

    void Stop() {
        if (mEventLoopThread) {
            mEventLoopThread->Wait(100);
        }
    }

    void HoldOn(bool exitFlag = false);

    void Resume();

    void Reload();

private:
    NetworkObserver() {
        mLastGCTimeNs = GetCurrentTimeInNanoSeconds();
        mLastL4FlushTimeNs = GetCurrentTimeInNanoSeconds();
        mLastL7FlushTimeNs = GetCurrentTimeInNanoSeconds();
        mConfig = NetworkConfig::GetInstance();
        mNetworkStatistic = NetworkStatistic::GetInstance();
        mServiceMetaManager = ServiceMetaManager::GetInstance();
    }
    ~NetworkObserver();

    void EventLoop();


    void GarbageCollection(uint64_t nowTimeNs);

    uint32_t EBPFConnectionGC(uint64_t nowTimeNs);

    /**
     * @brief Get or create a new process observer to process network packet.
     * @param header the network packet meta data.
     * @param create  whether create new process obj when not found
     * @return ProcessObserver allocated in heap.
     */
    ProcessObserver* GetProcess(PacketEventHeader* header, bool create = true);

    /**
     * @brief BindSender bind different output ways, such as sls or plugins output ways.
     */
    void BindSender();
    static int OutputPluginProcess(std::vector<sls_logs::Log>& logs, const Pipeline* cfg);
    static int OutputDirectly(std::vector<sls_logs::Log>& logs, const Pipeline* cfg);

    /**
     * @brief Process bytes by different protocol processors.
     * @param sp network transfer bytes.
     * @return -1 means illegal arguments and 0 means success.
     */
    int OnPacketEventStringPiece(StringPiece sp) { return OnPacketEvent((void*)sp.c_str(), sp.size()); }

    int OnPacketEvent(void* event, size_t len);

    void OnProcessDestroyed(uint32_t pid, const char* command, size_t len);

    /**
     * @brief Output layer 4 statistics
     * @param allData allData stores all observer logs
     */
    void FlushOutStatistics(std::vector<sls_logs::Log>& allData);
    /**
     * @brief Read logs from ContainerProcessGroupManager
     * @param allData allData stores all observer protocol logs
     */
    void FlushOutMetrics(std::vector<sls_logs::Log>& allData);

    void FlushStatistics(logtail::NetStaticticsMap& map, std::vector<sls_logs::Log>& logs);

    void ReloadSource();

    // create a still running thread to process observer data.
    void StartEventLoop();

    std::unordered_map<uint32_t, ProcessObserver*> mAllProcesses;
    std::function<int(std::vector<sls_logs::Log>&, const Pipeline*)> mSenderFunc;
    ThreadPtr mEventLoopThread;
    ReadWriteLock mEventLoopThreadRWL;
    uint64_t mLastGCTimeNs = 0;
    uint64_t mLastL4FlushTimeNs = 0;
    uint64_t mLastL7FlushTimeNs = 0;
    uint64_t mLastEbpfGCTimeNs = 0;
    uint64_t mLastFlushMetaTimeNs = 0;
    uint64_t mLastFlushNetlinkTimeNs = 0;
    uint64_t mLastProbeDisableProcessNs = 0;
    uint64_t mLastCleanAllDisableProcessNs = 0;
    FILE* mDumpFilePtr = nullptr;
    FILE* mReplayFilePtr = nullptr;
    int64_t mDumpSize = 0;

    // don't delete following pointer, the lifecycles of them may be over current instance.
    NetworkStatistic* mNetworkStatistic;
    ServiceMetaManager* mServiceMetaManager;
    PCAPWrapper* mPCAPWrapper = nullptr;
    EBPFWrapper* mEBPFWrapper = nullptr;
    NetworkConfig* mConfig;

    friend class NetworkObserverUnittest;
    friend class PCAPWrapperUnittest;
    friend class EBPFWrapperUnittest;
    friend class LocalFileWrapperUnittest;
    friend class ProtocolDnsUnittest;
    friend class ProtocolHttpUnittest;
    friend class ProtocolMySqlUnittest;
    friend class ProtocolRedisUnittest;
    friend class ProtocolPgSqlUnittest;
};

} // namespace logtail
