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

#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/entity/ProcessEntityRunner.h"
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

} // namespace logtail

UNIT_TEST_MAIN
