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

#include <cstdlib>
#include <mutex>

#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class ProcessDiscoveryManagerUnittest : public testing::Test {
public:
    void TestStartAndStop();
    void TestSingleConfig();
    void TestMultiConfig();
    void TestUpdateConfig();
    void TestRemoveConfig();
    void TestCheckConfigExist();
};

void ProcessDiscoveryManagerUnittest::TestStartAndStop() {
    auto callback = [](ProcessDiscoveryManager::DiscoverResult) {};

    // singleton
    ProcessDiscoveryManager::GetInstance()->Start(callback);
    ProcessDiscoveryManager::GetInstance()->Stop();
    ProcessDiscoveryManager::GetInstance()->Start(callback);
    ProcessDiscoveryManager::GetInstance()->Stop();
    
    // scope
    {
        ProcessDiscoveryManager manager;
        manager.Start(callback);
        manager.Stop();
        manager.Start(callback);
    }
}

void ProcessDiscoveryManagerUnittest::TestSingleConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) {
        count += r.size();
    };

    ProcessDiscoveryManager manager;
    manager.Start(callback);

    // watch single config
    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{
        .mRegexs = {boost::regex("sleep.+")}
    });
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 1);
}

void ProcessDiscoveryManagerUnittest::TestMultiConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) {
        count += r.size();
    };

    ProcessDiscoveryManager manager;
    manager.Start(callback);

    // watch single config
    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{
        .mRegexs = {boost::regex("sleep.+")}
    });
    manager.AddDiscovery("test_watch2", ProcessDiscoveryConfig{
        .mRegexs = {boost::regex("sleep.+")}
    });
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 2);
}

void ProcessDiscoveryManagerUnittest::TestUpdateConfig() {
    ProcessDiscoveryManager manager;
    manager.Start([](ProcessDiscoveryManager::DiscoverResult r) {});

    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{});

    // ok to update "test_watch"
    APSARA_TEST_TRUE(manager.UpdateDiscovery("test_watch", [](ProcessDiscoveryConfig& config) {}));

    // not ok to update "test_watch2" 
    APSARA_TEST_FALSE(manager.UpdateDiscovery("test_watch2", [](ProcessDiscoveryConfig& config) {}));
}

void ProcessDiscoveryManagerUnittest::TestRemoveConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) {
        count += r.size();
    };

    ProcessDiscoveryManager manager;
    manager.Start(callback);

    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{
        .mRegexs = {boost::regex("sleep.+")}
    });
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 1);

    count = 0;
    manager.RemoveDiscovery("test_watch");
    std::system("sleep 0.5");
    APSARA_TEST_EQUAL(count, 0);
}

void ProcessDiscoveryManagerUnittest::TestCheckConfigExist() {
    ProcessDiscoveryManager manager;
    manager.Start([](ProcessDiscoveryManager::DiscoverResult r) {});

    APSARA_TEST_FALSE(manager.CheckDiscoveryExist("test_watch"));
    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{});
    APSARA_TEST_TRUE(manager.CheckDiscoveryExist("test_watch"));
}

UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestStartAndStop);
UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestSingleConfig);
UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestMultiConfig);
UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestUpdateConfig);
UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestRemoveConfig);
UNIT_TEST_CASE(ProcessDiscoveryManagerUnittest, TestCheckConfigExist);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN