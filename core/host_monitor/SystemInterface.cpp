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

#include "host_monitor/SystemInterface.h"

#include <ctime>

#include <boost/asio.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <tuple>
#include <utility>

#include "boost/type_index.hpp"

#include "common/Flags.h"
#include "logger/Logger.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#ifdef __linux__
#include "host_monitor/LinuxSystemInterface.h"
#endif
#ifdef APSARA_UNIT_TEST_MAIN
#include "unittest/host_monitor/MockSystemInterface.h"
#endif

DEFINE_FLAG_INT32(system_interface_cache_queue_size, "system interface default cache size", 15);
DEFINE_FLAG_INT32(system_interface_cache_entry_expire_seconds, "cache entry expire time in seconds", 60);
DEFINE_FLAG_INT32(system_interface_cache_cleanup_interval_seconds, "cache cleanup interval in seconds", 300);
DEFINE_FLAG_INT32(system_interface_cache_max_cleanup_batch_size, "max entries to cleanup in one batch", 50);

namespace logtail {

SystemInterface* SystemInterface::GetInstance() {
#ifdef __linux__
    return LinuxSystemInterface::GetInstance();
#else
    LOG_ERROR(sLogger, "SystemInterface is not implemented for this platform");
    return nullptr;
#endif
}

bool SystemInterface::GetSystemInformation(SystemInformation& systemInfo) {
    // SystemInformation is static and will not be changed. So cache will never be expired.
    if (mSystemInformationCache.collectTime > 0) {
        systemInfo = mSystemInformationCache;
        return true;
    }
    if (GetSystemInformationOnce(mSystemInformationCache)) {
        systemInfo = mSystemInformationCache;
        return true;
    }
    return false;
}

bool SystemInterface::GetCPUInformation(time_t now, CPUInformation& cpuInfo) {
    const std::string errorType = "cpu";
    return MemoizedCall(
        mCPUInformationCache,
        now,
        [this](BaseInformation& info) { return this->GetCPUInformationOnce(static_cast<CPUInformation&>(info)); },
        cpuInfo,
        errorType);
}

bool SystemInterface::GetProcessListInformation(time_t now, ProcessListInformation& processListInfo) {
    const std::string errorType = "process list";
    return MemoizedCall(
        mProcessListInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetProcessListInformationOnce(static_cast<ProcessListInformation&>(info));
        },
        processListInfo,
        errorType);
}

bool SystemInterface::GetProcessInformation(time_t now, pid_t pid, ProcessInformation& processInfo) {
    const std::string errorType = "process";
    return MemoizedCall(
        mProcessInformationCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessInformationOnce(pid, static_cast<ProcessInformation&>(info));
        },
        processInfo,
        errorType,
        pid);
}

bool SystemInterface::GetSystemLoadInformation(time_t now, SystemLoadInformation& systemLoadInfo) {
    const std::string errorType = "system load";
    return MemoizedCall(
        mSystemLoadInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetSystemLoadInformationOnce(static_cast<SystemLoadInformation&>(info));
        },
        systemLoadInfo,
        errorType);
}

bool SystemInterface::GetCPUCoreNumInformation(CpuCoreNumInformation& cpuCoreNumInfo) {
    if (mCPUCoreNumInformationCache.collectTime > 0) {
        cpuCoreNumInfo = mCPUCoreNumInformationCache;
        return true;
    }
    if (GetCPUCoreNumInformationOnce(mCPUCoreNumInformationCache)) {
        cpuCoreNumInfo = mCPUCoreNumInformationCache;
        return true;
    }
    return false;
}

bool SystemInterface::GetHostMemInformationStat(time_t now, MemoryInformation& meminfo) {
    const std::string errorType = "mem";
    return MemoizedCall(
        mMemInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetHostMemInformationStatOnce(static_cast<MemoryInformation&>(info));
        },
        meminfo,
        errorType);
}

bool SystemInterface::GetFileSystemListInformation(time_t now, FileSystemListInformation& fileSystemListInfo) {
    const std::string errorType = "filesystem list";
    return MemoizedCall(
        mFileSystemListInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetFileSystemListInformationOnce(static_cast<FileSystemListInformation&>(info));
        },
        fileSystemListInfo,
        errorType);
}

