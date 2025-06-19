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

struct CPUInformation : public BaseInformation {
    std::vector<CPUStat> stats;
};

struct ProcessListInformation : public BaseInformation {
    std::vector<pid_t> pids;
};

struct ProcessInformation : public BaseInformation {
    ProcessStat stat; // shared data structrue with eBPF process
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
    bool GetHostMeminfoStatString(MemoryInformationString& meminfoString);
    bool GetMTRRInformationString(MTRRInformationString& mtrrString);

    explicit SystemInterface(std::chrono::milliseconds ttl
                             = std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)})
        : mSystemInformationCache(),
          mCPUInformationCache(ttl),
          mProcessListInformationCache(ttl),
          mProcessInformationCache(ttl),
          mMemInformationCache(ttl),
          mMTRRInformationCache(ttl) {}

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
    virtual bool GetMemoryInformationStringOnce(MemoryInformationString& meminfoStr) = 0;
    virtual bool GetMTRRInformationStringOnce(MTRRInformationString& mtrrStr) = 0;

    SystemInformation mSystemInformationCache;
    SystemInformationCache<CPUInformation> mCPUInformationCache;
    SystemInformationCache<ProcessListInformation> mProcessListInformationCache;
    SystemInformationCache<ProcessInformation, pid_t> mProcessInformationCache;
    SystemInformationCache<MemoryInformationString> mMemInformationCache;
    SystemInformationCache<MTRRInformationString> mMTRRInformationCache;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SystemInterfaceUnittest;
#endif
};

} // namespace logtail
