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

#pragma once

#include <sched.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "collector/MetricCalculate.h"
#include "common/Flags.h"
#include "common/ProcParser.h"
#include "collector/MetricCalculate.h"

DECLARE_FLAG_INT32(system_interface_default_cache_ttl);

namespace logtail {

struct BaseInformation {
    std::chrono::steady_clock::time_point collectTime;
};

struct SystemInformation : public BaseInformation {
    int64_t bootTime;
};

// man proc: https://man7.org/linux/man-pages/man5/proc.5.html
// search key: /proc/stat
enum class EnumCpuKey : int {
    user = 1,
    nice,
    system,
    idle,
    iowait, // since Linux 2.5.41
    irq, // since Linux 2.6.0
    softirq, // since Linux 2.6.0
    steal, // since Linux 2.6.11
    guest, // since Linux 2.6.24
    guest_nice, // since Linux 2.6.33
};

struct CPUStat {
    int32_t index; // -1 means total cpu
    double user;
    double nice;
    double system;
    double idle;
    double iowait;
    double irq;
    double softirq;
    double steal;
    double guest;
    double guestNice;
};

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

struct ProcessTime {
    int64_t startTime;
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

struct ProcessMemoryInformation : public BaseInformation {
    uint64_t size = 0;
    uint64_t resident = 0;
    uint64_t share = 0;
    uint64_t minorFaults = 0;
    uint64_t majorFaults = 0;
    uint64_t pageFaults = 0;
};

// 进程打开文件数
struct ProcessFd : public BaseInformation {
    uint64_t total = 0;
    bool exact = true;  // total是否是一个精确值，在Linux下进程打开文件数超10,000时，将不再继续统计，以防出现性能问题
};

struct ProcessCredName : public BaseInformation {
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

struct CPUInformation : public BaseInformation {
    std::vector<CPUStat> stats;
};

struct ProcessListInformation : public BaseInformation {
    std::vector<pid_t> pids;
};

struct ProcessInformation : public BaseInformation {
    ProcessStat stat; // shared data structrue with eBPF process
};

// /proc/loadavg
struct SystemStat {
    double load1;
    double load5;
    double load15;
    double load1PerCore;
    double load5PerCore;
    double load15PerCore;

    // Define the field descriptors
    static inline const FieldName<SystemStat> systemMetricFields[] = {
        FIELD_ENTRY(SystemStat, load1),
        FIELD_ENTRY(SystemStat, load5),
        FIELD_ENTRY(SystemStat, load15),
        FIELD_ENTRY(SystemStat, load1PerCore),
        FIELD_ENTRY(SystemStat, load5PerCore),
        FIELD_ENTRY(SystemStat, load15PerCore),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<SystemStat, double>&)>& callback) {
        for (const auto& field : systemMetricFields) {
            callback(field);
        }
    }
};

struct SystemLoadInformation : public BaseInformation {
    SystemStat systemStat;
};

struct CpuCoreNumInformation : public BaseInformation {
    unsigned int cpuCoreNum;
};

struct ProcessExecutePath : public BaseInformation {
    std::string path;
};

struct TupleHash {
    template <typename... T>
    std::size_t operator()(const std::tuple<T...>& t) const {
        size_t seed = 0;
        std::apply(
            [&](const T&... args) { ((seed ^= std::hash<T>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...); },
            t);
        return seed;
    }
};

struct MemoryInformationString : public BaseInformation {
    std::vector<std::string> meminfoString;
};

struct MTRRInformationString : public BaseInformation {
    std::vector<std::string> mtrrString;
};

// /proc/pid/status 
struct ProcessStatusString : public BaseInformation {
    std::vector<std::string> processStatusString;
};

// /proc/pid/cmdline
struct ProcessCmdlineString : public BaseInformation {
    std::vector<std::string> cmdline;
};

// /proc/pid/statm
struct ProcessStatmString : public BaseInformation {
    std::vector<std::string> processStatmString;
};

struct MemoryInformation {
    double ram = 0;
    double total = 0;
    double used = 0;
    double free = 0;
    double available = 0;
    double actualUsed = 0;
    double actualFree = 0;
    double buffers = 0;
    double cached = 0;
    double usedPercent = 0.0;
    double freePercent = 0.0;