bool SystemInterface::GetSystemUptimeInformation(time_t now, SystemUptimeInformation& systemUptimeInfo) {
    const std::string errorType = "system uptime";
    return MemoizedCall(
        mSystemUptimeInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetSystemUptimeInformationOnce(static_cast<SystemUptimeInformation&>(info));
        },
        systemUptimeInfo,
        errorType);
}

bool SystemInterface::GetDiskSerialIdInformation(time_t now, std::string diskName, SerialIdInformation& serialIdInfo) {
    const std::string errorType = "SerialId";
    return MemoizedCall(
        mSerialIdInformationCache,
        now,
        [this](BaseInformation& info, std::string diskName) {
            return this->GetDiskSerialIdInformationOnce(diskName, static_cast<SerialIdInformation&>(info));
        },
        serialIdInfo,
        errorType,
        diskName);
}

bool SystemInterface::GetDiskStateInformation(time_t now, DiskStateInformation& diskStateInfo) {
    const std::string errorType = "disk state";
    return MemoizedCall(
        mDiskStateInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetDiskStateInformationOnce(static_cast<DiskStateInformation&>(info));
        },
        diskStateInfo,
        errorType);
}

bool SystemInterface::GetFileSystemInformation(time_t now, std::string dirName, FileSystemInformation& fileSystemInfo) {
    const std::string errorType = "filesystem state";
    return MemoizedCall(
        mFileSystemInformationCache,
        now,
        [this](BaseInformation& info, std::string dirName) {
            return this->GetFileSystemInformationOnce(dirName, static_cast<FileSystemInformation&>(info));
        },
        fileSystemInfo,
        errorType,
        dirName);
}


bool SystemInterface::GetProcessCmdlineString(time_t now, pid_t pid, ProcessCmdlineString& cmdline) {
    const std::string errorType = "processCmdline";
    return MemoizedCall(
        mProcessCmdlineCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessCmdlineStringOnce(pid, static_cast<ProcessCmdlineString&>(info));
        },
        cmdline,
        errorType,
        pid);
}

bool SystemInterface::GetPorcessStatm(time_t now, pid_t pid, ProcessMemoryInformation& processMemory) {
    const std::string errorType = "processStatm";
    return MemoizedCall(
        mProcessStatmCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessStatmOnce(pid, static_cast<ProcessMemoryInformation&>(info));
        },
        processMemory,
        errorType,
        pid);
}

bool SystemInterface::GetProcessCredNameObj(time_t now, pid_t pid, ProcessCredName& credName) {
    const std::string errorType = "processStatus";
    return MemoizedCall(
        mProcessStatusCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessCredNameOnce(pid, static_cast<ProcessCredName&>(info));
        },
        credName,
        errorType,
        pid);
}

bool SystemInterface::GetExecutablePathCache(time_t now, pid_t pid, ProcessExecutePath& executePath) {
    const std::string errorType = "executablePath";
    return MemoizedCall(
        mExecutePathCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetExecutablePathOnce(pid, static_cast<ProcessExecutePath&>(info));
        },
        executePath,
        errorType,
        pid);
}

bool SystemInterface::GetProcessOpenFiles(time_t now, pid_t pid, ProcessFd& processFd) {
    const std::string errorType = "processOpenFiles";
    return MemoizedCall(
        mProcessFdCache,
        now,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessOpenFilesOnce(pid, static_cast<ProcessFd&>(info));
        },
        processFd,
        errorType,
        pid);
}

bool SystemInterface::GetTCPStatInformation(time_t now, TCPStatInformation& tcpStatInfo) {
    const std::string errorType = "TCP stat";
    return MemoizedCall(
        mTCPStatInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetTCPStatInformationOnce(static_cast<TCPStatInformation&>(info));
        },
        tcpStatInfo,
        errorType);
}

bool SystemInterface::GetNetInterfaceInformation(time_t now, NetInterfaceInformation& netInterfaceInfo) {
    const std::string errorType = "Net interface";
    return MemoizedCall(
        mNetInterfaceInformationCache,
        now,
        [this](BaseInformation& info) {
            return this->GetNetInterfaceInformationOnce(static_cast<NetInterfaceInformation&>(info));
        },
        netInterfaceInfo,
        errorType);
}

