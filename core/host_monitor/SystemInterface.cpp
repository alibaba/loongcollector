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

#include <boost/asio.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
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

bool SystemInterface::GetProcessCmdlineString(pid_t pid, ProcessCmdlineString& processCmdlineString) {
    const std::string errorType = "processCmdline";
    return MemoizedCall(
        mProcessCmdlineCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessCmdlineStringOnce(pid, static_cast<ProcessCmdlineString&>(info));
        },
        processCmdlineString,
        errorType,
        pid);
}

bool SystemInterface::GetPorcessStatm(pid_t pid, ProcessMemoryInformation& processMemory) {
    const std::string errorType = "processStatm";
    return MemoizedCall(
        mProcessStatmCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessStatmOnce(pid, static_cast<ProcessMemoryInformation&>(info));
        },
        processMemory,
        errorType,
        pid);
}

bool SystemInterface::GetProcessCredNameObj(pid_t pid, ProcessCredName& credName) {
    const std::string errorType = "processStatus";
    return MemoizedCall(
        mProcessStatusCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessCredNameOnce(pid, static_cast<ProcessCredName&>(info));
        },
        credName,
        errorType,
        pid);
}

bool SystemInterface::GetExecutablePathCache(pid_t pid, ProcessExecutePath& executePath) {
    const std::string errorType = "executablePath";
    return MemoizedCall(
        mExecutePathCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetExecutablePathOnce(pid, static_cast<ProcessExecutePath&>(info));
        },
        executePath,
        errorType,
        pid);
}

bool SystemInterface::GetProcessOpenFiles(pid_t pid, ProcessFd& processFd) {
    const std::string errorType = "processOpenFiles";
    return MemoizedCall(
        mProcessFdCache,
        [this](BaseInformation& info, pid_t pid) {
            return this->GetProcessOpenFilesOnce(pid, static_cast<ProcessFd&>(info));
        },
        processFd,
        errorType,
        pid);
}
bool SystemInterface::GetTCPStatInformation(TCPStatInformation& tcpStatInfo) {
    const std::string errorType = "TCP stat";
    return MemoizedCall(
        mTCPStatInformationCache,
        [this](BaseInformation& info) {
            return this->GetTCPStatInformationOnce(static_cast<TCPStatInformation&>(info));
        },
        tcpStatInfo,
        errorType);
}

bool SystemInterface::GetNetInterfaceInformation(NetInterfaceInformation& netInterfaceInfo) {
    const std::string errorType = "Net interface";
    return MemoizedCall(
        mNetInterfaceInformationCache,
        [this](BaseInformation& info) {
            return this->GetNetInterfaceInformationOnce(static_cast<NetInterfaceInformation&>(info));
        },
        netInterfaceInfo,
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

std::string MacString(const unsigned char* mac) {
    std::string str;
    if (mac != nullptr && sizeof(mac) >= 6) {
        str = fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        // str = fmt::sprintf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

} // namespace logtail
