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

#include <chrono>
#include <mutex>
#include <tuple>
#include <utility>

#include "boost/type_index.hpp"

#include "common/Flags.h"
#include "logger/Logger.h"
#ifdef __linux__
#include "host_monitor/LinuxSystemInterface.h"
#endif
#ifdef APSARA_UNIT_TEST_MAIN
#include "unittest/host_monitor/MockSystemInterface.h"
#endif

DEFINE_FLAG_INT32(system_interface_default_cache_ttl, "system interface default cache ttl, ms", 1000);

namespace logtail {

SystemInterface* SystemInterface::GetInstance() {
#ifdef __linux__
    return LinuxSystemInterface::GetInstance();
#elif APSARA_UNIT_TEST_MAIN
    return MockSystemInterface::GetInstance();
#else
    LOG_ERROR(sLogger, "SystemInterface is not implemented for this platform");
    return nullptr;
#endif
}

bool SystemInterface::GetSystemInformation(SystemInformation& systemInfo) {
    // SystemInformation is static and will not be changed. So cache will never be expired.
    if (mSystemInformationCache.collectTime.time_since_epoch().count() > 0) {
        systemInfo = mSystemInformationCache;
        return true;
    }
    if (GetSystemInformationOnce(mSystemInformationCache)) {
        systemInfo = mSystemInformationCache;
        return true;
    }
    return false;
}

bool SystemInterface::GetCPUInformation(CPUInformation& cpuInfo) {
    const std::string errorType = "cpu";
    return MemoizedCall(
        mCPUInformationCache,
        [this](BaseInformation& info) { return this->GetCPUInformationOnce(static_cast<CPUInformation&>(info)); },
        cpuInfo,
        errorType);
}

bool SystemInterface::GetProcessListInformation(ProcessListInformation& processListInfo) {
    const std::string errorType = "process list";
    return MemoizedCall(
        mProcessListInformationCache,
        [this](BaseInformation& info) {
            return this->GetProcessListInformationOnce(static_cast<ProcessListInformation&>(info));
        },
        processListInfo,
        errorType);
}

bool SystemInterface::GetProcessInformation(pid_t pid, ProcessInformation& processInfo) {
    const std::string errorType = "process";
    return MemoizedCall(
        mProcessInformationCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessInformationOnce(pid, static_cast<ProcessInformation&>(info));
        },
        processInfo,
        errorType,
        pid);
}

bool SystemInterface::GetSystemLoadInformation(SystemLoadInformation& systemLoadInfo) {
    const std::string errorType = "system load";
    return MemoizedCall(
        mSystemLoadInformationCache,
        [this](BaseInformation& info) {
            return this->GetSystemLoadInformationOnce(static_cast<SystemLoadInformation&>(info));
        },
        systemLoadInfo,
        errorType);
}

bool SystemInterface::GetCPUCoreNumInformation(CpuCoreNumInformation& cpuCoreNumInfo) {
    const std::string errorType = "cpu core num";
    return MemoizedCall(
        mCPUCoreNumInformationCache,
        [this](BaseInformation& info) {
            return this->GetCPUCoreNumInformationOnce(static_cast<CpuCoreNumInformation&>(info));
        },
        cpuCoreNumInfo,
        errorType);
}

bool SystemInterface::GetHostMemInformationStat(MemoryInformation& meminfo) {
    const std::string errorType = "mem";
    return MemoizedCall(
        mMemInformationCache,
        [this](BaseInformation& info) {
            return this->GetHostMemInformationStatOnce(static_cast<MemoryInformation&>(info));
        },
        meminfo,
        errorType);
}

template <typename F, typename InfoT, typename... Args>
bool SystemInterface::MemoizedCall(
    SystemInformationCache<InfoT, Args...>& cache, F&& func, InfoT& info, const std::string& errorType, Args... args) {
    if (cache.GetWithTimeout(
            info, std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)}, args...)) {
        return true;
    }
    bool status = std::forward<F>(func)(info, args...);
    if (status) {
        cache.Set(info, args...);
    } else {
        LOG_ERROR(sLogger, ("failed to get system information", errorType));
    }
    static int sGCCount = 0;
    sGCCount++;
    if (sGCCount >= 100) { // Perform GC every 100 calls
        cache.GC();
        sGCCount = 0;
    }
    return status;
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::GetWithTimeout(InfoT& info,
                                                                             std::chrono::milliseconds timeout,
                                                                             Args... args) {
    auto now = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(mMutex);
    auto it = mCache.find(std::make_tuple(args...));
    if (it != mCache.end()) {
        if (now - it->second.first.collectTime < mTTL) {
            info = it->second.first; // copy to avoid external modify
            return true;
        }
        if (!it->second.second) {
            // the cache is stale and no thread is updating, will update by this thread
            it->second.second.store(true);
            return false;
        }
    } else {
        // no data in cache, directly update
        mCache[std::make_tuple(args...)] = std::make_pair(InfoT{}, true);
        return false;
    }
    // the cache is stale and other threads is updating, wait for it
    auto status = mConditionVariable.wait_until(lock, std::chrono::steady_clock::now() + timeout);
    if (status == std::cv_status::timeout) {
        LOG_ERROR(sLogger,
                  ("system information update", "too slow")("type", boost::typeindex::type_id<InfoT>().pretty_name()));
        return false; // timeout
    }
    // query again
    now = std::chrono::steady_clock::now();
    it = mCache.find(std::make_tuple(args...));
    if (it != mCache.end() && now - it->second.first.collectTime < mTTL) {
        info = it->second.first; // copy to avoid external modify
        return true;
    }
    return false;
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::Set(InfoT& info, Args... args) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCache[std::make_tuple(args...)] = std::make_pair(info, false);
    mConditionVariable.notify_all();
    return true;
}

template <typename InfoT, typename... Args>
bool SystemInterface::SystemInformationCache<InfoT, Args...>::GC() {
    std::lock_guard<std::mutex> lock(mMutex);
    auto now = std::chrono::steady_clock::now();
    for (auto it = mCache.begin(); it != mCache.end();) {
        if (now - it->second.first.collectTime > mTTL) {
            it = mCache.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}

template <typename InfoT>
bool SystemInterface::SystemInformationCache<InfoT>::GetWithTimeout(InfoT& info, std::chrono::milliseconds timeout) {
    auto now = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(mMutex);
    if (mCache.first.collectTime.time_since_epoch().count() > 0 && now - mCache.first.collectTime < mTTL) {
        info = mCache.first; // copy to avoid external modify
        return true;
    }
    if (!mCache.second) {
        // the cache is stale and no thread is updating, will update by this thread
        mCache.second.store(true);
        return false;
    }
    // the cache is stale and other threads is updating, wait for it
    auto status
        = mConditionVariable.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout));
    if (status == std::cv_status::timeout) {
        LOG_ERROR(sLogger,
                  ("system information update", "too slow")("type", boost::typeindex::type_id<InfoT>().pretty_name()));
        return false; // timeout
    }
    // query again
    now = std::chrono::steady_clock::now();
    if (now - mCache.first.collectTime < mTTL) {
        info = mCache.first; // copy to avoid external modify
        return true;
    }
    return false;
}

template <typename InfoT>
bool SystemInterface::SystemInformationCache<InfoT>::Set(InfoT& info) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCache = std::make_pair(info, false);
    mConditionVariable.notify_all();
    return true;
}

template <typename InfoT>
bool SystemInterface::SystemInformationCache<InfoT>::GC() {
    // no need to GC for single cache
    return true;
}

} // namespace logtail