template <typename F, typename InfoT, typename... Args>
bool SystemInterface::MemoizedCall(SystemInformationCache<InfoT, Args...>& cache,
                                   time_t now,
                                   F&& func,
                                   InfoT& info,
                                   const std::string& errorType,
                                   Args... args) {
    auto cacheSizeBefore = cache.GetCacheSize();
    if (cache.Get(now, info, args...)) {
        auto cacheSizeAfter = cache.GetCacheSize();
        UpdateCacheMetrics(cacheSizeBefore, cacheSizeAfter);
        return true;
    }

    // Cache miss - need to call system API
    bool status = std::forward<F>(func)(info, args...);
    // We should use real time here, because input time may be delayed
    info.collectTime = time(nullptr);

    // Update system operation metrics
    UpdateSystemOpMetrics(status);

    if (status) {
        cache.Set(info, args...);
    } else {
        LOG_ERROR(sLogger, ("failed to get system information", errorType));
    }
    return status;
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::Get(time_t targetTime, InfoT& info, Args... args) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mCache.find(std::make_tuple(args...));
    if (it != mCache.end()) {
        auto& data = it->second.data;
        if (data.empty()) {
            return false;
        }

        // Use binary search to find the first entry with collectTime >= targetTime
        auto lower = std::lower_bound(data.begin(), data.end(), targetTime, [](const InfoT& entry, time_t target) {
            return entry.collectTime < target;
        });

        if (lower != data.end()) {
            // Found entry with collectTime >= targetTime, this is the closest entry after targetTime
            info = *lower;
            return true;
        } else {
            // All entries have collectTime < targetTime, no suitable entry found
            return false;
        }
    } else {
        return false;
    }
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::Set(InfoT& info, Args... args) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto& cacheEntry = mCache[std::make_tuple(args...)];
    auto& deque = cacheEntry.data;

    // Find correct position to maintain time ordering (newest at back)
    auto insertPos = std::upper_bound(
        deque.begin(), deque.end(), info, [](const InfoT& a, const InfoT& b) { return a.collectTime < b.collectTime; });
    if (insertPos != deque.end() && insertPos->collectTime == info.collectTime) {
        // conflict, use old value to keep consistency between different pipelines
        LOG_DEBUG(sLogger, ("system information cache conflict", "use old value instead of new value"));
        info = *insertPos;
    } else {
        deque.insert(insertPos, info);
        ++mCurrentSize;
    }

    // Remove oldest entries if size exceeds limit
    if (deque.size() > mCacheDequeSize) {
        deque.pop_front();
        --mCurrentSize;
    }

    // Update access time
    cacheEntry.lastAccessTime = std::chrono::steady_clock::now();

    if (ShouldPerformCleanup()) {
        PerformGarbageCollection();
    }
    return true;
}

template <typename InfoT>
bool SystemInterface::SystemInformationCache<InfoT>::Get(time_t targetTime, InfoT& info) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCache.empty()) {
        return false;
    }

    // Use binary search to find the first entry with collectTime >= targetTime
    auto lower = std::lower_bound(mCache.begin(), mCache.end(), targetTime, [](const InfoT& entry, time_t target) {
        return entry.collectTime < target;
    });

    if (lower != mCache.end()) {
        // Found entry with collectTime >= targetTime, this is the closest entry after targetTime
        info = *lower;
        return true;
    } else {
        // All entries have collectTime < targetTime, no suitable entry found
        return false;
    }
}

template <typename InfoT>
bool SystemInterface::SystemInformationCache<InfoT>::Set(InfoT& info) {
    std::lock_guard<std::mutex> lock(mMutex);

    // Find correct position to maintain time ordering (newest at back)
    auto insertPos = std::upper_bound(mCache.begin(), mCache.end(), info, [](const InfoT& a, const InfoT& b) {
        return a.collectTime < b.collectTime;
    });
    if (insertPos != mCache.end() && insertPos->collectTime == info.collectTime) {
        // conflict, use old value
        info = *insertPos;
    } else {
        mCache.insert(insertPos, info);
        ++mCurrentSize;
    }

    // Remove oldest entries if size exceeds limit
    if (mCache.size() > mCacheDequeSize) {
        mCache.pop_front();
        --mCurrentSize;
    }
    return true;
}

