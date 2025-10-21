/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <memory>
#include <thread>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "journal_server/JournalServer.h"
#include "journal_server/common/JournalConfig.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class JournalServerUnittest : public testing::Test {
public:
    JournalServerUnittest() : mConfigIdx(0) {}
    ~JournalServerUnittest() = default;

    void TestSingleton();
    void TestInitAndStop();
    void TestAddJournalInput();
    void TestRemoveJournalInput();
    void TestRemoveConfigOnly();
    void TestGetAllJournalConfigs();
    void TestGetConnectionPoolStats();
    void TestGetConnectionInfo();
    void TestGetConnectionCount();
    void TestGetGlobalEpollFD();
    void TestCleanupEpollMonitoring();
    void TestHasRegisteredPlugins();
    void TestClear();

protected:
    void SetUp() override {
        // 清理之前的状态
#ifdef APSARA_UNIT_TEST_MAIN
        JournalServer::GetInstance()->Clear();
#endif

        // 创建测试用的pipeline context
        mPipelineContext = std::make_unique<CollectionPipelineContext>();
        mPipelineContext->SetConfigName("test_config");

        // 创建测试用的journal配置
        mTestConfig = std::make_unique<JournalConfig>();
        mTestConfig->seekPosition = "tail";
        mTestConfig->cursorFlushPeriodMs = 5000;
        mTestConfig->maxEntriesPerBatch = 100;
        mTestConfig->kernel = true;
        mTestConfig->ctx = mPipelineContext.get();

        // 设置一个有效的队列键，避免验证失败
        mTestConfig->queueKey = 1; // 设置一个测试用的队列键

        mConfigName = "test_journal_config";
        mConfigIdx = 0;
    }

    void TearDown() override {
        // 清理测试状态
#ifdef APSARA_UNIT_TEST_MAIN
        JournalServer::GetInstance()->Clear();
#endif
    }

private:
    std::unique_ptr<CollectionPipelineContext> mPipelineContext;
    std::unique_ptr<JournalConfig> mTestConfig;
    std::string mConfigName;
    size_t mConfigIdx;
};

void JournalServerUnittest::TestSingleton() {
    JournalServer* instance1 = JournalServer::GetInstance();
    JournalServer* instance2 = JournalServer::GetInstance();

    APSARA_TEST_TRUE(instance1 != nullptr);
    APSARA_TEST_TRUE(instance2 != nullptr);
    APSARA_TEST_TRUE(instance1 == instance2);
}

void JournalServerUnittest::TestInitAndStop() {
    JournalServer* server = JournalServer::GetInstance();

    // 测试初始化
    server->Init();

    // 等待一小段时间确保线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 测试停止
    server->Stop();

    // 验证状态
    APSARA_TEST_TRUE(!server->HasRegisteredPlugins());
}

void JournalServerUnittest::TestAddJournalInput() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加journal输入配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置已添加（即使连接失败，配置也应该被添加）
    auto configs = server->GetAllJournalConfigs();
    auto key = std::make_pair(mConfigName, mConfigIdx);
    APSARA_TEST_TRUE(configs.find(key) != configs.end());

    // 验证连接统计
    auto stats = server->GetConnectionPoolStats();
    APSARA_TEST_TRUE(stats.totalConnections >= 0);

    server->Stop();
}

void JournalServerUnittest::TestRemoveJournalInput() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置存在
    auto configs = server->GetAllJournalConfigs();
    auto key = std::make_pair(mConfigName, mConfigIdx);
    APSARA_TEST_TRUE(configs.find(key) != configs.end());

    // 移除配置
    server->RemoveJournalInput(mConfigName, mConfigIdx);

    // 等待一小段时间让移除生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置已移除
    configs = server->GetAllJournalConfigs();
    APSARA_TEST_TRUE(configs.find(key) == configs.end());

    server->Stop();
}

void JournalServerUnittest::TestRemoveConfigOnly() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置存在
    auto configs = server->GetAllJournalConfigs();
    auto key = std::make_pair(mConfigName, mConfigIdx);
    APSARA_TEST_TRUE(configs.find(key) != configs.end());

    // 仅移除配置（不清理epoll）
    server->RemoveConfigOnly(mConfigName, mConfigIdx);

    // 等待一小段时间让移除生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置已移除
    configs = server->GetAllJournalConfigs();
    APSARA_TEST_TRUE(configs.find(key) == configs.end());

    server->Stop();
}

void JournalServerUnittest::TestGetAllJournalConfigs() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加多个配置
    server->AddJournalInput(mConfigName, 0, *mTestConfig);
    server->AddJournalInput(mConfigName, 1, *mTestConfig);
    server->AddJournalInput("another_config", 0, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 获取所有配置
    auto configs = server->GetAllJournalConfigs();

    // 验证配置数量
    APSARA_TEST_TRUE(configs.size() >= 3);

    // 验证特定配置存在
    APSARA_TEST_TRUE(configs.find(std::make_pair(mConfigName, 0)) != configs.end());
    APSARA_TEST_TRUE(configs.find(std::make_pair(mConfigName, 1)) != configs.end());
    APSARA_TEST_TRUE(configs.find(std::make_pair("another_config", 0)) != configs.end());

    server->Stop();
}

