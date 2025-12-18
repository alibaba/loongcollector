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
#include <fstream>
#include <memory>
#include <thread>

#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/entity/ProcessEntityRunner.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessEntityRunnerUnittest : public testing::Test {
public:
    void TestGetCurrentProcessPrimaryKeys();
    void TestGetProcessEntityInfo();
    void TestUpdateVariableAttributes();
    void TestShouldCollectProcessKernelThread();
    void TestShouldCollectProcessMinRunningTime();
    void TestShouldCollectProcessBlacklist();
    void TestShouldCollectProcessWhitelist();
    void TestDetectChangesAdded();
    void TestDetectChangesRemoved();
    void TestDetectChangesReused();
    void TestProcessPrimaryKeyHash();
    void TestProcessFilterConfig();
    void TestCollectContextTimeManagement();
    void TestInitAndStop();
    void TestRegisterAndRemoveConfig();
    void TestRegisterConfigDuplicate();
    void TestRemoveAllConfigs();
    void TestSetFullReportInterval();
    void TestTriggerFullReport();
    void TestHasRegisteredPlugins();
    void TestCheckClockRolling();
    void TestGenerateProcessEntityEvent();
    void TestCollectContextReset();
    void TestCollectContextCalculateFirstCollectTime();
    void TestIsCollectTaskValid();

protected:
    void SetUp() override {
        // 创建模拟的进程目录结构
        bfs::create_directories("./proc/1");
        bfs::create_directories("./proc/2");
        bfs::create_directories("./proc/3");

        // /proc/stat (系统启动时间)
        ofstream ofs_stat("./proc/stat", std::ios::trunc);
        ofs_stat << "cpu  1000 100 200 5000 10 0 5 0 0 0\n";
        ofs_stat << "btime 1731142542\n"; // 系统启动时间
        ofs_stat.close();

        // /proc/1/stat (普通用户进程)
        CreateProcessStat(1, "bash", 0, 1000000, 'S', 0);

        // /proc/2/stat (内核线程 - flags 包含 PF_KTHREAD)
        CreateProcessStat(2, "kworker", 2, 2000000, 'S', 0x00200000);

        // /proc/3/stat (短期进程)
        CreateProcessStat(3, "short", 1, GetRecentStartTime(), 'R', 0);

        // /proc/1/status (用户信息)
        CreateProcessStatus(1, "testuser");

        // /proc/1/cmdline
        CreateProcessCmdline(1, "/bin/bash\0-l\0");

        // /proc/1/exe (符号链接)
        CreateProcessExe(1, "/bin/bash");

        PROCESS_DIR = "./proc";
    }

    void TearDown() override {
        bfs::remove_all("./proc");
        PROCESS_DIR = "/proc";
    }

    void CreateProcessStat(pid_t pid, const string& name, pid_t ppid, int64_t startTime, char state, uint32_t flags) {
        string path = "./proc/" + to_string(pid) + "/stat";
        ofstream ofs(path, std::ios::trunc);
        ofs << pid << " (" << name << ") " << state << " " << ppid << " 1 1 34816 1 " << flags << " "
            << "1110 0 0 0 100 50 0 0 20 0 1 0 " << startTime
            << " 4505600 171 18446744073709551615 4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0";
        ofs.close();
    }

    void CreateProcessStatus(pid_t pid, const string& user) {
        string path = "./proc/" + to_string(pid) + "/status";
        ofstream ofs(path, std::ios::trunc);
        ofs << "Name:   test\n";
        ofs << "Uid:    1000    1000    1000    1000\n"; // UID for user lookup
        ofs << "Gid:    1000    1000    1000    1000\n";
        ofs << "VmSize:  1000 kB\n";
        ofs.close();
    }

    void CreateProcessCmdline(pid_t pid, const string& cmdline) {
        string path = "./proc/" + to_string(pid) + "/cmdline";
        ofstream ofs(path, std::ios::trunc | std::ios::binary);
        ofs.write(cmdline.c_str(), cmdline.size());
        ofs.close();
    }

    void CreateProcessExe(pid_t pid, const string& exePath) {
        string linkPath = "./proc/" + to_string(pid) + "/exe";
        // 在单元测试中，我们创建一个文件来模拟符号链接
        ofstream ofs(linkPath, std::ios::trunc);
        ofs << exePath;
        ofs.close();
    }

    int64_t GetRecentStartTime() {
        // 返回一个最近的启动时间（当前时间 - 5秒）
        SystemInformation systemInfo;
        SystemInterface::GetInstance()->GetSystemInformation(systemInfo);
        time_t now = time(nullptr);
        return (now - systemInfo.bootTime - 5) * SYSTEM_HERTZ;
    }
};

