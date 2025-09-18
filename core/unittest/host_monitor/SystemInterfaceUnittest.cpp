// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>

#include <chrono>
#include <future>
#include <thread>

#include "common/Flags.h"
#include "host_monitor/SystemInterface.h"
#include "unittest/Unittest.h"
#include "unittest/host_monitor/MockSystemInterface.h"

using namespace std;

DECLARE_FLAG_INT32(system_interface_cache_queue_size);
DECLARE_FLAG_INT32(system_interface_cache_entry_expire_seconds);
DECLARE_FLAG_INT32(system_interface_cache_cleanup_interval_seconds);
DECLARE_FLAG_INT32(system_interface_cache_max_cleanup_batch_size);

namespace logtail {

class SystemInterfaceUnittest : public testing::Test {
public:
    void TestSystemInterfaceCache() const;
    void TestSystemInterfaceCacheGC() const;
    void TestMemoizedCall() const;
};

void SystemInterfaceUnittest::TestSystemInterfaceCache() const {
    size_t cacheSize = 5;
    // No args
    { // case1: basic cache functionality
        SystemInterface::SystemInformationCache<MockInformation> cache(cacheSize);
        MockInformation info;
        auto now = time(nullptr);

        // Should miss initially
        APSARA_TEST_FALSE_FATAL(cache.Get(now, info));

        // Add data and retrieve it
        info.id = 1;
        info.collectTime = now;
        cache.Set(info);

        MockInformation info3;
        info3.id = 3;
        info3.collectTime = now + 2;
        cache.Set(info3);

        MockInformation info2;
        info2.id = 2;
        info2.collectTime = now + 1;
        cache.Set(info2);

        APSARA_TEST_EQUAL_FATAL(3, cache.mCache.size());
        for (size_t i = 0; i < cache.mCache.size(); ++i) {
            APSARA_TEST_EQUAL_FATAL(i + 1, cache.mCache[i].id);
            APSARA_TEST_EQUAL_FATAL(now + i, cache.mCache[i].collectTime);
        }
    }

    { // case2: concurrent access without args
        SystemInterface::SystemInformationCache<MockInformation> cache(cacheSize);
        auto future1 = async(std::launch::async, [&]() {
            MockInformation info;
            info.id = 2;
            info.collectTime = time(nullptr);
            cache.Set(info);
        });
        auto future2 = async(std::launch::async, [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            MockInformation info;
            auto now = time(nullptr);
            if (cache.Get(now, info)) {
                APSARA_TEST_EQUAL_FATAL(2, info.id);
            }
        });
        future1.get();
        future2.get();
    }

    { // case3: now is between two entries, so return the closest entry after now
        SystemInterface::SystemInformationCache<MockInformation> cache(cacheSize);
        MockInformation info;
        auto now = time(nullptr);
        APSARA_TEST_FALSE_FATAL(cache.Get(now, info));
        info.id = 1;
        info.collectTime = now - 1;
        cache.Set(info);

        MockInformation info2;
        info2.id = 2;
        info2.collectTime = now + 1;
        cache.Set(info2);

        MockInformation info3;
        APSARA_TEST_TRUE_FATAL(cache.Get(now, info3));
        APSARA_TEST_EQUAL_FATAL(2, info3.id);
        APSARA_TEST_EQUAL_FATAL(now + 1, info3.collectTime);

        MockInformation info4;
        APSARA_TEST_TRUE_FATAL(cache.Get(now - 2, info4));
        APSARA_TEST_EQUAL_FATAL(1, info4.id);
        APSARA_TEST_EQUAL_FATAL(now - 1, info4.collectTime);

        MockInformation info5;
        APSARA_TEST_FALSE_FATAL(cache.Get(now + 2, info5));
    }
    { // case4: cache is full, so the oldest entry is removed
        SystemInterface::SystemInformationCache<MockInformation> cache(3);
        auto now = time(nullptr);
        for (int i = 0; i < 3; ++i) {
            MockInformation info;
            info.id = i;
            info.collectTime = now + i;
            cache.Set(info);
        }
        APSARA_TEST_EQUAL_FATAL(3, cache.mCache.size());
        MockInformation info;
        APSARA_TEST_TRUE_FATAL(cache.Get(now, info));
        APSARA_TEST_EQUAL_FATAL(0, info.id);
        APSARA_TEST_EQUAL_FATAL(now, info.collectTime);
        APSARA_TEST_TRUE_FATAL(cache.Get(now + 1, info));

        MockInformation info2;
        info2.id = 2;
        info2.collectTime = now + 2;
        cache.Set(info2);
        APSARA_TEST_EQUAL_FATAL(3, cache.mCache.size());
        APSARA_TEST_TRUE_FATAL(cache.Get(now + 2, info));
        APSARA_TEST_EQUAL_FATAL(2, info.id);

        APSARA_TEST_TRUE_FATAL(cache.Get(now, info));
        APSARA_TEST_EQUAL_FATAL(1, info.id);
        APSARA_TEST_EQUAL_FATAL(now + 1, info.collectTime);
    }

    // With args
    { // case1: basic cache functionality with args
        SystemInterface::SystemInformationCache<MockInformation, int> cache(cacheSize);
        MockInformation info;
        auto now = time(nullptr);

        // Should miss initially
        APSARA_TEST_FALSE_FATAL(cache.Get(now, info, 1));

        // Add data and retrieve it
        info.id = 1;
        info.collectTime = now;
        cache.Set(info, 1);

        MockInformation info3;
        info3.id = 3;
        info3.collectTime = now + 2;
        cache.Set(info3, 1);

        MockInformation info2;
        info2.id = 2;
        info2.collectTime = now + 1;
        cache.Set(info2, 1);

        auto cacheEntry = cache.mCache[1];
        APSARA_TEST_EQUAL_FATAL(3, cacheEntry.data.size());
        for (size_t i = 0; i < cacheEntry.data.size(); ++i) {
            APSARA_TEST_EQUAL_FATAL(i + 1, cacheEntry.data[i].id);
            APSARA_TEST_EQUAL_FATAL(now + i, cacheEntry.data[i].collectTime);
        }
    }

    { // case2: concurrent access with args
        SystemInterface::SystemInformationCache<MockInformation, int> cache(cacheSize);
        auto future1 = async(std::launch::async, [&]() {
            MockInformation info;
            info.id = 2;
            info.collectTime = time(nullptr);
            cache.Set(info, 1);
        });
        auto future2 = async(std::launch::async, [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            MockInformation info;
            auto now = time(nullptr);
            if (cache.Get(now, info, 1)) {
                APSARA_TEST_EQUAL_FATAL(2, info.id);
            }
        });
        future1.get();
        future2.get();
    }

    { // case3: now is between two entries, so return the closest entry after now
        SystemInterface::SystemInformationCache<MockInformation, int> cache(cacheSize);
        MockInformation info;
        auto now = time(nullptr);
        APSARA_TEST_FALSE_FATAL(cache.Get(now, info, 1));
        info.id = 1;
        info.collectTime = now - 1;
        cache.Set(info, 1);

        MockInformation info2;
        info2.id = 2;
        info2.collectTime = now + 1;
        cache.Set(info2, 1);

        MockInformation info3;
        APSARA_TEST_TRUE_FATAL(cache.Get(now, info3, 1));
        APSARA_TEST_EQUAL_FATAL(2, info3.id);
        APSARA_TEST_EQUAL_FATAL(now + 1, info3.collectTime);

        MockInformation info4;
        APSARA_TEST_TRUE_FATAL(cache.Get(now - 2, info4, 1));
        APSARA_TEST_EQUAL_FATAL(1, info4.id);
        APSARA_TEST_EQUAL_FATAL(now - 1, info4.collectTime);

        MockInformation info5;
        APSARA_TEST_FALSE_FATAL(cache.Get(now + 2, info5, 1));
    }
    { // case4: cache is full, so the oldest entry is removed
        SystemInterface::SystemInformationCache<MockInformation, int> cache(3);
        auto now = time(nullptr);
        for (int i = 0; i < 3; ++i) {
            MockInformation info;
            info.id = i;
            info.collectTime = now + i;
            cache.Set(info, 1);
        }
        MockInformation info;
        APSARA_TEST_TRUE_FATAL(cache.Get(now, info, 1));
        APSARA_TEST_EQUAL_FATAL(0, info.id);
        APSARA_TEST_EQUAL_FATAL(now, info.collectTime);
        APSARA_TEST_TRUE_FATAL(cache.Get(now + 1, info, 1));

        MockInformation info2;
        info2.id = 4;
        info2.collectTime = now + 4;
        cache.Set(info2, 1);
        APSARA_TEST_TRUE_FATAL(cache.Get(now + 4, info, 1));
        APSARA_TEST_EQUAL_FATAL(4, info.id);
        APSARA_TEST_EQUAL_FATAL(now + 4, info.collectTime);

        APSARA_TEST_TRUE_FATAL(cache.Get(now, info, 1));
        APSARA_TEST_EQUAL_FATAL(1, info.id);
        APSARA_TEST_EQUAL_FATAL(now + 1, info.collectTime);
    }
    { // case5: multiple keys with args
        SystemInterface::SystemInformationCache<MockInformation, int> cache(cacheSize);
        auto now = time(nullptr);

        // Add multiple entries with different keys
        for (int i = 1; i <= 3; ++i) {
            MockInformation info;
            info.id = i;
            info.collectTime = now;
            cache.Set(info, i);
        }

        // Verify all entries exist
        for (int i = 1; i <= 3; ++i) {
            MockInformation info;
            APSARA_TEST_TRUE_FATAL(cache.Get(now, info, i));
            APSARA_TEST_EQUAL_FATAL(i, info.id);
        }

        // Verify cache size
        APSARA_TEST_EQUAL_FATAL(3, cache.GetCacheSize());
    }
}

void SystemInterfaceUnittest::TestSystemInterfaceCacheGC() const {
    int32_t defaultCacheSize = INT32_FLAG(system_interface_cache_queue_size);
    int32_t defaultEntryExpireSeconds = INT32_FLAG(system_interface_cache_entry_expire_seconds);
    int32_t defaultCleanupIntervalSeconds = INT32_FLAG(system_interface_cache_cleanup_interval_seconds);
    int32_t defaultMaxCleanupBatchSize = INT32_FLAG(system_interface_cache_max_cleanup_batch_size);
    INT32_FLAG(system_interface_cache_queue_size) = 5;
    INT32_FLAG(system_interface_cache_entry_expire_seconds) = 1;
    INT32_FLAG(system_interface_cache_cleanup_interval_seconds) = 1;
    INT32_FLAG(system_interface_cache_max_cleanup_batch_size) = 1;

    SystemInterface::SystemInformationCache<MockInformation, int> cache(5);
    auto defaultLastCleanupTime = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    cache.mLastCleanupTime = defaultLastCleanupTime;
    auto now = time(nullptr);
    for (int i = 0; i < 5; ++i) {
        MockInformation info;
        info.id = i;
        info.collectTime = now + i;
        cache.Set(info, i);
    }
    std::this_thread::sleep_for(std::chrono::seconds{2});
    // partial cleanup
    cache.PerformGarbageCollection();
    APSARA_TEST_EQUAL_FATAL(4, cache.GetCacheSize());
    APSARA_TEST_NOT_EQUAL_FATAL(cache.mLastCleanupTime, defaultLastCleanupTime);

    // restore flags
    INT32_FLAG(system_interface_cache_queue_size) = defaultCacheSize;
    INT32_FLAG(system_interface_cache_entry_expire_seconds) = defaultEntryExpireSeconds;
    INT32_FLAG(system_interface_cache_cleanup_interval_seconds) = defaultCleanupIntervalSeconds;
    INT32_FLAG(system_interface_cache_max_cleanup_batch_size) = defaultMaxCleanupBatchSize;
}

void SystemInterfaceUnittest::TestMemoizedCall() const {
    int32_t defaultCacheSize = INT32_FLAG(system_interface_cache_queue_size);
    int32_t defaultEntryExpireSeconds = INT32_FLAG(system_interface_cache_entry_expire_seconds);
    int32_t defaultCleanupIntervalSeconds = INT32_FLAG(system_interface_cache_cleanup_interval_seconds);
    int32_t defaultMaxCleanupBatchSize = INT32_FLAG(system_interface_cache_max_cleanup_batch_size);
    INT32_FLAG(system_interface_cache_queue_size) = 5;
    INT32_FLAG(system_interface_cache_entry_expire_seconds) = 1;
    INT32_FLAG(system_interface_cache_cleanup_interval_seconds) = 1;
    INT32_FLAG(system_interface_cache_max_cleanup_batch_size) = 1;
    {
        MockSystemInterface mockSystemInterface;
        SystemInformation info;
        mockSystemInterface.GetSystemInformation(info);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        mockSystemInterface.GetSystemInformation(info);
        // SystemInformation is static, cache will never be stale
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 10;
        mockSystemInterface.mMockCalledCount = 0;
        auto now = time(nullptr);
        CPUInformation info1;
        info1.collectTime = now;
        mockSystemInterface.GetCPUInformation(now, info1);
        CPUInformation info2;
        info2.collectTime = now;
        mockSystemInterface.GetCPUInformation(now, info2);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        CPUInformation info;
        info.collectTime = now + 10;
        mockSystemInterface.GetCPUInformation(now, info);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 10;
        mockSystemInterface.mMockCalledCount = 0;
        auto now = time(nullptr);
        ProcessListInformation info1;
        info1.collectTime = now;
        mockSystemInterface.GetProcessListInformation(now, info1);
        ProcessListInformation info2;
        info2.collectTime = now;
        mockSystemInterface.GetProcessListInformation(now, info2);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        ProcessListInformation info;
        mockSystemInterface.GetProcessListInformation(now, info);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 10;
        mockSystemInterface.mMockCalledCount = 0;
        auto now = time(nullptr);
        ProcessInformation info1;
        info1.collectTime = now;
        mockSystemInterface.GetProcessInformation(now, 1, info1);
        ProcessInformation info2;
        info2.collectTime = now;
        mockSystemInterface.GetProcessInformation(now, 1, info2);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        ProcessInformation info;
        mockSystemInterface.GetProcessInformation(now, 1, info);
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
    }

    // restore flags
    INT32_FLAG(system_interface_cache_queue_size) = defaultCacheSize;
    INT32_FLAG(system_interface_cache_entry_expire_seconds) = defaultEntryExpireSeconds;
    INT32_FLAG(system_interface_cache_cleanup_interval_seconds) = defaultCleanupIntervalSeconds;
    INT32_FLAG(system_interface_cache_max_cleanup_batch_size) = defaultMaxCleanupBatchSize;
}

UNIT_TEST_CASE(SystemInterfaceUnittest, TestSystemInterfaceCache);
UNIT_TEST_CASE(SystemInterfaceUnittest, TestSystemInterfaceCacheGC);
UNIT_TEST_CASE(SystemInterfaceUnittest, TestMemoizedCall);

} // namespace logtail

UNIT_TEST_MAIN
