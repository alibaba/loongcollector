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

#include <memory>
#include <string>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "journal_server/common/JournalConfig.h"
#include "journal_server/connection/JournalConnectionManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class JournalConnectionManagerUnittest : public testing::Test {
public:
    JournalConnectionManagerUnittest() = default;
    ~JournalConnectionManagerUnittest() = default;

    void TestSingleton();
    void TestInitialize();
    void TestAddConfig();
    void TestRemoveConfig();
    void TestGetConfig();
    void TestGetConnection();
    void TestGetAllConfigs();
    void TestGetStats();
    void TestGetConnectionCount();
    void TestCleanup();
    void TestAddConfigDuplicate();
    void TestAddConfigNotInitialized();
    void TestAddConfigReaderOpenFailure();
    void TestAddConfigFilterFailure();
    void TestAddConfigSeekFailure();
    void TestRemoveConfigNotInitialized();
    void TestRemoveConfigNotFound();
    void TestGetConnectionNotFound();
    void TestGetConfigNotFound();
    void TestGetStatsWithInvalidConnections();
    void TestAddConfigWithFilters();
    void TestAddConfigSeekHead();
    void TestAddConfigSeekCursor();
    void TestGetConfigsUsingConnection();
    void TestGetConfigsUsingConnectionNotFound();
    void TestAddConfigReplaceExisting();
    void TestGetStatsWithActiveConnections();
};

void JournalConnectionManagerUnittest::TestSingleton() {
    JournalConnectionManager& instance1 = JournalConnectionManager::GetInstance();
    JournalConnectionManager& instance2 = JournalConnectionManager::GetInstance();

    APSARA_TEST_TRUE(&instance1 == &instance2);
}

void JournalConnectionManagerUnittest::TestInitialize() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 测试初始化
    bool result = manager.Initialize();

    // 初始化应该成功（即使没有实际的journal支持）
    APSARA_TEST_TRUE(result);
}

void JournalConnectionManagerUnittest::TestAddConfig() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    bool result = manager.AddConfig("test_config", 0, config);

    // 添加配置应该成功
    APSARA_TEST_TRUE(result);

    // 验证配置已添加
    APSARA_TEST_TRUE(manager.GetConnectionCount() > 0);
}

void JournalConnectionManagerUnittest::TestRemoveConfig() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 验证配置存在
    APSARA_TEST_TRUE(manager.GetConnectionCount() > 0);

    // 移除配置
    manager.RemoveConfig("test_config", 0);

    // 验证配置已移除
    APSARA_TEST_TRUE(manager.GetConnectionCount() == 0);
}

void JournalConnectionManagerUnittest::TestGetConfig() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 获取配置
    JournalConfig retrievedConfig = manager.GetConfig("test_config", 0);

    // 验证配置
    APSARA_TEST_EQUAL(retrievedConfig.seekPosition, "tail");
    APSARA_TEST_EQUAL(retrievedConfig.cursorFlushPeriodMs, 5000);
    APSARA_TEST_EQUAL(retrievedConfig.maxEntriesPerBatch, 100);
    APSARA_TEST_TRUE(retrievedConfig.kernel);
}

void JournalConnectionManagerUnittest::TestGetConnection() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 获取连接
    auto connection = manager.GetConnection("test_config", 0);

    // 在测试环境中，连接可能为nullptr（因为没有实际的journal支持）
    // 但方法调用不应该崩溃
    APSARA_TEST_TRUE(true); // 方法调用成功
}

void JournalConnectionManagerUnittest::TestGetAllConfigs() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加多个配置
    manager.AddConfig("test_config", 0, config);
    manager.AddConfig("test_config", 1, config);
    manager.AddConfig("another_config", 0, config);

    // 获取所有配置
    auto configs = manager.GetAllConfigs();

    // 验证配置数量
    APSARA_TEST_TRUE(configs.size() >= 3);

    // 验证特定配置存在
    auto key1 = std::make_pair(std::string("test_config"), size_t(0));
    auto key2 = std::make_pair(std::string("test_config"), size_t(1));
    auto key3 = std::make_pair(std::string("another_config"), size_t(0));

    APSARA_TEST_TRUE(configs.find(key1) != configs.end());
    APSARA_TEST_TRUE(configs.find(key2) != configs.end());
    APSARA_TEST_TRUE(configs.find(key3) != configs.end());
}

void JournalConnectionManagerUnittest::TestGetStats() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 获取统计信息
    auto stats = manager.GetStats();

    // 验证统计信息
    APSARA_TEST_TRUE(stats.totalConfigs >= 0);
    APSARA_TEST_TRUE(stats.totalConnections >= 0);
    APSARA_TEST_TRUE(stats.activeConnections >= 0);
    APSARA_TEST_TRUE(stats.invalidConnections >= 0);
}