void ProcessEntityRunnerUnittest::TestGetCurrentProcessPrimaryKeys() {
    auto runner = ProcessEntityRunner::GetInstance();
    time_t now = time(nullptr);

    auto pidMap = runner->GetCurrentProcessPrimaryKeys(now);

    // 应该至少找到一些进程（实际系统进程）
    APSARA_TEST_TRUE(!pidMap.empty());

    // 验证 PID 和 startTime 都被正确设置
    for (const auto& [pid, key] : pidMap) {
        APSARA_TEST_EQUAL(pid, key.pid);
        APSARA_TEST_TRUE(key.startTime > 0);
    }
}

void ProcessEntityRunnerUnittest::TestGetProcessEntityInfo() {
    auto runner = ProcessEntityRunner::GetInstance();
    time_t now = time(nullptr);

    ProcessEntityInfo info;
    bool success = runner->GetProcessEntityInfo(1, now, info);

    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(1, info.pid);
    APSARA_TEST_EQUAL("bash", info.comm);
    APSARA_TEST_TRUE(info.startTime > 0);
    // cmdline 和 exe 依赖于 SystemInterface 的实现，可能为空
}

void ProcessEntityRunnerUnittest::TestUpdateVariableAttributes() {
    auto runner = ProcessEntityRunner::GetInstance();
    time_t now = time(nullptr);

    // 先获取进程信息（初始：ppid=0, state='S'）
    ProcessEntityInfo info;
    APSARA_TEST_TRUE(runner->GetProcessEntityInfo(1, now, info));

    // 保存原始值
    auto originalPpid = info.ppid;
    auto originalState = info.state;

    // 修改系统文件：改变 ppid 和 state
    CreateProcessStat(1, "bash", 999, 1000000, 'R', 0); // ppid 从 0 改为 999，state 从 'S' 改为 'R'

    // 更新可变属性（应该从系统重新读取）
    bool success = runner->UpdateVariableAttributes(1, now, info);
    APSARA_TEST_TRUE(success);

    // 验证属性被更新为新的系统值（与原始值不同）
    APSARA_TEST_NOT_EQUAL(originalPpid, info.ppid);
    APSARA_TEST_NOT_EQUAL(originalState, info.state);
    APSARA_TEST_EQUAL(999, info.ppid); // 验证新值
    APSARA_TEST_EQUAL('R', info.state); // 验证新值
    APSARA_TEST_EQUAL(now, info.lastVariableUpdateTime);
}

void ProcessEntityRunnerUnittest::TestShouldCollectProcessKernelThread() {
    auto runner = ProcessEntityRunner::GetInstance();

    ProcessEntityInfo info;
    info.pid = 2;
    info.flags = 0x00200000; // PF_KTHREAD

    ProcessFilterConfig config;
    config.excludeKernelThreads = true;

    // 应该被过滤掉
    APSARA_TEST_FALSE(runner->ShouldCollectProcess(2, info, config));

    // 不过滤内核线程
    config.excludeKernelThreads = false;
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(2, info, config));
}

