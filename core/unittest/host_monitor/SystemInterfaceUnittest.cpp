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

#include <chrono>
#include <future>
#include <thread>

#include "common/Flags.h"
#include "host_monitor/SystemInterface.h"
#include "unittest/Unittest.h"
#include "unittest/host_monitor/MockSystemInterface.h"

using namespace std;

DECLARE_FLAG_INT32(system_interface_default_cache_ttl);

namespace logtail {

class SystemInterfaceUnittest : public testing::Test {
public:
    void TestSystemInterfaceCache() const;
    void TestSystemInterfaceCacheGC() const;
    void TestMemoizedCall() const;
};

void SystemInterfaceUnittest::TestSystemInterfaceCache() const {
    auto timeout = std::chrono::milliseconds{100};
    // No args
    { // case1: cache stale -> thread1 query -> thread1 update -> thread2 query
        SystemInterface::SystemInformationCache<MockInformation> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        // thread1 query and update
        auto future1 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout));
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        future1.get();
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout));
            APSARA_TEST_EQUAL_FATAL(2, info.id);
        });
        future2.get();
    }
    { // case2: cache stale -> thread1 query -> thread2 query -> thread1 update
        SystemInterface::SystemInformationCache<MockInformation> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10));
            APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout * 2));
            APSARA_TEST_EQUAL_FATAL(2, info.id);
        });
        future1.get();
        future2.get();
    }
    { // case3: cache stale -> thread1 query -> thread2 query -> thread1 update -> thread2 update
        SystemInterface::SystemInformationCache<MockInformation> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{1100});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread2 update
            info.id = 3;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        future1.get();
        future2.get();
        // check if cache is updated
        APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout));
        APSARA_TEST_EQUAL_FATAL(3, info.id);
    }
    { // case4: cache stale -> thread1 query -> thread2 query -> thread2 update -> thread1 update
        SystemInterface::SystemInformationCache<MockInformation> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10));
            // thread2 update
            info.id = 3;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info);
        });
        future1.get();
        future2.get();
        // check if cache is updated
        APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout));
        APSARA_TEST_EQUAL_FATAL(2, info.id);
    }
    // With args
    { // case1: cache stale -> thread1 query -> thread1 update -> thread2 query
        SystemInterface::SystemInformationCache<MockInformation, int> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info, 1);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        // thread1 query and update
        auto future1 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout, 1));
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        future1.get();
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout, 1));
            APSARA_TEST_EQUAL_FATAL(2, info.id);
        });
        future2.get();
    }
    { // case2: cache stale -> thread1 query -> thread2 query -> thread1 update
        SystemInterface::SystemInformationCache<MockInformation, int> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info, 1);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout, 1));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10, 1));
            APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout * 2, 1));
            APSARA_TEST_EQUAL_FATAL(2, info.id);
        });
        future1.get();
        future2.get();
    }
    { // case3: cache stale -> thread1 query -> thread2 query -> thread1 update -> thread2 update
        SystemInterface::SystemInformationCache<MockInformation, int> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info, 1);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{1100});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout, 1));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10, 1));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread2 update
            info.id = 3;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        future1.get();
        future2.get();
        // check if cache is updated
        APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout, 1));
        APSARA_TEST_EQUAL_FATAL(3, info.id);
    }
    { // case4: cache stale -> thread1 query -> thread2 query -> thread2 update -> thread1 update
        SystemInterface::SystemInformationCache<MockInformation, int> cache(timeout);
        // add data into cache
        MockInformation info;
        info.id = 1;
        info.collectTime = std::chrono::steady_clock::now();
        cache.Set(info, 1);
        // wait for cache to be stale
        this_thread::sleep_for(std::chrono::milliseconds{200});
        auto future1 = async(std::launch::async, [&]() {
            // thread1 query
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout, 1));
            this_thread::sleep_for(std::chrono::milliseconds{200});
            // thread1 update
            info.id = 2;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        // thread2 query
        auto future2 = async(std::launch::async, [&]() {
            MockInformation info;
            APSARA_TEST_FALSE_FATAL(cache.GetWithTimeout(info, timeout / 10, 1));
            // thread2 update
            info.id = 3;
            info.collectTime = std::chrono::steady_clock::now();
            cache.Set(info, 1);
        });
        future1.get();
        future2.get();
        // check if cache is updated
        APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout, 1));
        APSARA_TEST_EQUAL_FATAL(2, info.id);
    }
}