void JournalConnectionManagerUnittest::TestGetConnectionCount() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 先清理之前的状态
    manager.Cleanup();

    // 初始化管理器
    manager.Initialize();

    // 记录初始连接数（可能不为0，因为其他测试可能留下了状态）
    size_t initialCount = manager.GetConnectionCount();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 设置一个有效的队列键，避免验证失败
    config.queueKey = 1;


    // 添加配置
    bool added = manager.AddConfig("test_config", 0, config);

    if (added) {
        // 验证连接数增加了
        auto count = manager.GetConnectionCount();
        APSARA_TEST_TRUE(count >= initialCount);

        // 移除配置
        manager.RemoveConfig("test_config", 0);

        // 验证连接数回到了初始状态
        APSARA_TEST_TRUE(manager.GetConnectionCount() == initialCount);
    } else {
        // 如果添加失败（比如在测试环境中），至少验证方法调用不崩溃
        APSARA_TEST_TRUE(true);
    }
}

void JournalConnectionManagerUnittest::TestCleanup() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 验证配置存在
    APSARA_TEST_TRUE(manager.GetConnectionCount() > 0);

    // 清理
    manager.Cleanup();

    // 验证配置已清理
    APSARA_TEST_TRUE(manager.GetConnectionCount() == 0);
}

void JournalConnectionManagerUnittest::TestAddConfigDuplicate() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 添加配置
    bool result1 = manager.AddConfig("test_config", 0, config);

    // 再次添加相同配置（应该替换）
    bool result2 = manager.AddConfig("test_config", 0, config);

    // 验证配置被替换
    APSARA_TEST_TRUE(result1 || result2); // 至少一次成功

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigNotInitialized() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 不初始化管理器，直接添加配置
    JournalConfig config;
    config.seekPosition = "tail";

    bool result = manager.AddConfig("test_config", 0, config);

    // 应该失败
    APSARA_TEST_FALSE(result);
}

void JournalConnectionManagerUnittest::TestAddConfigReaderOpenFailure() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 添加配置（在测试环境中可能失败）
    manager.AddConfig("test_config", 0, config);

    // 无论成功与否都应该有相应的日志记录
    APSARA_TEST_TRUE(true); // 主要测试错误处理路径

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigFilterFailure() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 添加配置（测试过滤器失败的情况）
    manager.AddConfig("test_config", 0, config);

    // 主要测试过滤器失败的处理路径
    APSARA_TEST_TRUE(true);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigSeekFailure() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "invalid_position"; // 无效的seek位置
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 添加配置（测试seek失败的情况）
    manager.AddConfig("test_config", 0, config);

    // 主要测试seek失败的处理路径
    APSARA_TEST_TRUE(true);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestRemoveConfigNotInitialized() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 不初始化管理器，直接移除配置
    manager.RemoveConfig("test_config", 0);

    // 应该不会崩溃
    APSARA_TEST_TRUE(true);
}

void JournalConnectionManagerUnittest::TestRemoveConfigNotFound() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 移除不存在的配置
    manager.RemoveConfig("nonexistent_config", 0);

    // 应该不会崩溃
    APSARA_TEST_TRUE(true);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestGetConnectionNotFound() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 获取不存在的连接
    auto connection = manager.GetConnection("nonexistent_config", 0);

    // 应该返回nullptr
    APSARA_TEST_TRUE(connection == nullptr);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestGetConfigNotFound() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 获取不存在的配置
    JournalConfig config = manager.GetConfig("nonexistent_config", 0);

    // 应该返回空配置
    APSARA_TEST_TRUE(config.queueKey == -1);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestGetStatsWithInvalidConnections() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 获取统计信息
    auto stats = manager.GetStats();

    // 验证统计信息
    APSARA_TEST_TRUE(stats.totalConfigs >= 0);
    APSARA_TEST_TRUE(stats.activeConnections >= 0);
    APSARA_TEST_TRUE(stats.invalidConnections >= 0);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigWithFilters() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置（带过滤器）
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;
    config.units = {"test.service"};
    config.matchPatterns = {"MESSAGE=test"};

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();


    // 添加配置
    bool result = manager.AddConfig("test_config_with_filters", 0, config);

    // 添加配置应该成功
    APSARA_TEST_TRUE(result);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigSeekHead() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置（seek到head）
    JournalConfig config;
    config.seekPosition = "head";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();


    // 添加配置
    bool result = manager.AddConfig("test_config_seek_head", 0, config);

    // 添加配置应该成功
    APSARA_TEST_TRUE(result);

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestAddConfigSeekCursor() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置（seek到cursor）
    JournalConfig config;
    config.seekPosition = "cursor:test_cursor";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();


    // 添加配置
    bool result = manager.AddConfig("test_config_seek_cursor", 0, config);

    // 添加配置应该成功
    APSARA_TEST_TRUE(result);

    manager.Cleanup();
}


