/*
 * Copyright 2025 iLogtail Authors
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
#include <boost/filesystem.hpp>
#include <filesystem>
#include <vector>

#include "common/ProcParser.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"

using namespace std::chrono;

namespace logtail {

class ProcessCollector : public BaseCollector {
public:
    ProcessCollector();
    ~ProcessCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;

    static const int mProcessSilentCount = 1000;
    const std::string& Name() const override { return sName; }

public:
    bool GetProcessTime(time_t now, pid_t pid, ProcessTime& output);

    bool ReadProcessStat(time_t now, pid_t pid, ProcessInformation& processInfo);

    bool GetPidsCpu(const CollectTime& collectTime, const std::vector<pid_t>& pids, std::map<pid_t, uint64_t>& pidMap);

    bool GetProcessAllStat(const CollectTime& collectTime, pid_t pid, ProcessAllStat& processStat);

    bool GetProcessMemory(time_t now, pid_t pid, ProcessMemoryInformation& processMemory);

    bool GetProcessFdNumber(time_t now, pid_t pid, ProcessFd& processFd);

    bool GetProcessInfo(time_t now, pid_t pid, ProcessInfo& processInfo);

    bool GetProcessCredName(time_t now, pid_t pid, ProcessCredName& processCredName);

    bool GetProcessArgs(time_t now, pid_t pid, std::vector<std::string>& args);

    bool GetProcessState(time_t now, pid_t pid, ProcessStat& processState);

    std::string GetExecutablePath(time_t now, pid_t pid);

protected:
    bool GetProcessCpuInformation(const CollectTime& collectTime, pid_t pid, ProcessCpuInformation& information);

    bool GetProcessCpuInCache(pid_t pid);

    void ClearProcessCpuTimeCache();

private:
    std::vector<pid_t> pids;
    std::vector<pid_t> mSortPids;
    int mSelfPid = 0;
    int mParentPid = 0;
    uint64_t mTotalMemory = 0;
    size_t mTopN = 0;
    std::chrono::steady_clock::time_point mProcessSortCollectTime;
    std::chrono::steady_clock::time_point mLastCollectSteadyTime;
    decltype(ProcessCpuInformation{}.total) mLastAgentTotalMillis = 0;
    std::shared_ptr<std::map<pid_t, uint64_t>> mLastPidCpuMap;
    std::unordered_map<pid_t, ProcessCpuInformation> cpuTimeCache;
    std::unordered_map<pid_t, MetricCalculate<ProcessPushMertic>> mProcessPushMertic; // 记录每个pid对应的多值体系
    MetricCalculate<VMProcessNumStat> mVMProcessNumStat;
    std::unordered_map<pid_t, double> mAvgProcessCpuPercent;
    std::unordered_map<pid_t, double> mAvgProcessMemPercent;
    std::unordered_map<pid_t, double> mAvgProcessFd;
    std::unordered_map<pid_t, double> mAvgProcessNumThreads;
    std::unordered_map<pid_t, double> mMinProcessNumThreads;
    std::unordered_map<pid_t, double> mMaxProcessNumThreads;
    std::unordered_map<pid_t, std::string> pidNameMap;
};

} // namespace logtail
