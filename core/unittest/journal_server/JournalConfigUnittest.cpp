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
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "journal_server/common/JournalConfig.h"
#include "journal_server/common/JournalUtils.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class JournalConfigUnittest : public testing::Test {
public:
    JournalConfigUnittest() = default;
    ~JournalConfigUnittest() = default;

    void TestDefaultValues();
    void TestValidateAndFixConfig();
    void TestIsValid();
    void TestFieldSettings();
    void TestContextAssignment();
};

void JournalConfigUnittest::TestDefaultValues() {
    JournalConfig config;

    // 测试默认值
    APSARA_TEST_TRUE(config.seekPosition.empty());
    APSARA_TEST_EQUAL(config.cursorFlushPeriodMs, 5000);
    APSARA_TEST_TRUE(config.cursorSeekFallback.empty());
    APSARA_TEST_TRUE(config.units.empty());
    APSARA_TEST_TRUE(config.identifiers.empty());
    APSARA_TEST_TRUE(config.matchPatterns.empty());
    APSARA_TEST_TRUE(config.kernel);
    APSARA_TEST_EQUAL(config.maxEntriesPerBatch, 1000);
    APSARA_TEST_FALSE(config.parsePriority);
    APSARA_TEST_FALSE(config.parseSyslogFacility);
    APSARA_TEST_TRUE(config.useJournalEventTime);
    APSARA_TEST_TRUE(config.journalPaths.empty());
    APSARA_TEST_TRUE(config.ctx == nullptr);
    APSARA_TEST_EQUAL(config.queueKey, -1);
}

void JournalConfigUnittest::TestValidateAndFixConfig() {
    JournalConfig config;

    // 设置一些无效值
    config.cursorFlushPeriodMs = -1000; // 无效值
    config.maxEntriesPerBatch = 0; // 无效值
    config.seekPosition = "invalid_position"; // 无效值
    config.cursorSeekFallback = "invalid_fallback"; // 无效值
    config.units = {"valid_unit", "", "another_valid_unit"}; // 包含空字符串
    config.identifiers = {"", "valid_id"}; // 包含空字符串
    config.matchPatterns = {"valid_pattern", ""}; // 包含空字符串
    config.journalPaths = {"valid_path", "", "another_valid_path"}; // 包含空字符串

    // 执行验证和修正
    int fixedCount = config.ValidateAndFixConfig();

    // 验证修正结果
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.cursorFlushPeriodMs, 5000); // 修正为默认值
    APSARA_TEST_EQUAL(config.maxEntriesPerBatch, 1000); // 修正为默认值
    APSARA_TEST_EQUAL(config.seekPosition, "tail"); // 修正为默认值
    APSARA_TEST_EQUAL(config.cursorSeekFallback, "head"); // 修正为默认值

    // 验证空字符串被移除
    APSARA_TEST_EQUAL(config.units.size(), 2);
    APSARA_TEST_EQUAL(config.units[0], "valid_unit");
    APSARA_TEST_EQUAL(config.units[1], "another_valid_unit");

    APSARA_TEST_EQUAL(config.identifiers.size(), 1);
    APSARA_TEST_EQUAL(config.identifiers[0], "valid_id");

    APSARA_TEST_EQUAL(config.matchPatterns.size(), 1);
    APSARA_TEST_EQUAL(config.matchPatterns[0], "valid_pattern");

    APSARA_TEST_EQUAL(config.journalPaths.size(), 2);
    APSARA_TEST_EQUAL(config.journalPaths[0], "valid_path");
    APSARA_TEST_EQUAL(config.journalPaths[1], "another_valid_path");
}

void JournalConfigUnittest::TestIsValid() {
    JournalConfig config;

    // 测试无效配置
    APSARA_TEST_FALSE(config.IsValid());

    // 设置有效值
    config.cursorFlushPeriodMs = 5000;
    config.maxEntriesPerBatch = 1000;
    config.seekPosition = "tail";
    config.cursorSeekFallback = "head";

    // 测试有效配置
    APSARA_TEST_TRUE(config.IsValid());

    // 测试边界值
    config.cursorFlushPeriodMs = 1;
    config.maxEntriesPerBatch = 1;
    APSARA_TEST_TRUE(config.IsValid());

    config.cursorFlushPeriodMs = 300000; // 最大值
    config.maxEntriesPerBatch = 10000; // 最大值
    APSARA_TEST_TRUE(config.IsValid());
}

void JournalConfigUnittest::TestFieldSettings() {
    JournalConfig config;

    // 测试字段设置
    config.seekPosition = "head";
    config.cursorFlushPeriodMs = 10000;
    config.cursorSeekFallback = "tail";
    config.units = {"nginx.service", "apache.service"};
    config.identifiers = {"nginx", "apache"};
    config.matchPatterns = {"*error*", "*warning*"};
    config.kernel = false;
    config.maxEntriesPerBatch = 500;
    config.parsePriority = true;
    config.parseSyslogFacility = true;
    config.useJournalEventTime = false;
    config.journalPaths = {"/var/log/journal", "/run/log/journal"};

    // 验证字段设置
    APSARA_TEST_EQUAL(config.seekPosition, "head");
    APSARA_TEST_EQUAL(config.cursorFlushPeriodMs, 10000);
    APSARA_TEST_EQUAL(config.cursorSeekFallback, "tail");
    APSARA_TEST_EQUAL(config.units.size(), 2);
    APSARA_TEST_EQUAL(config.units[0], "nginx.service");
    APSARA_TEST_EQUAL(config.units[1], "apache.service");
    APSARA_TEST_EQUAL(config.identifiers.size(), 2);
    APSARA_TEST_EQUAL(config.identifiers[0], "nginx");
    APSARA_TEST_EQUAL(config.identifiers[1], "apache");
    APSARA_TEST_EQUAL(config.matchPatterns.size(), 2);
    APSARA_TEST_EQUAL(config.matchPatterns[0], "*error*");
    APSARA_TEST_EQUAL(config.matchPatterns[1], "*warning*");
    APSARA_TEST_FALSE(config.kernel);
    APSARA_TEST_EQUAL(config.maxEntriesPerBatch, 500);
    APSARA_TEST_TRUE(config.parsePriority);
    APSARA_TEST_TRUE(config.parseSyslogFacility);
    APSARA_TEST_FALSE(config.useJournalEventTime);
    APSARA_TEST_EQUAL(config.journalPaths.size(), 2);
    APSARA_TEST_EQUAL(config.journalPaths[0], "/var/log/journal");
    APSARA_TEST_EQUAL(config.journalPaths[1], "/run/log/journal");
}

void JournalConfigUnittest::TestContextAssignment() {
    JournalConfig config;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");

    // 设置context
    config.ctx = ctx.get();

    // 验证context设置
    APSARA_TEST_TRUE(config.ctx != nullptr);
    APSARA_TEST_EQUAL(config.ctx->GetConfigName(), "test_config");

    // 测试queueKey设置
    config.queueKey = 12345;
    APSARA_TEST_EQUAL(config.queueKey, 12345);
}

// 注册测试用例
TEST_F(JournalConfigUnittest, TestDefaultValues) {
    TestDefaultValues();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfig) {
    TestValidateAndFixConfig();
}

TEST_F(JournalConfigUnittest, TestIsValid) {
    TestIsValid();
}

TEST_F(JournalConfigUnittest, TestFieldSettings) {
    TestFieldSettings();
}

TEST_F(JournalConfigUnittest, TestContextAssignment) {
    TestContextAssignment();
}

} // namespace logtail

UNIT_TEST_MAIN
