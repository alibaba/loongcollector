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

DECLARE_FLAG_INT32(system_interface_default_cache_ttl);

namespace logtail {

struct BaseInformation {
    std::chrono::steady_clock::time_point collectTime;
};

struct SystemInformation : public BaseInformation {
    int64_t bootTime;
};

class ScopeGuard {
    std::function<void()> fn;

public:
    explicit ScopeGuard(std::function<void()> f) : fn(std::move(f)) {}

    ~ScopeGuard() { fn(); }
};

#define defer3(ln, statement) ScopeGuard __##ln##_defer_([&]() { statement; })
#define defer2(ln, statement) defer3(ln, statement)
#define defer(statement) defer2(__LINE__, statement)

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

struct MemoryStat {
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

    static inline const FieldName<MemoryStat> memStatMetas[] = {
        FIELD_ENTRY(MemoryStat, ram),
        FIELD_ENTRY(MemoryStat, total),
        FIELD_ENTRY(MemoryStat, used),
        FIELD_ENTRY(MemoryStat, free),
        FIELD_ENTRY(MemoryStat, available),
        FIELD_ENTRY(MemoryStat, actualUsed),
        FIELD_ENTRY(MemoryStat, actualFree),
        FIELD_ENTRY(MemoryStat, buffers),
        FIELD_ENTRY(MemoryStat, cached),
        FIELD_ENTRY(MemoryStat, usedPercent),
        FIELD_ENTRY(MemoryStat, freePercent),
    };

    static void enumerate(const std::function<void(const FieldName<MemoryStat>&)>& callback) {
        for (const auto& field : memStatMetas) {
            callback(field);
        }
    }
};

struct MemoryInformation : public BaseInformation {
    MemoryStat memStat;
};

enum FileSystemType {
    FILE_SYSTEM_TYPE_UNKNOWN = 0,
    FILE_SYSTEM_TYPE_NONE,
    FILE_SYSTEM_TYPE_LOCAL_DISK,
    FILE_SYSTEM_TYPE_NETWORK,
    FILE_SYSTEM_TYPE_RAM_DISK,
    FILE_SYSTEM_TYPE_CDROM,
    FILE_SYSTEM_TYPE_SWAP,
    FILE_SYSTEM_TYPE_MAX
};

struct FileSystem {
    std::string dirName;
    std::string devName;
    std::string typeName;
    std::string sysTypeName;
    std::string options;
    FileSystemType type = FILE_SYSTEM_TYPE_UNKNOWN;
    unsigned long flags = 0;
};

struct FileSystemListInformation : public BaseInformation {
    // mounted file systems
    std::vector<FileSystem> fileSystemList;
};

struct SystemUptimeInformation : public BaseInformation {
    double uptime;
};

struct SerialIdInformation : public BaseInformation {
    std::string serialId;
};

enum class EnumDiskState {
    major,
    minor,
    devName,

    reads,
    readsMerged,
    readSectors,
    rMillis,

    writes,
    writesMerged,
    writeSectors,
    wMillis,

    ioCount,
    rwMillis, // 输入输出花费的毫秒数
    qMillis, // 输入/输出操作花费的加权毫秒数

    count, // 这个用于收尾，不是实际的列号。
};
static_assert((int)EnumDiskState::count == 14, "EnumDiskState::count unexpected");

struct DiskState {
    unsigned int major;
    unsigned int minor;

    uint64_t reads;
    uint64_t readBytes;
    uint64_t rTime;

    uint64_t writes;
    uint64_t writeBytes;
    uint64_t wTime;

    uint64_t time; // 输入输出花费的毫秒数
    uint64_t qTime; // 输入/输出操作花费的加权毫秒数
};

struct DiskStateInformation : public BaseInformation {
    std::vector<DiskState> diskStats;
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
    bool GetFileSystemListInformation(FileSystemListInformation& fileSystemListInfo);
    bool GetSystemUptimeInformation(SystemUptimeInformation& systemUptimeInfo);
    bool GetDiskSerialIdInformation(std::string diskName, SerialIdInformation& serialIdInfo);
    bool GetDiskStateInformation(DiskStateInformation& diskStateInfo);

    explicit SystemInterface(std::chrono::milliseconds ttl
                             = std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)})
        : mSystemInformationCache(),
          mCPUInformationCache(ttl),
          mProcessListInformationCache(ttl),
          mProcessInformationCache(ttl),
          mSystemLoadInformationCache(ttl),
          mCPUCoreNumInformationCache(ttl),
          mMemInformationCache(ttl),
          mFileSystemListInformationCache(ttl),
          mSystemUptimeInformationCache(ttl),
          mSerialIdInformationCache(ttl),
          mDiskStateInformationCache(ttl) {}

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
    virtual bool GetFileSystemListInformationOnce(FileSystemListInformation& fileSystemListInfo) = 0;
    virtual bool GetSystemUptimeInformationOnce(SystemUptimeInformation& systemUptimeInfo) = 0;
    virtual bool GetDiskSerialIdInformationOnce(std::string diskName, SerialIdInformation& serialIdInfo) = 0;
    virtual bool GetDiskStateInformationOnce(DiskStateInformation& diskStateInfo) = 0;

    SystemInformation mSystemInformationCache;
    SystemInformationCache<CPUInformation> mCPUInformationCache;
    SystemInformationCache<ProcessListInformation> mProcessListInformationCache;
    SystemInformationCache<ProcessInformation, pid_t> mProcessInformationCache;
    SystemInformationCache<SystemLoadInformation> mSystemLoadInformationCache;
    SystemInformationCache<CpuCoreNumInformation> mCPUCoreNumInformationCache;
    SystemInformationCache<MemoryInformation> mMemInformationCache;
    SystemInformationCache<FileSystemListInformation> mFileSystemListInformationCache;
    SystemInformationCache<SystemUptimeInformation> mSystemUptimeInformationCache;
    SystemInformationCache<SerialIdInformation, std::string> mSerialIdInformationCache;
    SystemInformationCache<DiskStateInformation> mDiskStateInformationCache;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SystemInterfaceUnittest;
#endif
};

} // namespace logtail