    static inline const FieldName<MemoryInformation> memStatMetas[] = {
        FIELD_ENTRY(MemoryInformation, ram),
        FIELD_ENTRY(MemoryInformation, total),
        FIELD_ENTRY(MemoryInformation, used),
        FIELD_ENTRY(MemoryInformation, free),
        FIELD_ENTRY(MemoryInformation, available),
        FIELD_ENTRY(MemoryInformation, actualUsed),
        FIELD_ENTRY(MemoryInformation, actualFree),
        FIELD_ENTRY(MemoryInformation, buffers),
        FIELD_ENTRY(MemoryInformation, cached),
        FIELD_ENTRY(MemoryInformation, usedPercent),
        FIELD_ENTRY(MemoryInformation, freePercent),
    };

    static void enumerate(const std::function<void(const FieldName<MemoryInformation>&)>& callback) {
        for (const auto& field : memStatMetas) {
            callback(field);
        }
    }
};

struct MemoryInformation : public BaseInformation {
    MemoryStat memStat;
};

struct SwapInformation {
    double total = 0;
    double used = 0;
    double free = 0;
    double pageIn = 0;
    double pageOut = 0;

    static inline const FieldName<SwapInformation> swapStatMetas[] = {
        FIELD_ENTRY(SwapInformation, total),
        FIELD_ENTRY(SwapInformation, used),
        FIELD_ENTRY(SwapInformation, used),
        FIELD_ENTRY(SwapInformation, pageIn),
        FIELD_ENTRY(SwapInformation, pageOut),
    };

    static void enumerate(const std::function<void(const FieldName<SwapInformation>&)>& callback) {
        for (const auto& field : swapStatMetas) {
            callback(field);
        }
    }
};

class SystemInterface {
public:
    template <typename InfoT, typename... Args>
    class SystemInformationCache {
    public:
        SystemInformationCache(std::chrono::milliseconds ttl) : mTTL(ttl) {}
        bool GetWithTimeout(InfoT& info, std::chrono::milliseconds timeout, Args... args);
        bool Set(InfoT& info, Args... args);
        bool GC();

    private:
        std::mutex mMutex;
        std::unordered_map<std::tuple<Args...>, std::pair<InfoT, std::atomic_bool>, TupleHash> mCache;
        std::condition_variable mConditionVariable;
        std::chrono::milliseconds mTTL;

#ifdef APSARA_UNIT_TEST_MAIN
        friend class SystemInterfaceUnittest;
#endif
    };

    template <typename InfoT>
    class SystemInformationCache<InfoT> {
    public:
        SystemInformationCache(std::chrono::milliseconds ttl) : mTTL(ttl) {}
        bool GetWithTimeout(InfoT& info, std::chrono::milliseconds timeout);
        bool Set(InfoT& info);
        bool GC();

    private:
        std::mutex mMutex;
        std::pair<InfoT, std::atomic_bool> mCache;
        std::condition_variable mConditionVariable;
        std::chrono::milliseconds mTTL;

#ifdef APSARA_UNIT_TEST_MAIN
        friend class SystemInterfaceUnittest;
#endif
    };

    SystemInterface(const SystemInterface&) = delete;
    SystemInterface(SystemInterface&&) = delete;
    SystemInterface& operator=(const SystemInterface&) = delete;
    SystemInterface& operator=(SystemInterface&&) = delete;

    static SystemInterface* GetInstance();