void ProcessEntityRunnerUnittest::TestShouldCollectProcessMinRunningTime() {
    auto runner = ProcessEntityRunner::GetInstance();

    ProcessEntityInfo info;
    info.pid = 3;
    info.flags = 0; // 不是内核线程

    SystemInformation systemInfo;
    SystemInterface::GetInstance()->GetSystemInformation(systemInfo);

    // 设置一个刚启动的进程（运行时间 < 20秒）
    time_t now = time(nullptr);
    info.startTime = (now - systemInfo.bootTime - 5) * SYSTEM_HERTZ; // 运行了5秒

    ProcessFilterConfig config;
    config.excludeKernelThreads = false;
    config.minRunningTimeSeconds = 20; // 最小运行时间20秒

    // 应该被过滤掉（运行时间不够）
    APSARA_TEST_FALSE(runner->ShouldCollectProcess(3, info, config));

    // 设置为不过滤
    config.minRunningTimeSeconds = 0;
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(3, info, config));

    // 设置一个运行时间足够长的进程
    info.startTime = (now - systemInfo.bootTime - 30) * SYSTEM_HERTZ; // 运行了30秒
    config.minRunningTimeSeconds = 20;
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(3, info, config));
}

void ProcessEntityRunnerUnittest::TestShouldCollectProcessBlacklist() {
    auto runner = ProcessEntityRunner::GetInstance();

    ProcessEntityInfo info;
    info.pid = 1;
    info.flags = 0;
    info.comm = "bash";
    info.exe = "/bin/bash";

    ProcessFilterConfig config;
    config.excludeKernelThreads = false;
    config.minRunningTimeSeconds = 0;

    // 黑名单匹配进程名
    config.blacklistPatterns = {"bash"};
    APSARA_TEST_FALSE(runner->ShouldCollectProcess(1, info, config));

    // 黑名单匹配可执行文件路径
    config.blacklistPatterns = {"/bin/.*"};
    APSARA_TEST_FALSE(runner->ShouldCollectProcess(1, info, config));

    // 黑名单不匹配
    config.blacklistPatterns = {"python"};
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(1, info, config));
}

void ProcessEntityRunnerUnittest::TestShouldCollectProcessWhitelist() {
    auto runner = ProcessEntityRunner::GetInstance();

    ProcessEntityInfo info;
    info.pid = 1;
    info.flags = 0;
    info.comm = "bash";
    info.exe = "/bin/bash";

    ProcessFilterConfig config;
    config.excludeKernelThreads = false;
    config.minRunningTimeSeconds = 0;

    // 白名单匹配进程名
    config.whitelistPatterns = {"bash"};
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(1, info, config));

    // 白名单匹配可执行文件路径
    config.whitelistPatterns = {"/bin/.*"};
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(1, info, config));

    // 白名单不匹配（应该被过滤）
    config.whitelistPatterns = {"python"};
    APSARA_TEST_FALSE(runner->ShouldCollectProcess(1, info, config));

    // 空白名单（不过滤）
    config.whitelistPatterns = {};
    APSARA_TEST_TRUE(runner->ShouldCollectProcess(1, info, config));
}

void ProcessEntityRunnerUnittest::TestDetectChangesAdded() {
    auto runner = ProcessEntityRunner::GetInstance();

    // 旧缓存：空
    std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash> oldCache;

    // 当前进程：有一个新进程
    std::unordered_map<pid_t, ProcessPrimaryKey> currentPidMap;
    ProcessPrimaryKey key1{1, 1000};
    currentPidMap[1] = key1;

    auto changes = runner->DetectChanges(oldCache, currentPidMap);

    APSARA_TEST_EQUAL(1, changes.added.size());
    APSARA_TEST_EQUAL(0, changes.removed.size());
    APSARA_TEST_EQUAL(0, changes.reused.size());
    APSARA_TEST_EQUAL(1, changes.added[0].pid);
    APSARA_TEST_EQUAL(1000, changes.added[0].startTime);
}

