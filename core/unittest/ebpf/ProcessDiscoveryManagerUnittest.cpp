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

#include <fstream>
#include <mutex>

#include "app_config/AppConfig.h"
#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"
#include "unittest/Unittest.h"
#include "unittest/common/DirErrorInjector.h"

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

protected:
    void SetUp() override { AppConfig::GetInstance()->mPurageContainerMode = false; }
};

void ProcessDiscoveryManagerUnittest::TestStartAndStop() {
    auto callback = [](ProcessDiscoveryManager::DiscoverResult) {};

    // singleton
    ProcessDiscoveryManager::GetInstance()->Start(callback, /*milliseconds=*/100);
    ProcessDiscoveryManager::GetInstance()->Stop();
    ProcessDiscoveryManager::GetInstance()->Start(callback, /*milliseconds=*/100);
    ProcessDiscoveryManager::GetInstance()->Stop();

    // scope
    {
        ProcessDiscoveryManager manager;
        manager.Start(callback, /*milliseconds=*/100);
        manager.Stop();
        manager.Start(callback, /*milliseconds=*/100);
    }
}

void ProcessDiscoveryManagerUnittest::TestSingleConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) { count += r.size(); };

    ProcessDiscoveryManager manager;
    manager.Start(callback, /*milliseconds=*/100);

    // watch single config
    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{.mRegexs = {boost::regex("sleep.+")}});
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 1);
}

void ProcessDiscoveryManagerUnittest::TestMultiConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) { count += r.size(); };

    ProcessDiscoveryManager manager;
    manager.Start(callback, /*milliseconds=*/100);

    // watch single config
    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{.mRegexs = {boost::regex("sleep.+")}});
    manager.AddDiscovery("test_watch2", ProcessDiscoveryConfig{.mRegexs = {boost::regex("sleep.+")}});
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 2);
}

void ProcessDiscoveryManagerUnittest::TestUpdateConfig() {
    ProcessDiscoveryManager manager;
    manager.Start([](ProcessDiscoveryManager::DiscoverResult r) {}, /*milliseconds=*/100);

    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{});

    ContainerDiff diff;
    // ok to update "test_watch"
    APSARA_TEST_TRUE(manager.UpdateDiscovery("test_watch", diff));
    // not ok to update "test_watch2"
    APSARA_TEST_FALSE(manager.UpdateDiscovery("test_watch2", diff));
}

void ProcessDiscoveryManagerUnittest::TestRemoveConfig() {
    std::atomic<int> count = 0;
    auto callback = [&](ProcessDiscoveryManager::DiscoverResult r) { count += r.size(); };

    ProcessDiscoveryManager manager;
    manager.Start(callback, /*milliseconds=*/100);

    manager.AddDiscovery("test_watch", ProcessDiscoveryConfig{.mRegexs = {boost::regex("sleep.+")}});
    std::system("sleep 0.5");
    APSARA_TEST_GE(count, 1);

    manager.RemoveDiscovery("test_watch");
    count = 0;
    std::system("sleep 0.5");
    APSARA_TEST_EQUAL(count, 0);
}

void ProcessDiscoveryManagerUnittest::TestCheckConfigExist() {
    ProcessDiscoveryManager manager;
    manager.Start([](ProcessDiscoveryManager::DiscoverResult r) {}, /*milliseconds=*/100);

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

#if defined(__linux__)
namespace logtail {
namespace ebpf {
TEST_F(ProcessDiscoveryManagerUnittest, IncompletePidScanCannotWithdrawProfilingTargets) {
    const auto root = std::filesystem::path(logtail::GetProcessExecutionDir()) / "p1_process_discovery";
    const auto procDir = root / "proc";
    std::filesystem::create_directories(procDir / "12345");
    std::ofstream(procDir / "12345/cmdline") << "p1-test-worker";
    logtail::ebpf::ProcessDiscoveryManager manager;
    manager.mProcParser.emplace(root.string());
    std::vector<logtail::ebpf::ProcessDiscoveryManager::DiscoverResult> notifications;
    manager.mCallback = [&](auto result) { notifications.push_back(std::move(result)); };
    logtail::ebpf::ProcessDiscoveryConfig config{};
    config.mConfigKey = 1;
    config.mFullDiscovery = true;
    manager.AddDiscovery("test", config);
    manager.discover();
    ASSERT_EQ(1U, notifications.size());
    ASSERT_EQ(1U, notifications.back().size());
    EXPECT_EQ((std::set<uint32_t>{12345}), notifications.back().front().second);
    for (const auto& operation : {"open", "read", "close"}) {
        logtail::ScopedDirErrorInjector fault(procDir.string(), operation, EIO, 1);
        manager.discover();
        EXPECT_EQ(1U, notifications.size()) << operation;
        EXPECT_EQ((std::set<uint32_t>{12345}), manager.mStates.at("test").mPrevPids);
    }
    std::filesystem::remove_all(procDir / "12345");
    manager.discover();
    ASSERT_EQ(2U, notifications.size());
    EXPECT_TRUE(notifications.back().front().second.empty());
    std::filesystem::remove_all(root);
}
} // namespace ebpf
} // namespace logtail
#endif

UNIT_TEST_MAIN