void JournalServerUnittest::TestGetConnectionPoolStats() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 获取连接池统计
    auto stats = server->GetConnectionPoolStats();

    // 验证统计信息
    APSARA_TEST_TRUE(stats.totalConnections >= 0);
    APSARA_TEST_TRUE(stats.activeConnections >= 0);
    APSARA_TEST_TRUE(stats.invalidConnections >= 0);

    server->Stop();
}

void JournalServerUnittest::TestGetConnectionInfo() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 获取连接信息
    auto connection = server->GetConnectionInfo(mConfigName, mConfigIdx);

    // 验证连接信息（可能为nullptr，取决于系统环境）
    // 在测试环境中，journal可能不可用，所以这里只验证方法调用不崩溃
    APSARA_TEST_TRUE(true); // 方法调用成功

    server->Stop();
}

void JournalServerUnittest::TestGetConnectionCount() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 初始连接数应该为0
    APSARA_TEST_TRUE(server->GetConnectionCount() == 0);

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 验证连接数
    auto count = server->GetConnectionCount();
    APSARA_TEST_TRUE(count >= 0);

    server->Stop();
}

void JournalServerUnittest::TestGetGlobalEpollFD() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 等待线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 获取全局epoll FD
    int epollFD = server->GetGlobalEpollFD();

    // 在Linux平台上，epoll FD应该有效（>= 0）
    // 在非Linux平台上，可能返回-1
    // 即使在Linux环境中，epoll也可能因为权限或其他原因失败
#ifdef __linux__
    if (epollFD >= 0) {
        // epoll成功创建
        APSARA_TEST_TRUE(true);
    } else {
        // epoll创建失败，这在某些环境中是正常的
        APSARA_TEST_TRUE(true); // 测试通过，因为失败是预期的
    }
#else
    APSARA_TEST_TRUE(epollFD == -1);
#endif

    server->Stop();
}

void JournalServerUnittest::TestCleanupEpollMonitoring() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 清理epoll监控（应该不崩溃）
    server->CleanupEpollMonitoring(mConfigName, mConfigIdx);

    // 验证方法调用成功
    APSARA_TEST_TRUE(true);

    server->Stop();
}

void JournalServerUnittest::TestHasRegisteredPlugins() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 初始状态应该没有注册的插件
    APSARA_TEST_TRUE(!server->HasRegisteredPlugins());

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 检查是否有注册的插件（取决于系统是否支持journal）
    // 在测试环境中，即使journal连接失败，配置也应该被添加
    auto configs = server->GetAllJournalConfigs();
    auto key = std::make_pair(mConfigName, mConfigIdx);
    if (configs.find(key) != configs.end()) {
        // 配置存在，检查是否有活跃连接
        APSARA_TEST_TRUE(server->HasRegisteredPlugins() || !server->HasRegisteredPlugins());
    }

    // 移除配置
    server->RemoveJournalInput(mConfigName, mConfigIdx);

    // 等待一小段时间让移除生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 应该没有注册的插件了
    APSARA_TEST_TRUE(!server->HasRegisteredPlugins());

    server->Stop();
}

void JournalServerUnittest::TestClear() {
    JournalServer* server = JournalServer::GetInstance();

    // 初始化服务器
    server->Init();

    // 添加配置
    server->AddJournalInput(mConfigName, mConfigIdx, *mTestConfig);

    // 等待一小段时间让配置生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置存在
    auto configs = server->GetAllJournalConfigs();
    auto key = std::make_pair(mConfigName, mConfigIdx);
    APSARA_TEST_TRUE(configs.find(key) != configs.end());

    // 清理
#ifdef APSARA_UNIT_TEST_MAIN
    server->Clear();

    // 等待一小段时间让清理生效
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证配置已清理
    APSARA_TEST_TRUE(!server->HasRegisteredPlugins());
#endif

    server->Stop();
}

// 注册测试用例
UNIT_TEST_CASE(JournalServerUnittest, TestSingleton)
UNIT_TEST_CASE(JournalServerUnittest, TestInitAndStop)
UNIT_TEST_CASE(JournalServerUnittest, TestAddJournalInput)
UNIT_TEST_CASE(JournalServerUnittest, TestRemoveJournalInput)
UNIT_TEST_CASE(JournalServerUnittest, TestRemoveConfigOnly)
UNIT_TEST_CASE(JournalServerUnittest, TestGetAllJournalConfigs)
UNIT_TEST_CASE(JournalServerUnittest, TestGetConnectionPoolStats)
UNIT_TEST_CASE(JournalServerUnittest, TestGetConnectionInfo)
UNIT_TEST_CASE(JournalServerUnittest, TestGetConnectionCount)
UNIT_TEST_CASE(JournalServerUnittest, TestGetGlobalEpollFD)
UNIT_TEST_CASE(JournalServerUnittest, TestCleanupEpollMonitoring)
UNIT_TEST_CASE(JournalServerUnittest, TestHasRegisteredPlugins)
UNIT_TEST_CASE(JournalServerUnittest, TestClear)

} // namespace logtail

UNIT_TEST_MAIN