    bool GetSystemInformation(SystemInformation& systemInfo);
    bool GetCPUInformation(CPUInformation& cpuInfo);
    bool GetProcessListInformation(ProcessListInformation& processListInfo);
    bool GetProcessInformation(pid_t pid, ProcessInformation& processInfo);
    bool GetSystemLoadInformation(SystemLoadInformation& systemLoadInfo);
    bool GetCPUCoreNumInformation(CpuCoreNumInformation& cpuCoreNumInfo);
    bool GetHostMemInformationStat(MemoryInformation& meminfo);
    bool GetHostMeminfoStatString(MemoryInformationString& meminfoString);
    bool GetMTRRInformationString(MTRRInformationString& mtrrString);
    bool GetProcessCmdlineString(pid_t pid, ProcessCmdlineString& cmdline);
    bool GetPorcessStatm(pid_t pid, ProcessMemoryInformation &processMemory);
    bool GetProcessCredNameObj(pid_t pid, ProcessCredName &credName);
    bool GetExecutablePathCache(pid_t pid, ProcessExecutePath &executePath);
    bool GetProcessOpenFiles(pid_t pid, ProcessFd &processFd);

    explicit SystemInterface(std::chrono::milliseconds ttl
                             = std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)})
        : mSystemInformationCache(),
          mCPUInformationCache(ttl),
          mProcessListInformationCache(ttl),
          mProcessInformationCache(ttl),
          mSystemLoadInformationCache(ttl),
          mCPUCoreNumInformationCache(ttl),
          mMemInformationCache(ttl),
          mMTRRInformationCache(ttl),
          mProcessCmdlineCache(ttl),
          mProcessStatmCache(ttl),
          mProcessStatusCache(ttl),
          mProcessFdCache(ttl),
          mExecutePathCache(ttl) {}

    virtual ~SystemInterface() = default;

private:
    template <typename F, typename InfoT, typename... Args>
    bool MemoizedCall(SystemInformationCache<InfoT, Args...>& cache,
                      F&& func,
                      InfoT& info,
                      const std::string& errorType,
                      Args... args);

    virtual bool GetSystemInformationOnce(SystemInformation& systemInfo) = 0;
    virtual bool GetCPUInformationOnce(CPUInformation& cpuInfo) = 0;
    virtual bool GetProcessListInformationOnce(ProcessListInformation& processListInfo) = 0;
    virtual bool GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) = 0;
    virtual bool GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) = 0;
    virtual bool GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) = 0;
    virtual bool GetHostMemInformationStatOnce(MemoryInformation& meminfoStr) = 0;
    virtual bool GetMemoryInformationStringOnce(MemoryInformationString& meminfoStr) = 0;
    virtual bool GetMTRRInformationStringOnce(MTRRInformationString& mtrrStr) = 0;
    virtual bool GetProcessCmdlineStringOnce(pid_t pid, ProcessCmdlineString& cmdline) = 0;
    virtual bool GetProcessStatmOnce(pid_t pid, ProcessMemoryInformation& processMemory) = 0;
    virtual bool GetProcessCredNameOnce(pid_t pid, ProcessCredName& processCredName) = 0;
    virtual bool GetExecutablePathOnce(pid_t pid, ProcessExecutePath &executePath) = 0;
    virtual bool GetProcessOpenFilesOnce(pid_t pid, ProcessFd &processFd) = 0;

    SystemInformation mSystemInformationCache;
    SystemInformationCache<CPUInformation> mCPUInformationCache;
    SystemInformationCache<ProcessListInformation> mProcessListInformationCache;
    SystemInformationCache<ProcessInformation, pid_t> mProcessInformationCache;
    SystemInformationCache<SystemLoadInformation> mSystemLoadInformationCache;
    SystemInformationCache<CpuCoreNumInformation> mCPUCoreNumInformationCache;
    SystemInformationCache<MemoryInformation> mMemInformationCache;
    SystemInformationCache<MemoryInformationString> mMemInformationCache;
    SystemInformationCache<MTRRInformationString> mMTRRInformationCache;
    SystemInformationCache<ProcessCmdlineString, pid_t> mProcessCmdlineCache;
    SystemInformationCache<ProcessMemoryInformation, pid_t> mProcessStatmCache;
    SystemInformationCache<ProcessCredName, pid_t> mProcessStatusCache;
    SystemInformationCache<ProcessFd, pid_t> mProcessFdCache;
    SystemInformationCache<ProcessExecutePath, pid_t> mExecutePathCache;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SystemInterfaceUnittest;
#endif
};

} // namespace logtail
