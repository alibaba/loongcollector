// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "NetworkObserver.h"
#include "logger/Logger.h"
#include "ProcessObserver.h"
#include "network/protocols/ProtocolEventAggregators.h"
#include "metas/ContainerProcessGroup.h"
#include "sources/pcap/PCAPWrapper.h"
#include "sources/ebpf/EBPFWrapper.h"
#include "config_manager/ConfigManager.h"
#include "MachineInfoUtil.h"
#include "FileSystemUtil.h"
#include "Monitor.h"
#include "metas/ConnectionMetaManager.h"
#include "Sender.h"
#include "LogtailPlugin.h"
#include "Constants.h"
#include "LogFileProfiler.h"

DEFINE_FLAG_INT64(sls_observer_network_ebpf_connection_gc_interval,
                  "SLS Observer NetWork connection gc interval seconds",
                  300);
DEFINE_FLAG_INT64(sls_observer_network_max_save_size,
                  "SLS Observer NetWork max save file size",
                  1024LL * 1024LL * 1024LL);
DEFINE_FLAG_STRING(sls_observer_network_save_filename, "SLS Observer NetWork save disk's file name", "ebpf.dump");

DECLARE_FLAG_INT32(merge_log_count_limit);

namespace logtail {

NetworkObserver::~NetworkObserver() {
    for (auto iter = mAllProcesses.begin(); iter != mAllProcesses.end(); ++iter) {
        delete iter->second;
    }
}
void NetworkObserver::HoldOn(bool exitFlag) {
    if (mEBPFWrapper != NULL) {
        mEBPFWrapper->HoldOn();
    }
    mEventLoopThreadRWL.lock();
    LOG_INFO(sLogger, ("hold on", "observer"));
}

void NetworkObserver::Resume() {
    if (mEBPFWrapper != NULL) {
        mEBPFWrapper->Resume();
    }
    Reload();
    mEventLoopThreadRWL.unlock();
    LOG_INFO(sLogger, ("resume on", "observer"));
}

uint32_t NetworkObserver::EBPFConnectionGC(uint64_t nowTimeNs) {
    std::vector<struct connect_id_t> connIds, toDeleteConnIds;
    mEBPFWrapper->GetAllConnections(connIds);
    uint32_t originConnSize = connIds.size();
    if (originConnSize < (size_t)64) {
        return originConnSize;
    }
    std::unordered_set<int32_t> pids;
    GetAllPids(pids);
    for (auto& connId : connIds) {
        auto findIter = mAllProcesses.find(connId.tgid);
        if (findIter != mAllProcesses.end()) {
            if (findIter->second->HasConnection(EBPFWrapper::ConvertConnIdToSockHash(&connId))) {
                continue;
            }
        }
        // check pid exists
        if (pids.find(connId.tgid) == pids.end()) {
            // @debug
            LOG_DEBUG(sLogger, ("delete conn because pid not exist, pid", connId.tgid)("fd", connId.fd));
            toDeleteConnIds.push_back(connId);
            continue;
        }
        // check fd exists
        std::string fdPath = std::string("/proc/")
                                 .append(std::to_string(connId.tgid))
                                 .append("/fd/")
                                 .append(std::to_string(connId.fd));
        if (!CheckExistance(fdPath)) {
            LOG_DEBUG(sLogger, ("delete conn because fd not exist, pid", connId.tgid)("fd", connId.fd));
            toDeleteConnIds.push_back(connId);
        }
    }
    uint32_t todoDeleteConnCount = toDeleteConnIds.size();
    ++mNetworkStatistic->mEbpfGCCount;
    mNetworkStatistic->mEbpfGCReleaseFDCount += todoDeleteConnCount;
    LOG_INFO(sLogger,
             ("ebpf connection gc, count", connIds.size())("to delete", todoDeleteConnCount)("pids", pids.size()));
    mEBPFWrapper->DeleteInvalidConnections(toDeleteConnIds);
    return originConnSize - todoDeleteConnCount;
}

void NetworkObserver::GarbageCollection(uint64_t nowTimeNs) {
    size_t maxSizeLimit = 1024 * 1024;
    ++mNetworkStatistic->mGCCount;
    ProtocolDebugStatistic::Clear();
    for (auto iter = mAllProcesses.begin(); iter != mAllProcesses.end();) {
        ProcessObserver* observer = iter->second;
        if (observer->GarbageCollection(maxSizeLimit, nowTimeNs)) {
            LOG_DEBUG(sLogger,
                      ("delete processor observer when gc, meta", observer->GetProcessMeta()->ToString())("pid",
                                                                                                          iter->first));
            static ContainerProcessGroupManager* containerProcessGroupManager
                = ContainerProcessGroupManager::GetInstance();
            // @note we must us iter->first as pid (not processMeta->Pid), because processMeta may belong to other pid
            // in the same container
            containerProcessGroupManager->OnProcessDestroy(observer->GetProcessMeta().get(), iter->first);
            mServiceMetaManager->OnProcessDestroy(iter->first);
            delete observer;
            iter = mAllProcesses.erase(iter);
            ++mNetworkStatistic->mGCReleaseProcessCount;
        } else {
            ++iter;
        }
        mServiceMetaManager->GarbageTimeoutHostname(nowTimeNs / 1000000);
    }
}

void NetworkObserver::FlushOutMetrics(std::vector<sls_logs::Log>& allData) {
    static ContainerProcessGroupManager* containerProcessGroupManager = ContainerProcessGroupManager::GetInstance();
    containerProcessGroupManager->FlushOutMetrics(allData, mConfig->mTags);
}

void NetworkObserver::FlushStatistics(NetStaticticsMap& statisticsMap, std::vector<sls_logs::Log>& allData) {
    static ContainerProcessGroupManager* cpgManager = ContainerProcessGroupManager::GetInstance();
    MergedNetStatisticsHashMap mergedMap;
    for (auto& item : statisticsMap.mHashMap) {
        auto iter = mergedMap.find(item.first);
        if (iter == mergedMap.end()) {
            mergedMap.insert(std::make_pair(item.first, item.second));
        } else {
            iter->second.Merge(item.second);
        }
    }
    size_t lastSize = allData.size();
    allData.resize(mergedMap.size() + lastSize);
    for (auto iter = mergedMap.begin(); iter != mergedMap.end() && lastSize < allData.size(); ++iter) {
        sls_logs::Log* log = &allData[lastSize];
        log->mutable_contents()->Reserve(16);
        auto content = log->add_contents();
        content->set_key("type");
        content->set_value("statistics");
        if (iter->first.PID == 0) {
            content = log->add_contents();
            content->set_key("pid");
            content->set_value("0");
        } else {
            auto meta = cpgManager->GetProcessMeta(iter->first.PID);
            if (!meta->PassFilterRules()) {
                if (this->mEBPFWrapper != nullptr) {
                    this->mEBPFWrapper->DisableProcess(iter->first.PID);
                }
                continue;
            }
            auto& tags = meta->GetFormattedMeta();
            for (const auto& item : tags) {
                content = log->add_contents();
                content->set_key(item.first);
                content->set_value(item.second);
            }
        }
        for (const auto& item : mConfig->mTags) {
            content = log->add_contents();
            content->set_key(item.first);
            content->set_value(item.second);
        }
        NetStaticticsMap::StatisticsPairToPB(iter->first, iter->second, log, false);
        mNetworkStatistic->mInputBytes += iter->second.Base.RecvBytes;
        mNetworkStatistic->mInputBytes += iter->second.Base.SendBytes;
        mNetworkStatistic->mInputEvents += iter->second.Base.RecvPackets;
        mNetworkStatistic->mInputEvents += iter->second.Base.SendPackets;
        mNetworkStatistic->mProtocolMatched += iter->second.Base.ProtocolMatched;
        mNetworkStatistic->mProtocolUnMatched += iter->second.Base.ProtocolUnMatched;
        ++lastSize;
    }
}

void NetworkObserver::FlushOutStatistics(std::vector<sls_logs::Log>& allData) {
    // pcap wrapper, do not need to add meta
    if (mPCAPWrapper != NULL) {
        NetStaticticsMap& statisticsMap = mPCAPWrapper->GetStatistics();
        FlushStatistics(statisticsMap, allData);
        statisticsMap.Clear();
    }

    if (mEBPFWrapper != NULL) {
        NetStaticticsMap& statisticsMap = mEBPFWrapper->GetStatistics();
        FlushStatistics(statisticsMap, allData);
        statisticsMap.Clear();
    }
}

ProcessObserver* NetworkObserver::GetProcess(PacketEventHeader* header, bool create) {
    auto findIter = mAllProcesses.find(header->PID);
    if (findIter != mAllProcesses.end()) {
        return findIter->second;
    }
    if (!create) {
        return NULL;
    }
    ProcessObserver* newProc = new ProcessObserver(header->TimeNano);
    static ContainerProcessGroupManager* containerProcessGroupManager = ContainerProcessGroupManager::GetInstance();
    ProcessMetaPtr processMeta = containerProcessGroupManager->GetProcessMeta(header->PID);
    ContainerProcessGroupPtr groupPtr
        = containerProcessGroupManager->GetContainerProcessGroupPtr(processMeta, header->PID);
    newProc->SetProcessGroup(groupPtr);
    mAllProcesses.insert(std::make_pair(header->PID, newProc));
    return newProc;
}

int NetworkObserver::OnPacketEvent(void* event, size_t len) {
    if (len < sizeof(PacketEventHeader)) {
        LOG_ERROR(sLogger, ("invalid packet len", len));
        return -1;
    }
    PacketEventHeader* header = static_cast<PacketEventHeader*>(event);
    static bool openPartialSelect = false;
    if (mConfig->mSaveToDisk) {
        if (mDumpFilePtr == NULL) {
            std::string fileName = STRING_FLAG(sls_observer_network_save_filename);
            if (mConfig->mLocalFileEnabled) {
                fileName += ".new";
            }
            mDumpFilePtr = fopen64(fileName.c_str(), "wb+");
            openPartialSelect = mConfig->isOpenPartialSelectDump();
        }
        if (mDumpFilePtr != NULL && mDumpSize < INT64_FLAG(sls_observer_network_max_save_size)) {
            if (!openPartialSelect
                || (header->PID == mConfig->localPickPID || header->SockHash == mConfig->localPickConnectionHashId
                    || header->SrcPort == mConfig->localPickSrcPort || header->DstPort == mConfig->localPickDstPort)) {
                std::string dumpContent;
                PacketEventToBuffer(event, len, dumpContent);
                mDumpSize += dumpContent.size();
                fwrite(dumpContent.data(), 1, dumpContent.size(), mDumpFilePtr);
            }
        }
    }
    switch (header->EventType) {
        case PacketEventType_None:
            break;
        case PacketEventType_Data: {
            PacketEventData* data = reinterpret_cast<PacketEventData*>((char*)event + sizeof(PacketEventHeader));

            if (data->PtlType == ProtocolType_None) {
                break;
            }
            ProcessObserver* proc = GetProcess(header);
            if (!proc->GetProcessMeta()->PassFilterRules()) {
                if (this->mEBPFWrapper != nullptr) {
                    this->mEBPFWrapper->DisableProcess(header->PID);
                }
                break;
            }
            proc->OnData(header, data);
        } break;
        case PacketEventType_Connected:
        case PacketEventType_Accepted:
            // create process
            GetProcess(header);
            break;
        case PacketEventType_Closed: {
            ProcessObserver* proc = GetProcess(header, false);
            if (proc == NULL) {
                break;
            }
            proc->ConnectionMarkDeleted(header);
        } break;
    }
    return 0;
}
void NetworkObserver::OnProcessDestroyed(uint32_t pid, const char* command, size_t len) {
    auto findIter = mAllProcesses.find(pid);
    if (findIter != mAllProcesses.end()) {
        auto& meta = findIter->second->GetProcessMeta();
        if (meta && meta->ProcessCMD.size() == len && memcmp(meta->ProcessCMD.c_str(), command, len) == 0) {
            findIter->second->MarkDeleted();
            LOG_DEBUG(sLogger, ("process destroyed, mark deleted, command", command)("pid", pid));
        } else {
            LOG_INFO(sLogger,
                     ("find pid on process destroyed, but command not match, destroyed command",
                      command)("pid", pid)("real command", meta ? meta->ProcessCMD : ""));
        }
    }
}
void NetworkObserver::ReloadSource() {
    LOG_INFO(sLogger, ("reload observer", "begin"));
    bool success = true;
    if (mConfig->mPCAPEnabled) {
        LOG_INFO(sLogger, ("reload module", "pcap"));
        mPCAPWrapper = PCAPWrapper::GetInstance();
        success = mPCAPWrapper->Stop()
            && mPCAPWrapper->Init(std::bind(&NetworkObserver::OnPacketEventStringPiece, this, std::placeholders::_1))
            && mPCAPWrapper->Start();
    } else {
        if (mPCAPWrapper != NULL) {
            LOG_INFO(sLogger, ("disable module", "pcap"));
            success = mPCAPWrapper->Stop();
        }
        mPCAPWrapper = NULL;
    }
    if (mConfig->mEBPFEnabled) {
        LOG_INFO(sLogger, ("reload module", "ebpf"));
        mEBPFWrapper = EBPFWrapper::GetInstance();
        success = mEBPFWrapper->Stop()
            && mEBPFWrapper->Init(std::bind(&NetworkObserver::OnPacketEventStringPiece, this, std::placeholders::_1))
            && mEBPFWrapper->Start();
    } else {
        if (mEBPFWrapper != NULL) {
            LOG_INFO(sLogger, ("disable module", "ebpf"));
            success = mEBPFWrapper->Stop();
        }
        mEBPFWrapper = NULL;
    }
    if (success) {
        ContainerProcessGroupManager::GetInstance()->ResetFilterProcessMeta();
    }
    LOG_INFO(sLogger, ("reload observer result", success ? "success" : "fail"));
}

void NetworkObserver::Reload() {
    std::vector<Config*> allObserverConfigs;
    ConfigManager::GetInstance()->GetAllObserverConfig(allObserverConfigs);
    mConfig->BeginLoadConfig();
    for (auto config : allObserverConfigs) {
        mConfig->LoadConfig(config);
    }
    mConfig->EndLoadConfig();
    if (!mConfig->NeedReload()) {
        return;
    }
    mConfig->Reload();
    ReloadSource();
    StartEventLoop();
    BindSender();
}

void NetworkObserver::EventLoop() {
    LOG_INFO(sLogger, ("start observer network event loop", "success"));
    ContainerProcessGroupManager::GetInstance()->Init();
    if (mConfig->mLocalFileEnabled) {
        mReplayFilePtr = fopen64(STRING_FLAG(sls_observer_network_save_filename).c_str(), "rb+");
    }
    uint64_t lastProfilingTime = GetCurrentTimeInNanoSeconds();
    while (true) {
        bool hasMoreData = false;
        ReadLock lock(mEventLoopThreadRWL);
        if (mPCAPWrapper == nullptr && mEBPFWrapper == nullptr) {
            static int sErrorCount = 0;
            static int sErrorPintCount = 60000 / INT32_FLAG(sls_observer_network_no_data_sleep_interval_ms);
            if (++sErrorCount % sErrorPintCount == 0) {
                LOG_WARNING(sLogger, ("no observer datasource working", ""));
                sErrorCount = 0;
            }
            usleep(1000 * INT32_FLAG(sls_observer_network_no_data_sleep_interval_ms));
            continue;
        }
        uint64_t nowTimeNs = GetCurrentTimeInNanoSeconds();
        // fetching and processing packets
        if (mPCAPWrapper != NULL) {
            int32_t rst = mPCAPWrapper->ProcessPackets(100, 100);
            if (rst >= 100) {
                hasMoreData = true;
            }
            if (rst != 0) {
                auto deltaTime = GetCurrentTimeInNanoSeconds() - nowTimeNs;
                LOG_DEBUG(sLogger, ("process pcap packets, rst", rst)("time", deltaTime / 1000LL / 1000LL));
            }
        }

        if (mEBPFWrapper != NULL) {
            if (nowTimeNs - mLastCleanAllDisableProcessNs
                >= INT64_FLAG(sls_observer_network_cleanall_disable_process_interval) * 1000ULL * 1000ULL * 1000ULL) {
                mLastCleanAllDisableProcessNs = nowTimeNs;
                mEBPFWrapper->CleanAllDisableProcesses();
            }
            mNetworkStatistic->mEbpfDisableProcesses = mEBPFWrapper->GetDisablesProcessCount();
            if (nowTimeNs - mLastProbeDisableProcessNs
                >= INT64_FLAG(sls_observer_network_probe_disable_process_interval) * 1000ULL * 1000ULL * 1000ULL) {
                mLastProbeDisableProcessNs = nowTimeNs;
                mEBPFWrapper->ProbeProcessStat();
            }
            int32_t rst = mEBPFWrapper->ProcessPackets(100, 100);
            if (rst >= 100) {
                hasMoreData = true;
            }
            if (rst != 0) {
                LOG_TRACE(sLogger,
                          ("process ebpf packets, rst",
                           rst)("time", GetCurrentTimeInNanoSeconds() - nowTimeNs / 1000LL / 1000LL));
            }
        }
        if (mReplayFilePtr != NULL) {
            uint32_t dataSize = 0;
            fread(&dataSize, 1, 4, mReplayFilePtr);
            if (dataSize != 0 && dataSize < 1024 * 1024) {
                char* buf = (char*)malloc(dataSize);
                if (dataSize != fread(buf, 1, dataSize, mReplayFilePtr)) {
                    LOG_ERROR(sLogger, ("read ebpf replay file failed", errno));
                } else {
                    void* event = NULL;
                    int32_t len = 0;
                    BufferToPacketEvent(buf, dataSize, event, len);
                    if (event != NULL) {
                        OnPacketEvent(event, len);
                        hasMoreData = true;
                    }
                }
                free(buf);
            }
        }

        // fetching metas
        if (nowTimeNs - mLastFlushMetaTimeNs >= mConfig->mFlushMetaInterval * 1000ULL * 1000ULL * 1000ULL) {
            mLastFlushMetaTimeNs = nowTimeNs;
            ContainerProcessGroupManager::GetInstance()->Init();
            ContainerProcessGroupManager::GetInstance()->FlushMetas();
        }

        // GC
        if (nowTimeNs - mLastGCTimeNs >= INT64_FLAG(sls_observer_network_gc_interval) * 1000ULL * 1000ULL * 1000ULL) {
            mLastGCTimeNs = nowTimeNs;
            GarbageCollection(nowTimeNs);
        }
        if (mEBPFWrapper != NULL
            && nowTimeNs - mLastEbpfGCTimeNs
                > INT64_FLAG(sls_observer_network_ebpf_connection_gc_interval) * 1000ULL * 1000ULL * 1000ULL) {
            mLastEbpfGCTimeNs = nowTimeNs;
            mNetworkStatistic->mEbpfUsingConnections = EBPFConnectionGC(nowTimeNs);
        }
        if (nowTimeNs - mLastFlushNetlinkTimeNs >= mConfig->mFlushNetlinkInterval * 1000ULL * 1000ULL * 1000ULL) {
            mLastFlushNetlinkTimeNs = nowTimeNs;
            ConnectionMetaManager::GetInstance()->Init();
            ConnectionMetaManager::GetInstance()->GarbageCollection();
        }

        // flush observer metrics
        if (nowTimeNs - mLastFlushTimeNs >= mConfig->mFlushOutInterval * 1000ULL * 1000ULL * 1000ULL) {
            mLastFlushTimeNs = nowTimeNs;
            std::vector<sls_logs::Log> allLogs;
            FlushOutMetrics(allLogs);
            if (mSenderFunc) {
                mSenderFunc(allLogs, mConfig->mLastApplyedConfig);
            }

            mNetworkStatistic->mOutputEvents += allLogs.size();
            for (const auto& item : allLogs) {
                mNetworkStatistic->mOutputBytes += item.GetCachedSize();
            }
            allLogs.clear();

            FlushOutStatistics(allLogs);
            if (mSenderFunc) {
                mSenderFunc(allLogs, mConfig->mLastApplyedConfig);
            }
        }

        // flush profile metrics
        if ((nowTimeNs - lastProfilingTime) >= INT32_FLAG(monitor_interval) * 1000ULL * 1000ULL * 1000ULL) {
            static auto sPMStat = ProcessMetaStatistic::GetInstance();
            static auto sCMStat = ConnectionMetaStatistic::GetInstance();
            static auto sPStat = ProtocolStatistic::GetInstance();
            static auto sPDStat = ProtocolDebugStatistic::GetInstance();
            LOG_DEBUG(sLogger, ("observer_process_meta_statistic", sPMStat->ToString()));
            LOG_DEBUG(sLogger, ("observer_connection_meta_statistic", sCMStat->ToString()));
            LOG_DEBUG(sLogger, ("observer_protocol_statistic", sPStat->ToString()));
            LOG_DEBUG(sLogger, ("observer_protocol_mem_statistic", sPDStat->ToString()));
            LOG_DEBUG(sLogger, ("observer_network_statistic", mNetworkStatistic->ToString()));

            sPMStat->FlushMetrics();
            sCMStat->FlushMetrics();
            sPStat->FlushMetrics();
            sPDStat->FlushMetrics(BOOL_FLAG(sls_observer_network_protocol_stat));
            mNetworkStatistic->FlushMetrics();
            LogtailMonitor::Instance()->UpdateMetric("observer_container_category",
                                                     ContainerProcessGroupManager::GetInstance()->GetContainerType());
            lastProfilingTime = nowTimeNs;
        }
        if (!hasMoreData) {
            usleep(1000 * INT32_FLAG(sls_observer_network_no_data_sleep_interval_ms));
        }
    }
}

void NetworkObserver::BindSender() {
    mSenderFunc = this->mConfig->mLastApplyedConfig->mPluginProcessFlag ? OutputPluginProcess : OutputDirectly;
}

inline void NetworkObserver::StartEventLoop() {
    if (!mEventLoopThread) {
        mEventLoopThread = CreateThread([this]() { EventLoop(); });
    }
}
int NetworkObserver::OutputPluginProcess(std::vector<sls_logs::Log>& logs, Config* config) {
    static auto sPlugin = LogtailPlugin::GetInstance();
    uint32_t nowTime = time(nullptr);
    for (auto& item : logs) {
        item.set_time(nowTime);
        sPlugin->ProcessLog(config->mConfigName, item, "", config->mGroupTopic, "");
    }
    return 0;
}

int NetworkObserver::OutputDirectly(std::vector<sls_logs::Log>& logs, Config* config) {
    static auto sSenderInstance = Sender::Instance();
    uint32_t nowTime = time(nullptr);
    const size_t maxCount = INT32_FLAG(merge_log_count_limit) / 4;
    for (size_t beginIndex = 0; beginIndex < logs.size(); beginIndex += maxCount) {
        size_t endIndex = beginIndex + maxCount;
        if (endIndex > logs.size()) {
            endIndex = logs.size();
        }
        sls_logs::LogGroup logGroup;
        sls_logs::LogTag* logTagPtr = logGroup.add_logtags();
        logTagPtr->set_key(LOG_RESERVED_KEY_HOSTNAME);
        logTagPtr->set_value(LogFileProfiler::mHostname.substr(0, 99));
        std::string userDefinedId = ConfigManager::GetInstance()->GetUserDefinedIdSet();
        if (!userDefinedId.empty()) {
            logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key(LOG_RESERVED_KEY_USER_DEFINED_ID);
            logTagPtr->set_value(userDefinedId.substr(0, 99));
        }
        logGroup.set_category(config->mCategory);
        logGroup.set_source(LogFileProfiler::mIpAddr);
        if (!config->mGroupTopic.empty()) {
            logGroup.set_topic(config->mGroupTopic);
        }
        for (size_t i = beginIndex; i < endIndex; ++i) {
            sls_logs::Log* log = logGroup.add_logs();
            log->mutable_contents()->CopyFrom(*(logs[i].mutable_contents()));
            log->set_time(nowTime);
        }
        if (!sSenderInstance->Send(config->mProjectName,
                                   "",
                                   logGroup,
                                   config,
                                   config->mMergeType,
                                   (uint32_t)((endIndex - beginIndex) * 1024))) {
            LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                   "push observer data into batch map fail",
                                                   config->mProjectName,
                                                   config->mCategory,
                                                   config->mRegion);
            LOG_ERROR(sLogger,
                      ("push observer data into batch map fail, discard logs",
                       logGroup.logs_size())("project", config->mProjectName)("logstore", config->mCategory));
            return -1;
        }
    }
    return 0;
}
} // namespace logtail