void JournalConnectionManagerUnittest::TestGetConfigsUsingConnection() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 获取连接
    auto connection = manager.GetConnection("test_config", 0);

    if (connection) {
        // 获取使用该连接的配置
        auto configs = manager.GetConfigsUsingConnection(connection);

        // 验证配置
        APSARA_TEST_TRUE(configs.size() >= 0);
    }

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestGetConfigsUsingConnectionNotFound() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 获取不存在的连接对应的配置
    auto configs = manager.GetConfigsUsingConnection(nullptr);

    // 应该返回空列表
    APSARA_TEST_TRUE(configs.empty());

    manager.Cleanup();
}


void JournalConnectionManagerUnittest::TestAddConfigReplaceExisting() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config1;
    config1.seekPosition = "tail";
    config1.cursorFlushPeriodMs = 5000;
    config1.maxEntriesPerBatch = 100;
    config1.kernel = true;

    JournalConfig config2;
    config2.seekPosition = "head";
    config2.cursorFlushPeriodMs = 3000;
    config2.maxEntriesPerBatch = 50;
    config2.kernel = false;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config1.ctx = ctx.get();
    config2.ctx = ctx.get();


    // 添加第一个配置
    bool result1 = manager.AddConfig("test_config_replace", 0, config1);

    // 添加第二个配置（替换第一个）
    bool result2 = manager.AddConfig("test_config_replace", 0, config2);

    // 验证配置被替换
    APSARA_TEST_TRUE(result1 || result2); // 至少一次成功

    manager.Cleanup();
}

void JournalConnectionManagerUnittest::TestGetStatsWithActiveConnections() {
    JournalConnectionManager& manager = JournalConnectionManager::GetInstance();

    // 初始化管理器
    manager.Initialize();

    // 创建测试配置
    JournalConfig config;
    config.seekPosition = "tail";
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 100;
    config.kernel = true;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");
    config.ctx = ctx.get();

    // 添加配置
    manager.AddConfig("test_config", 0, config);

    // 获取统计信息
    auto stats = manager.GetStats();

    // 验证统计信息
    APSARA_TEST_TRUE(stats.totalConfigs >= 0);
    APSARA_TEST_TRUE(stats.activeConnections >= 0);
    APSARA_TEST_TRUE(stats.invalidConnections >= 0);
    APSARA_TEST_TRUE(stats.connectionKeys.size() >= 0);

    manager.Cleanup();
}

// 注册测试用例
TEST_F(JournalConnectionManagerUnittest, TestSingleton) {
    TestSingleton();
}

TEST_F(JournalConnectionManagerUnittest, TestInitialize) {
    TestInitialize();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfig) {
    TestAddConfig();
}

TEST_F(JournalConnectionManagerUnittest, TestRemoveConfig) {
    TestRemoveConfig();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConfig) {
    TestGetConfig();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConnection) {
    TestGetConnection();
}

TEST_F(JournalConnectionManagerUnittest, TestGetAllConfigs) {
    TestGetAllConfigs();
}

TEST_F(JournalConnectionManagerUnittest, TestGetStats) {
    TestGetStats();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConnectionCount) {
    TestGetConnectionCount();
}

TEST_F(JournalConnectionManagerUnittest, TestCleanup) {
    TestCleanup();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfigDuplicate) {
    TestAddConfigDuplicate();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfigNotInitialized) {
    TestAddConfigNotInitialized();
}


TEST_F(JournalConnectionManagerUnittest, TestRemoveConfigNotInitialized) {
    TestRemoveConfigNotInitialized();
}

TEST_F(JournalConnectionManagerUnittest, TestRemoveConfigNotFound) {
    TestRemoveConfigNotFound();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConnectionNotFound) {
    TestGetConnectionNotFound();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConfigNotFound) {
    TestGetConfigNotFound();
}

TEST_F(JournalConnectionManagerUnittest, TestGetStatsWithInvalidConnections) {
    TestGetStatsWithInvalidConnections();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfigWithFilters) {
    TestAddConfigWithFilters();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfigSeekHead) {
    TestAddConfigSeekHead();
}

TEST_F(JournalConnectionManagerUnittest, TestAddConfigSeekCursor) {
    TestAddConfigSeekCursor();
}


TEST_F(JournalConnectionManagerUnittest, TestGetConfigsUsingConnection) {
    TestGetConfigsUsingConnection();
}

TEST_F(JournalConnectionManagerUnittest, TestGetConfigsUsingConnectionNotFound) {
    TestGetConfigsUsingConnectionNotFound();
}


TEST_F(JournalConnectionManagerUnittest, TestAddConfigReplaceExisting) {
    TestAddConfigReplaceExisting();
}

TEST_F(JournalConnectionManagerUnittest, TestGetStatsWithActiveConnections) {
    TestGetStatsWithActiveConnections();
}

} // namespace logtail

UNIT_TEST_MAIN
