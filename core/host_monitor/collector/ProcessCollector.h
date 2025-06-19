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

using namespace std::chrono;

enum class EnumProcessStat : int {
    pid,                   // 0
    comm,                  // 1
    state,                 // 2
    ppid,                  // 3
    pgrp,                  // 4
    session,               // 5
    tty_nr,                // 6
    tpgid,                 // 7
    flags,                 // 8
    minflt,                // 9
    cminflt,               // 10
    majflt,                // 11
    cmajflt,               // 12
    utime,                 // 13
    stime,                 // 14
    cutime,                // 15
    cstime,                // 16
    priority,              // 17
    nice,                  // 18
    num_threads,           // 19
    itrealvalue,           // 20
    starttime,             // 21
    vsize,                 // 22
    rss,                   // 23
    rsslim,                // 24
    startcode,             // 25
    endcode,               // 26
    startstack,            // 27
    kstkesp,               // 28
    kstkeip,               // 29
    signal,                // 30
    blocked,               // 31
    sigignore,             // 32
    sigcatch,              // 33
    wchan,                 // 34
    nswap,                 // 35
    cnswap,                // 36
    exit_signal,           // 37
    processor,             // 38 <--- 至少需要有该字段
    rt_priority,           // 39
    policy,                // 40
    delayacct_blkio_ticks, // 41
    guest_time,            // 42
    cguest_time,           // 43
    start_data,            // 44
    end_data,              // 45
    start_brk,             // 46
    arg_start,             // 47
    arg_end,               // 48
    env_start,             // 49
    env_end,               // 50
    exit_code,             // 51

    _count, // 只是用于计数，非实际字段
};

static_assert((int) EnumProcessStat::comm == 1, "EnumProcessStat invalid");
static_assert((int) EnumProcessStat::processor == 38, "EnumProcessStat invalid");

constexpr int operator-(EnumProcessStat a, EnumProcessStat b) {
    return (int) a - (int) b;
}

namespace logtail {

struct tagPidTotal {
    pid_t pid = 0;
    uint64_t total = 0;

    tagPidTotal() = default;

    tagPidTotal(pid_t p, uint64_t t) : pid(p), total(t) {}
};

// 单进程CPU信息
struct ProcessCpuInformation {
    int64_t startTime = 0;
    std::chrono::steady_clock::time_point lastTime;
    uint64_t user = 0;
    uint64_t sys = 0;
    uint64_t total = 0;
    double percent = 0.0;
};

struct LinuxProcessInfo {
    pid_t pid = 0;
    // time_t mTime = 0;
    uint64_t vSize = 0;
    uint64_t rss = 0;
    uint64_t minorFaults = 0;
    uint64_t majorFaults = 0;
    pid_t parentPid = 0;
    int tty = 0;
    int priority = 0;
    int nice = 0;
    int numThreads = 0;
    std::chrono::system_clock::time_point startTime;
    std::chrono::milliseconds utime{0};
    std::chrono::milliseconds stime{0};
    std::chrono::milliseconds cutime{0};
    std::chrono::milliseconds cstime{0};
    std::string name;
    char state = '\0';
    int processor = 0;

    int64_t startMillis() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count();
    }
};

struct ProcessTime {
    std::chrono::system_clock::time_point startTime;
    std::chrono::milliseconds cutime{0};
    std::chrono::milliseconds cstime{0};

    std::chrono::milliseconds user{0}; // utime + cutime
    std::chrono::milliseconds sys{0}; // stime + cstime

    std::chrono::milliseconds total{0}; // user + sys

    std::chrono::milliseconds utime() const {
        return user - cutime;
    }

    std::chrono::milliseconds stime() const {
        return sys - cstime;
    }

    int64_t startMillis() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count();
    }
};

struct ProcessCpuInformationCache {
    ProcessCpuInformation processCpu;
    std::chrono::steady_clock::time_point expireTime;
};

struct CpuInformationCache {
    pid_t pid;
    std::chrono::steady_clock::time_point recordTimestamp;
    uint16_t cpuTime;
};

struct ProcessInfo {
        pid_t pid;
        std::string name;
        std::string path;
        std::string cwd;
        std::string root;
        std::string args;
        std::string user;
};

struct ProcessMemoryInformation {
    uint64_t size = 0;
    uint64_t resident = 0;
    uint64_t share = 0;
    uint64_t minorFaults = 0;
    uint64_t majorFaults = 0;
    uint64_t pageFaults = 0;
};

// 进程打开文件数
struct ProcessFd {
    uint64_t total = 0;
    bool exact = true;  // total是否是一个精确值，在Linux下进程打开文件数超10,000时，将不再继续统计，以防出现性能问题
};

struct ProcessCredName {
    std::string user;
    std::string group;
    std::string name;
    uid_t uid;   //real user ID
    gid_t gid;   //real group ID
    uid_t euid;  //effective user ID
    gid_t egid;  //effective group ID
};

struct ProcessCred {
    uid_t uid;   //real user ID
    gid_t gid;   //real group ID
    uid_t euid;  //effective user ID
    gid_t egid;  //effective group ID
};

struct ProcessAllStat {
    pid_t pid;
    ProcessStat processState;
    ProcessInfo processInfo;
    ProcessCpuInformation processCpu;
    ProcessMemoryInformation processMemory;
    double memPercent = 0.0;
    uint64_t fdNum = 0;
    bool fdNumExact = true;
};

struct ProcessPushMertic {
    pid_t pid;
    std::string name;
    std::string user;
    std::string path;
    std::string args;
    double cpuPercent = 0.0;
    double memPercent = 0.0;
    double fdNum = 0.0;
    double numThreads  = 0.0;
    double allNumProcess = 0.0;

    static inline const FieldName<ProcessPushMertic> processPushMerticFields[] = {
        FIELD_ENTRY(ProcessPushMertic, cpuPercent),
        FIELD_ENTRY(ProcessPushMertic, memPercent),
        FIELD_ENTRY(ProcessPushMertic, fdNum),
        FIELD_ENTRY(ProcessPushMertic, numThreads),
        FIELD_ENTRY(ProcessPushMertic, allNumProcess),
    };

    static void enumerate(const std::function<void(const FieldName<ProcessPushMertic, double>&)>& callback) {
        for (const auto& field : processPushMerticFields) {
            callback(field);
        }
    }
};

struct VMProcessNumStat {
    double vmProcessNum = 0;

    static inline const FieldName<VMProcessNumStat> vmProcessNumStatMerticFields[] = {
        FIELD_ENTRY(VMProcessNumStat, vmProcessNum),
    };

    static void enumerate(const std::function<void(const FieldName<VMProcessNumStat, double>&)>& callback) {
        for (const auto& field : vmProcessNumStatMerticFields) {
            callback(field);
        }
    }
};

struct SystemTaskInfo {
    uint64_t threadCount = 0;
    uint64_t processCount = 0;
    uint64_t zombieProcessCount = 0;
};

class ProcessCollector : public BaseCollector {
public:
    ProcessCollector();

    int Init(int totalCount=3);

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

    int ReadProcessStat(pid_t pid, LinuxProcessInfo &processInfo);

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
    int mTotalCount = 0;
    int mCount = 0;
    std::vector<pid_t> pids;
    std::vector<pid_t> mSortPids;
    int mSelfPid = 0;
    int mParentPid = 0;
    uint64_t mTotalMemory = 0;
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