void SystemInterfaceUnittest::TestSystemInterfaceCacheGC() const {
    auto timeout = std::chrono::milliseconds{100};
    SystemInterface::SystemInformationCache<MockInformation, int> cache(timeout);
    // add data into cache
    MockInformation info;
    info.id = 1;
    info.collectTime = std::chrono::steady_clock::now();
    cache.Set(info, 1);
    APSARA_TEST_TRUE_FATAL(cache.GetWithTimeout(info, timeout, 1));
    // wait for cache to be stale
    this_thread::sleep_for(std::chrono::milliseconds{200});
    cache.GC();
    APSARA_TEST_EQUAL_FATAL(cache.mCache.size(), 0);
}

void SystemInterfaceUnittest::TestMemoizedCall() const {
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 100;
        mockSystemInterface.mMockCalledCount = 0;
        SystemInformation info;
        mockSystemInterface.GetSystemInformation(info);
        this_thread::sleep_for(std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)});
        mockSystemInterface.GetSystemInformation(info);
        // SystemInformation is static, cache will never be stale
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 10;
        mockSystemInterface.mMockCalledCount = 0;
        auto future1 = async(std::launch::async, [&]() {
            CPUInformation info;
            mockSystemInterface.GetCPUInformation(info);
        });
        auto future2 = async(std::launch::async, [&]() {
            CPUInformation info;
            mockSystemInterface.GetCPUInformation(info);
        });
        future1.get();
        future2.get();
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        this_thread::sleep_for(std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)});
        CPUInformation info;
        mockSystemInterface.GetCPUInformation(info);
        APSARA_TEST_EQUAL_FATAL(2, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 100;
        mockSystemInterface.mMockCalledCount = 0;
        auto future1 = async(std::launch::async, [&]() {
            ProcessListInformation info;
            mockSystemInterface.GetProcessListInformation(info);
        });
        auto future2 = async(std::launch::async, [&]() {
            ProcessListInformation info;
            mockSystemInterface.GetProcessListInformation(info);
        });
        future1.get();
        future2.get();
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        this_thread::sleep_for(std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)});
        ProcessListInformation info;
        mockSystemInterface.GetProcessListInformation(info);
        APSARA_TEST_EQUAL_FATAL(2, mockSystemInterface.mMockCalledCount);
    }
    {
        MockSystemInterface mockSystemInterface;
        mockSystemInterface.mBlockTime = 100;
        mockSystemInterface.mMockCalledCount = 0;
        auto future1 = async(std::launch::async, [&]() {
            ProcessInformation info;
            mockSystemInterface.GetProcessInformation(1, info);
        });
        auto future2 = async(std::launch::async, [&]() {
            ProcessInformation info;
            mockSystemInterface.GetProcessInformation(1, info);
        });
        future1.get();
        future2.get();
        APSARA_TEST_EQUAL_FATAL(1, mockSystemInterface.mMockCalledCount);
        this_thread::sleep_for(std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)});
        ProcessInformation info;
        mockSystemInterface.GetProcessInformation(1, info);
        APSARA_TEST_EQUAL_FATAL(2, mockSystemInterface.mMockCalledCount);
    }
}

UNIT_TEST_CASE(SystemInterfaceUnittest, TestSystemInterfaceCache);
UNIT_TEST_CASE(SystemInterfaceUnittest, TestSystemInterfaceCacheGC);
UNIT_TEST_CASE(SystemInterfaceUnittest, TestMemoizedCall);

} // namespace logtail

UNIT_TEST_MAIN
