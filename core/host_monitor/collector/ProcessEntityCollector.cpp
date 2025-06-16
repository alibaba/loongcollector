/*
 * Copyright 2024 iLogtail Authors
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

#include "host_monitor/collector/ProcessEntityCollector.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <sched.h>
#include <unistd.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/MachineInfoUtil.h"
#include "common/ProcParser.h"
#include "common/StringView.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"

DEFINE_FLAG_INT32(process_collect_silent_count, "number of process scanned between a sleep", 1000);

namespace logtail {

const size_t ProcessTopN = 20;

const std::string ProcessEntityCollector::sName = "process_entity";

ProcessEntityCollector::ProcessEntityCollector()
    : mProcParser(""), mProcessSilentCount(INT32_FLAG(process_collect_silent_count)) {
}

system_clock::time_point ProcessEntityCollector::TicksToUnixTime(int64_t startTicks) {
    SystemInformation systemInfo;
    if (!SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
        LOG_ERROR(sLogger, ("failed to get system information", "use current time instead"));
        return system_clock::now();
    }
    return system_clock::time_point{static_cast<milliseconds>(startTicks) + milliseconds{systemInfo.bootTime * 1000}};
}

bool ProcessEntityCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig,
                                     PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    SystemInformation systemInfo;
    if (!SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
        LOG_ERROR(sLogger, ("failed to get system information", "use current time instead"));
        systemInfo.bootTime = 0;
    }
    std::vector<ExtendedProcessStatPtr> processes;
    GetSortedProcess(processes, ProcessTopN);
    for (const auto& extentedProcess : processes) {
        auto process = extentedProcess->stat;
        auto* event = group->AddLogEvent();
        time_t logtime = time(nullptr);
        event->SetTimestamp(logtime);

        auto startTime = system_clock::time_point{static_cast<milliseconds>(process.startTicks)
                                                  + milliseconds{systemInfo.bootTime * 1000}};

        std::string processCreateTime = std::to_string(duration_cast<seconds>(startTime.time_since_epoch()).count());

        // common fields
        std::string domain;
        std::string entityType;
        std::string hostEntityType;
        StringView hostEntityID;
        FetchDomainInfo(domain, entityType, hostEntityType, hostEntityID);
        event->SetContent(DEFAULT_CONTENT_KEY_DOMAIN, domain);
        event->SetContent(DEFAULT_CONTENT_KEY_ENTITY_TYPE, entityType);
        auto entityID = GetProcessEntityID(std::to_string(process.pid), processCreateTime, hostEntityID);
        event->SetContent(DEFAULT_CONTENT_KEY_ENTITY_ID, entityID);

        event->SetContent(DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME, processCreateTime);
        event->SetContent(DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME, std::to_string(logtime));
        int keepAliveSeconds = collectConfig.mInterval.count() * 2;
        event->SetContent(DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS, std::to_string(keepAliveSeconds));

        // custom fields
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PID, std::to_string(process.pid));
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID, std::to_string(process.parentPid));
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_COMM, process.name);
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_KTIME, processCreateTime);
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_USER, ""); TODO: get user name
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CWD, ""); TODO: get cwd
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_BINARY, ""); TODO: get binary
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS, ""); TODO: get arguments
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE, ""); TODO: get language
        // event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID, ""); TODO: get container id

        // process -> host link
        auto* linkEvent = group->AddLogEvent();
        linkEvent->SetTimestamp(logtime);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_SRC_DOMAIN, domain);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_SRC_ENTITY_TYPE, entityType);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_SRC_ENTITY_ID, entityID);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_DEST_DOMAIN, domain);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_DEST_ENTITY_TYPE, hostEntityType);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_DEST_ENTITY_ID, hostEntityID);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_RELATION_TYPE, DEFAULT_CONTENT_VALUE_METHOD_UPDATE);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_RELATION_TYPE, DEFAULT_CONTENT_VALUE_METHOD_UPDATE);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME, processCreateTime);
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME, std::to_string(logtime));
        linkEvent->SetContent(DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS, std::to_string(keepAliveSeconds));
    }
    return true;
}

void ProcessEntityCollector::GetSortedProcess(std::vector<ExtendedProcessStatPtr>& processStats, size_t topN) {
    steady_clock::time_point now = steady_clock::now();
    auto compare = [](const std::pair<ExtendedProcessStatPtr, double>& a,
                      const std::pair<ExtendedProcessStatPtr, double>& b) { return a.second > b.second; };
    std::priority_queue<std::pair<ExtendedProcessStatPtr, double>,
                        std::vector<std::pair<ExtendedProcessStatPtr, double>>,
                        decltype(compare)>
        queue(compare);

    int readCount = 0;
    std::unordered_map<pid_t, ExtendedProcessStatPtr> newProcessStat;
    ProcessListInformation processListInfo;
    if (!SystemInterface::GetInstance()->GetProcessListInformation(processListInfo)) {
        LOG_ERROR(sLogger, ("failed to get process list information", "skip collect"));
        return;
    }

    for (const auto& pid : processListInfo.pids) {
        if (pid == 0) {
            continue;
        }
        if (++readCount > mProcessSilentCount) {
            readCount = 0;
            std::this_thread::sleep_for(milliseconds{100});
        }
        bool isFirstCollect = false;
        auto ptr = GetProcessStat(pid, isFirstCollect);
        if (ptr == nullptr) {
            continue;
        }
        newProcessStat[pid] = ptr;
        if (!isFirstCollect) {
            queue.emplace(ptr, ptr->cpuInfo.percent);
        }
        if (queue.size() > topN) {
            queue.pop();
        }
    }

    processStats.clear();
    processStats.reserve(queue.size());
    while (!queue.empty()) {
        processStats.push_back(queue.top().first);
        queue.pop();
    }
    std::reverse(processStats.begin(), processStats.end());

    if (processStats.empty()) {
        LOG_INFO(sLogger, ("first collect Process Cpu info", "empty"));
    }
    LOG_DEBUG(sLogger, ("collect Process Cpu info, top", processStats.size()));

    mPrevProcessStat = std::move(newProcessStat);
    mProcessSortTime = now;
}

ExtendedProcessStatPtr ProcessEntityCollector::GetProcessStat(pid_t pid, bool& isFirstCollect) {
    const auto now = steady_clock::now();

    // TODO: more accurate cache
    auto prev = mPrevProcessStat.find(pid);
    if (prev == mPrevProcessStat.end() || prev->second == nullptr
        || prev->second->lastStatTime.time_since_epoch().count() == 0) {
        isFirstCollect = true;
    } else {
        isFirstCollect = false;
    }
    // proc/[pid]/stat的统计粒度通常为10ms，两次采样之间需要足够大才能平滑。
    if (prev != mPrevProcessStat.end() && prev->second && now < prev->second->lastStatTime + seconds{1}) {
        return prev->second;
    }
    auto ptr = std::make_shared<ExtendedProcessStat>();
    ProcessInformation processInfo;
    if (SystemInterface::GetInstance()->GetProcessInformation(pid, processInfo)) {
        ptr->stat = processInfo.stat;
    } else {
        LOG_ERROR(sLogger, ("failed to get process information", pid));
        return nullptr;
    }

    // calculate CPU related fields
    {
        ptr->lastStatTime = now;
        constexpr const uint64_t MILLISECOND = 1000;
        ptr->cpuInfo.user = (ptr->stat.utimeTicks + ptr->stat.cutimeTicks) * MILLISECOND / SYSTEM_HERTZ;
        ptr->cpuInfo.sys = (ptr->stat.stimeTicks + ptr->stat.cstimeTicks) * MILLISECOND / SYSTEM_HERTZ;
        ptr->cpuInfo.total = ptr->cpuInfo.user + ptr->cpuInfo.sys;
        if (isFirstCollect || ptr->cpuInfo.total <= prev->second->cpuInfo.total) {
            // first time called
            ptr->cpuInfo.percent = 0.0;
        } else {
            auto totalDiff = static_cast<double>(ptr->cpuInfo.total - prev->second->cpuInfo.total);
            auto timeDiff = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(ptr->lastStatTime.time_since_epoch()
                                                                      - prev->second->lastStatTime.time_since_epoch())
                    .count());
            ptr->cpuInfo.percent = totalDiff / timeDiff;
        }
    }
    return ptr;
}

std::string ProcessEntityCollector::GetProcessEntityID(StringView pid, StringView createTime, StringView hostEntityID) {
    std::ostringstream oss;
    oss << hostEntityID << pid << createTime;
    auto bigID = CalcMD5(oss.str());
    std::transform(bigID.begin(), bigID.end(), bigID.begin(), ::tolower);
    return bigID;
}

void ProcessEntityCollector::FetchDomainInfo(std::string& domain,
                                             std::string& entityType,
                                             std::string& hostEntityType,
                                             StringView& hostEntityID) {
    auto entity = InstanceIdentity::Instance()->GetEntity();
    if (entity != nullptr) {
        ECSMeta meta = entity->GetECSMeta();
        if (meta.GetInstanceID().empty()) {
            domain = DEFAULT_VALUE_DOMAIN_INFRA;
            hostEntityType = DEFAULT_HOST_TYPE_HOST;
        } else {
            domain = DEFAULT_VALUE_DOMAIN_ACS;
            hostEntityType = DEFAULT_HOST_TYPE_ECS;
        }
        entityType = DEFAULT_CONTENT_VALUE_ENTITY_TYPE_ECS_PROCESS;
        hostEntityID = entity->GetHostID();
    }
}

} // namespace logtail