void ProcessEntityRunnerUnittest::TestDetectChangesRemoved() {
    auto runner = ProcessEntityRunner::GetInstance();

    // 旧缓存：有一个进程
    std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash> oldCache;
    ProcessPrimaryKey key1{1, 1000};
    ProcessEntityInfo info1;
    info1.pid = 1;
    info1.startTime = 1000;
    oldCache[key1] = info1;

    // 当前进程：空（进程已退出）
    std::unordered_map<pid_t, ProcessPrimaryKey> currentPidMap;

    auto changes = runner->DetectChanges(oldCache, currentPidMap);

    APSARA_TEST_EQUAL(0, changes.added.size());
    APSARA_TEST_EQUAL(1, changes.removed.size());
    APSARA_TEST_EQUAL(0, changes.reused.size());
    APSARA_TEST_EQUAL(1, changes.removed[0].pid);
    APSARA_TEST_EQUAL(1000, changes.removed[0].startTime);
}

void ProcessEntityRunnerUnittest::TestDetectChangesReused() {
    auto runner = ProcessEntityRunner::GetInstance();

    // 旧缓存：PID 1, startTime 1000
    std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash> oldCache;
    ProcessPrimaryKey oldKey{1, 1000};
    ProcessEntityInfo info1;
    info1.pid = 1;
    info1.startTime = 1000;
    oldCache[oldKey] = info1;

    // 当前进程：PID 1, startTime 2000（PID被复用）
    std::unordered_map<pid_t, ProcessPrimaryKey> currentPidMap;
    ProcessPrimaryKey newKey{1, 2000};
    currentPidMap[1] = newKey;

    auto changes = runner->DetectChanges(oldCache, currentPidMap);

    APSARA_TEST_EQUAL(0, changes.added.size());
    APSARA_TEST_EQUAL(1, changes.removed.size()); // 旧进程被标记为移除
    APSARA_TEST_EQUAL(1, changes.reused.size()); // 新进程被标记为复用

    // 验证复用的是新进程
    APSARA_TEST_EQUAL(1, changes.reused[0].pid);
    APSARA_TEST_EQUAL(2000, changes.reused[0].startTime);

    // 验证移除的是旧进程
    APSARA_TEST_EQUAL(1, changes.removed[0].pid);
    APSARA_TEST_EQUAL(1000, changes.removed[0].startTime);
}

void ProcessEntityRunnerUnittest::TestProcessPrimaryKeyHash() {
    ProcessPrimaryKey key1{1, 1000};
    ProcessPrimaryKey key2{1, 1000};
    ProcessPrimaryKey key3{1, 2000};
    ProcessPrimaryKey key4{2, 1000};

    ProcessPrimaryKeyHash hasher;

    // 相同的键应该有相同的哈希值
    APSARA_TEST_EQUAL(hasher(key1), hasher(key2));

    // 不同的键应该有不同的哈希值（通常情况下）
    APSARA_TEST_NOT_EQUAL(hasher(key1), hasher(key3));
    APSARA_TEST_NOT_EQUAL(hasher(key1), hasher(key4));

    // 测试相等性运算符
    APSARA_TEST_TRUE(key1 == key2);
    APSARA_TEST_FALSE(key1 == key3);
    APSARA_TEST_FALSE(key1 == key4);
    APSARA_TEST_TRUE(key1 != key3);
}

void ProcessEntityRunnerUnittest::TestProcessFilterConfig() {
    ProcessFilterConfig config;

    // 测试默认值
    APSARA_TEST_EQUAL(20, config.minRunningTimeSeconds);
    APSARA_TEST_TRUE(config.excludeKernelThreads);
    APSARA_TEST_TRUE(config.whitelistPatterns.empty());
    APSARA_TEST_TRUE(config.blacklistPatterns.empty());

    // 测试设置值
    config.minRunningTimeSeconds = 60;
    config.excludeKernelThreads = false;
    config.whitelistPatterns = {"pattern1", "pattern2"};
    config.blacklistPatterns = {"pattern3"};

    APSARA_TEST_EQUAL(60, config.minRunningTimeSeconds);
    APSARA_TEST_FALSE(config.excludeKernelThreads);
    APSARA_TEST_EQUAL(2, config.whitelistPatterns.size());
    APSARA_TEST_EQUAL(1, config.blacklistPatterns.size());
}