std::string MacString(const unsigned char* mac) {
    std::string str;
    if (mac != nullptr) {
        str = fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return str;
}

std::string IPv4String(uint32_t address) {
    using namespace boost::asio::ip;
    // address = boost::endian::big_to_native(address);
    address_v4 v4(*(address_v4::bytes_type*)(&address));
    return v4.to_string();
}

std::string IPv6String(const uint32_t address[4]) {
    using namespace boost::asio::ip;
    // address = boost::endian::big_to_native(address);
    address_v6 v6(*(address_v6::bytes_type*)(address));
    return v6.to_string();
}

NetAddress::NetAddress() {
    memset(this, 0, sizeof(*this));
}

static const auto& mapNetAddressStringer = *new std::map<int, std::function<std::string(const NetAddress*)>>{
    {NetAddress::SI_AF_UNSPEC, [](const NetAddress* me) { return std::string{}; }},
    {NetAddress::SI_AF_INET, [](const NetAddress* me) { return IPv4String(me->addr.in); }},
    {NetAddress::SI_AF_INET6, [](const NetAddress* me) { return IPv6String(me->addr.in6); }},
    {NetAddress::SI_AF_LINK, [](const NetAddress* me) { return MacString(me->addr.mac); }},
};

std::string NetAddress::str() const {
    std::string name;
    auto it = mapNetAddressStringer.find(this->family);
    if (it != mapNetAddressStringer.end()) {
        name = it->second(this);
    }
    return name;
}

// GC implementation for SystemInformationCache with arguments
template <typename InfoT, typename... Args>
void SystemInterface::SystemInformationCache<InfoT, Args...>::PerformGarbageCollection() {
    if (ClearExpiredEntries(mExpireThreshold)) {
        // only update last cleanup time if all expired entries are cleared
        mLastCleanupTime = std::chrono::steady_clock::now();
    }
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::ClearExpiredEntries(
    std::chrono::steady_clock::duration maxAge) {
    auto now = std::chrono::steady_clock::now();
    auto maxCleanupCount = mMaxCleanupCount;
    // if maxCleanupCount is set to 0 or negative, scan all entries
    if (maxCleanupCount <= 0) {
        maxCleanupCount = mCache.size();
    }
    int32_t cleanedCount = 0;

    for (auto it = mCache.begin(); it != mCache.end() && cleanedCount < maxCleanupCount;) {
        if (now - it->second.lastAccessTime > maxAge) {
            mCurrentSize -= it->second.data.size();
            it = mCache.erase(it);
            ++cleanedCount;
        } else {
            ++it;
        }
    }
    return cleanedCount < maxCleanupCount;
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::ShouldPerformCleanup() const {
    auto now = std::chrono::steady_clock::now();
    return (now - mLastCleanupTime) >= mCleanupInterval;
}

template <typename InfoT, typename... Args>
size_t SystemInterface::SystemInformationCache<InfoT, Args...>::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentSize;
}

template <typename InfoT>
size_t SystemInterface::SystemInformationCache<InfoT>::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentSize;
}

void SystemInterface::InitMetrics() {
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, "system_interface");
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels));

    mSystemOpTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SYSTEM_OP_TOTAL);
    mSystemOpFailTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SYSTEM_OP_FAIL_TOTAL);
    mUseCacheTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_SYSTEM_USE_CACHE_TOTAL);
    mCacheItemsSize = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_SYSTEM_CACHE_ITEMS_SIZE);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

void SystemInterface::UpdateSystemOpMetrics(bool success) {
    if (mSystemOpTotal) {
        mSystemOpTotal->Add(1);
    }
    if (!success && mSystemOpFailTotal) {
        mSystemOpFailTotal->Add(1);
    }
}

void SystemInterface::UpdateCacheMetrics(size_t cacheSizeBefore, size_t cacheSizeAfter) {
    if (mCacheItemsSize) {
        mCacheItemsSize->Set(cacheSizeAfter - cacheSizeBefore);
    }
    if (mUseCacheTotal) {
        mUseCacheTotal->Add(1);
    }
}

} // namespace logtail
