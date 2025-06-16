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
#include <vector>
#include <boost/filesystem.hpp>
#include <filesystem>

#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "common/ProcParser.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"

using namespace std::chrono;

namespace logtail {

class ProcessCollector : public BaseCollector {
public:
    ProcessCollector();

    int Init(int processTotalCount,int processReportTopN);

    ~ProcessCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;

    static const int mProcessSilentCount = 1000;
    const std::string& Name() const override { return sName; }

public:
    bool GetPids(std::vector<pid_t>& pids);

    bool WalkAllDigitDirs(const std::filesystem::path& root, const std::function<void(const std::string&)>& callback);

    void RemovePid(std::vector<pid_t> &pids, pid_t pid, pid_t ppid);

    void GetSelfPid(pid_t &pid, pid_t &ppid);

    int GetProcessTime(pid_t pid, ProcessTime &output, bool includeCTime);

    int ReadProcessStat(pid_t pid, ProcessStat &processStat);

    int GetPidsCpu(const std::vector<pid_t> &pids, std::map<pid_t, uint64_t> &pidMap);

    int GetTopNProcessStat(std::vector <pid_t> &sortPids, int topN, std::vector<ProcessAllStat> &processAllStats);

    int GetProcessAllStat(pid_t pid, ProcessAllStat &processStat);

    int GetProcessMemory(pid_t pid, ProcessMemoryInformation &processMemory);

    int GetProcessFdNumber(pid_t pid, ProcessFd &processFd);

    int GetProcessInfo(pid_t pid, ProcessInfo &processInfo);

    int GetProcessCredName(pid_t pid,ProcessCredName &processCredName);

    int GetProcessArgs(pid_t pid, std::vector<std::string> &args);

    int GetProcessState(pid_t pid, ProcessStat &processState);

    int CountNumsDir(const std::filesystem::path& root, ProcessFd &processFd);

    double GetSysHz();

    std::string GetExecutablePath(pid_t pid);

protected:

    int GetProcessCpuInformation(pid_t pid, ProcessCpuInformation &information,bool includeCTime);

    bool GetProcessCpuInCache(pid_t pid, bool includeCTime);

private:
    int mCountPerReport = 0;
    int mCount = 0;
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
    std::map<pid_t, ProcessCpuInformation> cpuTimeCache;
    std::map<pid_t, MetricCalculate<ProcessPushMertic>> mProcessPushMertic; //记录每个pid对应的多值体系
    MetricCalculate<VMProcessNumStat> mVMProcessNumStat;
    std::map<pid_t, double> mAvgProcessCpuPercent;
    std::map<pid_t, double> mAvgProcessMemPercent;
    std::map<pid_t, double> mAvgProcessFd;
    std::map<pid_t, double> mAvgProcessNumThreads;
    std::map<pid_t, double> mMinProcessNumThreads;
    std::map<pid_t, double> mMaxProcessNumThreads;
    std::map<pid_t, std::string> pidNameMap;
};

} // namespace logtail