void ProcessEntityRunnerUnittest::TestCollectContextTimeManagement() {
    QueueKey queueKey = 1; // QueueKey is int64_t
    ProcessFilterConfig filterConfig;
    auto incrementalInterval = std::chrono::seconds(10);
    auto fullReportInterval = std::chrono::seconds(3600);

    auto context = std::make_shared<ProcessEntityCollectContext>(
        "test_config", queueKey, 0, filterConfig, incrementalInterval, fullReportInterval);

    // 测试初始化
    APSARA_TEST_EQUAL("test_config", context->mConfigName);
    APSARA_TEST_EQUAL(10, context->mIncrementalInterval.count());
    APSARA_TEST_EQUAL(3600, context->mFullReportInterval.count());

    // 测试时间设置
    auto scheduleTime = std::chrono::steady_clock::now();
    time_t metricTime = time(nullptr);
    context->SetTime(scheduleTime, metricTime);

    APSARA_TEST_EQUAL(scheduleTime, context->GetScheduleTime());
    APSARA_TEST_EQUAL(metricTime, context->GetMetricTime());

    // 测试全量上报时间（应该在构造时设置为很早的时间，以便立即触发第一次全量上报）
    auto timeSinceLastFull = std::chrono::steady_clock::now() - context->lastFullReportTime;
    APSARA_TEST_TRUE(timeSinceLastFull >= fullReportInterval);
}

void ProcessEntityRunnerUnittest::TestInitAndStop() {
    auto runner = ProcessEntityRunner::GetInstance();

    // 测试 Init
    runner->Init();
    APSARA_TEST_TRUE(runner->mIsStarted);

    // 测试 Stop
    runner->Stop();
    APSARA_TEST_FALSE(runner->mIsStarted);
}

void ProcessEntityRunnerUnittest::TestRegisterAndRemoveConfig() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;

    // 注册配置
    runner->RegisterConfig("test_config_1", queueKey, 0, filterConfig, 10, 3600);

    // 验证配置已注册
    APSARA_TEST_TRUE(runner->HasRegisteredPlugins());
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(1, runner->mContexts.size());
        APSARA_TEST_TRUE(runner->mContexts.find("test_config_1") != runner->mContexts.end());
    }

    // 移除配置
    runner->RemoveConfig("test_config_1");
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(0, runner->mContexts.size());
    }
    APSARA_TEST_FALSE(runner->HasRegisteredPlugins());

    runner->Stop();
}

void ProcessEntityRunnerUnittest::TestRegisterConfigDuplicate() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;

    // 第一次注册
    runner->RegisterConfig("test_config_duplicate", queueKey, 0, filterConfig, 10, 3600);

    // 验证注册成功
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(1, runner->mContexts.size());
    }

    // 第二次注册相同的 configName，应该被拒绝
    runner->RegisterConfig("test_config_duplicate", queueKey, 1, filterConfig, 10, 3600);

    // 验证仍然只有一个配置（第二次注册被拒绝）
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(1, runner->mContexts.size());
        // 验证 inputIndex 仍然是第一次注册的值（0）
        APSARA_TEST_EQUAL(0, runner->mContexts["test_config_duplicate"]->mInputIndex);
    }

    runner->RemoveConfig("test_config_duplicate");
    runner->Stop();
}

void ProcessEntityRunnerUnittest::TestRemoveAllConfigs() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;

    // 注册多个配置
    runner->RegisterConfig("config1", queueKey, 0, filterConfig, 10, 3600);
    runner->RegisterConfig("config2", queueKey, 1, filterConfig, 10, 3600);
    runner->RegisterConfig("config3", queueKey, 2, filterConfig, 10, 3600);

    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(3, runner->mContexts.size());
    }

    // 移除所有配置
    runner->RemoveAllConfigs();

    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        APSARA_TEST_EQUAL(0, runner->mContexts.size());
    }
    APSARA_TEST_FALSE(runner->HasRegisteredPlugins());

    runner->Stop();
}

void ProcessEntityRunnerUnittest::TestSetFullReportInterval() {
    auto runner = ProcessEntityRunner::GetInstance();

    // 设置全量上报间隔
    auto newInterval = std::chrono::seconds(7200);
    runner->SetFullReportInterval(newInterval);

    APSARA_TEST_EQUAL(7200, runner->mFullReportInterval.count());
}

void ProcessEntityRunnerUnittest::TestTriggerFullReport() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;

    // 注册配置
    runner->RegisterConfig("test_trigger", queueKey, 0, filterConfig, 10, 3600);

    // 记录当前时间
    auto beforeTrigger = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 触发全量上报
    runner->TriggerFullReport("test_trigger");

    // 验证 lastFullReportTime 被重置
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        auto context = runner->mContexts["test_trigger"];
        // lastFullReportTime 应该被设置为一个很早的时间
        APSARA_TEST_TRUE(context->lastFullReportTime < beforeTrigger);
    }

    runner->RemoveConfig("test_trigger");
    runner->Stop();
}

void ProcessEntityRunnerUnittest::TestHasRegisteredPlugins() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    // 初始状态：没有注册的插件
    APSARA_TEST_FALSE(runner->HasRegisteredPlugins());

    // 注册一个配置
    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;
    runner->RegisterConfig("test_has_plugin", queueKey, 0, filterConfig, 10, 3600);

    // 应该有注册的插件
    APSARA_TEST_TRUE(runner->HasRegisteredPlugins());

    // 移除配置
    runner->RemoveConfig("test_has_plugin");

    // 再次检查：没有注册的插件
    APSARA_TEST_FALSE(runner->HasRegisteredPlugins());

    runner->Stop();
}

void ProcessEntityRunnerUnittest::TestCheckClockRolling() {
    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;
    auto incrementalInterval = std::chrono::seconds(10);
    auto fullReportInterval = std::chrono::seconds(3600);

    auto context = std::make_shared<ProcessEntityCollectContext>(
        "test_clock_rolling", queueKey, 0, filterConfig, incrementalInterval, fullReportInterval);

    // 设置正常的时间
    auto steadyTime = std::chrono::steady_clock::now();
    time_t metricTime = time(nullptr);
    context->SetTime(steadyTime, metricTime);

    // 正常情况：不应该检测到时钟跳跃
    APSARA_TEST_FALSE(context->CheckClockRolling());

    // 模拟时钟跳跃：修改 metricTime，使其与 scheduleTime 不一致
    // 将 metricTime 向前跳跃 120 秒（超过 60 秒阈值）
    context->mMetricTime = metricTime + 120;

    // 应该检测到时钟跳跃
    APSARA_TEST_TRUE(context->CheckClockRolling());
}

void ProcessEntityRunnerUnittest::TestGenerateProcessEntityEvent() {
    auto runner = ProcessEntityRunner::GetInstance();

    ProcessEntityInfo info;
    info.pid = 12345;
    info.startTime = 1000000;
    info.startTimeUnix = time(nullptr) - 3600; // 1小时前启动
    info.comm = "test_process";
    info.exe = "/usr/bin/test";
    info.cmdline = "/usr/bin/test --arg1 --arg2";
    info.ppid = 1;
    info.user = "testuser";
    info.state = 'S';

    // 创建事件组
    PipelineEventGroup group(std::make_shared<SourceBuffer>());

    // 生成事件（全量上报）
    runner->GenerateProcessEntityEvent(&group, info, true);

    // 验证事件生成成功
    APSARA_TEST_EQUAL(1, group.GetEvents().size());

    auto& event = group.GetEvents()[0];
    APSARA_TEST_TRUE(event.Is<LogEvent>());

    auto& logEvent = event.Cast<LogEvent>();

    // 验证主键字段
    auto pidValue = logEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_PID);
    APSARA_TEST_EQUAL("12345", pidValue.to_string());

    auto commValue = logEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_COMM);
    APSARA_TEST_EQUAL("test_process", commValue.to_string());

    auto ppidValue = logEvent.GetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID);
    APSARA_TEST_EQUAL("1", ppidValue.to_string());
}

void ProcessEntityRunnerUnittest::TestCollectContextReset() {
    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;
    auto incrementalInterval = std::chrono::seconds(10);
    auto fullReportInterval = std::chrono::seconds(3600);

    auto context = std::make_shared<ProcessEntityCollectContext>(
        "test_reset", queueKey, 0, filterConfig, incrementalInterval, fullReportInterval);

    // 设置一些初始时间
    auto initialScheduleTime = std::chrono::steady_clock::now();
    time_t initialMetricTime = time(nullptr);
    context->SetTime(initialScheduleTime, initialMetricTime);

    // 调用 Reset
    context->Reset();

    // 验证时间被重新计算（应该对齐到下一个采集间隔）
    APSARA_TEST_TRUE(context->GetScheduleTime() > initialScheduleTime);
    APSARA_TEST_TRUE(context->GetMetricTime() > initialMetricTime);
}

void ProcessEntityRunnerUnittest::TestCollectContextCalculateFirstCollectTime() {
    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;
    auto incrementalInterval = std::chrono::seconds(10);
    auto fullReportInterval = std::chrono::seconds(3600);

    auto context = std::make_shared<ProcessEntityCollectContext>(
        "test_calculate", queueKey, 0, filterConfig, incrementalInterval, fullReportInterval);

    auto steadyClockNow = std::chrono::steady_clock::now();
    time_t metricTimeNow = time(nullptr);

    // 调用 CalculateFirstCollectTime
    context->CalculateFirstCollectTime(metricTimeNow, steadyClockNow);

    // 验证首次采集时间被对齐到下一个采集间隔的整数倍
    time_t firstMetricTime = context->GetMetricTime();
    APSARA_TEST_TRUE(firstMetricTime >= metricTimeNow);

    // 验证时间对齐：首次采集时间应该是 incrementalInterval 的整数倍
    APSARA_TEST_EQUAL(0, firstMetricTime % incrementalInterval.count());
}

void ProcessEntityRunnerUnittest::TestIsCollectTaskValid() {
    auto runner = ProcessEntityRunner::GetInstance();
    runner->Init();

    QueueKey queueKey = 123;
    ProcessFilterConfig filterConfig;

    // 注册配置
    runner->RegisterConfig("test_valid", queueKey, 0, filterConfig, 10, 3600);

    // 获取配置的启动时间
    auto startTime = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(runner->mConfigMutex);
        auto context = runner->mContexts["test_valid"];
        startTime = context->mStartTime;
    }

    // 验证任务有效
    APSARA_TEST_TRUE(runner->IsCollectTaskValid(startTime, "test_valid"));

    // 验证无效的配置名
    APSARA_TEST_FALSE(runner->IsCollectTaskValid(startTime, "invalid_config"));

    // 验证错误的启动时间
    auto wrongStartTime = startTime - std::chrono::seconds(100);
    APSARA_TEST_FALSE(runner->IsCollectTaskValid(wrongStartTime, "test_valid"));

    runner->RemoveConfig("test_valid");
    runner->Stop();
}

UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestGetCurrentProcessPrimaryKeys);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestGetProcessEntityInfo);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestUpdateVariableAttributes);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestShouldCollectProcessKernelThread);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestShouldCollectProcessMinRunningTime);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestShouldCollectProcessBlacklist);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestShouldCollectProcessWhitelist);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestDetectChangesAdded);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestDetectChangesRemoved);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestDetectChangesReused);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestProcessPrimaryKeyHash);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestProcessFilterConfig);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestCollectContextTimeManagement);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestInitAndStop);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestRegisterAndRemoveConfig);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestRegisterConfigDuplicate);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestRemoveAllConfigs);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestSetFullReportInterval);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestTriggerFullReport);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestHasRegisteredPlugins);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestCheckClockRolling);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestGenerateProcessEntityEvent);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestCollectContextReset);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestCollectContextCalculateFirstCollectTime);
UNIT_TEST_CASE(ProcessEntityRunnerUnittest, TestIsCollectTaskValid);

} // namespace logtail

UNIT_TEST_MAIN
