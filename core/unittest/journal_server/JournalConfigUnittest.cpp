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
    void TestValidateAndFixConfigEdgeCases();
    void TestValidateAndFixConfigBoundaryValues();
    void TestValidateAndFixConfigInvalidSeekPosition();
    void TestValidateAndFixConfigInvalidCursorSeekFallback();
    void TestValidateAndFixConfigEmptyStringArrays();
    void TestValidateAndFixConfigInvalidJournalPaths();
    void TestIsValidEdgeCases();
};

void JournalConfigUnittest::TestDefaultValues() {
    JournalConfig config;

    // 测试默认值
    APSARA_TEST_TRUE(config.mSeekPosition.empty());
    APSARA_TEST_EQUAL(config.mCursorFlushPeriodMs, 5000);
    APSARA_TEST_TRUE(config.mCursorSeekFallback.empty());
    APSARA_TEST_TRUE(config.mUnits.empty());
    APSARA_TEST_TRUE(config.mIdentifiers.empty());
    APSARA_TEST_TRUE(config.mMatchPatterns.empty());
    APSARA_TEST_TRUE(config.mKernel);
    APSARA_TEST_EQUAL(config.mMaxEntriesPerBatch, 1000);
    APSARA_TEST_FALSE(config.mParsePriority);
    APSARA_TEST_FALSE(config.mParseSyslogFacility);
    APSARA_TEST_TRUE(config.mUseJournalEventTime);
    APSARA_TEST_TRUE(config.mJournalPaths.empty());
    APSARA_TEST_TRUE(config.mCtx == nullptr);
    APSARA_TEST_EQUAL(config.mQueueKey, -1);
}

void JournalConfigUnittest::TestValidateAndFixConfig() {
    JournalConfig config;

    // 设置一些无效值
    config.mCursorFlushPeriodMs = -1000; // 无效值
    config.mMaxEntriesPerBatch = 0; // 无效值
    config.mSeekPosition = "invalid_position"; // 无效值
    config.mCursorSeekFallback = "invalid_fallback"; // 无效值
    config.mUnits = {"valid_unit", "", "another_valid_unit"}; // 包含空字符串
    config.mIdentifiers = {"", "valid_id"}; // 包含空字符串
    config.mMatchPatterns = {"valid_pattern", ""}; // 包含空字符串
    config.mJournalPaths = {"valid_path", "", "another_valid_path"}; // 包含空字符串

    // 执行验证和修正
    int fixedCount = config.ValidateAndFixConfig();

    // 验证修正结果
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.mCursorFlushPeriodMs, 5000); // 修正为默认值
    APSARA_TEST_EQUAL(config.mMaxEntriesPerBatch, 1000); // 修正为默认值
    APSARA_TEST_EQUAL(config.mSeekPosition, "tail"); // 修正为默认值
    APSARA_TEST_EQUAL(config.mCursorSeekFallback, "head"); // 修正为默认值

    // 验证空字符串被移除
    APSARA_TEST_EQUAL(config.mUnits.size(), 2);
    APSARA_TEST_EQUAL(config.mUnits[0], "valid_unit");
    APSARA_TEST_EQUAL(config.mUnits[1], "another_valid_unit");

    APSARA_TEST_EQUAL(config.mIdentifiers.size(), 1);
    APSARA_TEST_EQUAL(config.mIdentifiers[0], "valid_id");

    APSARA_TEST_EQUAL(config.mMatchPatterns.size(), 1);
    APSARA_TEST_EQUAL(config.mMatchPatterns[0], "valid_pattern");

    APSARA_TEST_EQUAL(config.mJournalPaths.size(), 2);
    APSARA_TEST_EQUAL(config.mJournalPaths[0], "valid_path");
    APSARA_TEST_EQUAL(config.mJournalPaths[1], "another_valid_path");
}

void JournalConfigUnittest::TestIsValid() {
    JournalConfig config;

    // 测试无效配置
    APSARA_TEST_FALSE(config.IsValid());

    // 设置有效值
    config.mCursorFlushPeriodMs = 5000;
    config.mMaxEntriesPerBatch = 1000;
    config.mSeekPosition = "tail";
    config.mCursorSeekFallback = "head";

    // 测试有效配置
    APSARA_TEST_TRUE(config.IsValid());

    // 测试边界值
    config.mCursorFlushPeriodMs = 1;
    config.mMaxEntriesPerBatch = 1;
    APSARA_TEST_TRUE(config.IsValid());

    config.mCursorFlushPeriodMs = 300000; // 最大值
    config.mMaxEntriesPerBatch = 10000; // 最大值
    APSARA_TEST_TRUE(config.IsValid());
}

void JournalConfigUnittest::TestFieldSettings() {
    JournalConfig config;

    // 测试字段设置
    config.mSeekPosition = "head";
    config.mCursorFlushPeriodMs = 10000;
    config.mCursorSeekFallback = "tail";
    config.mUnits = {"nginx.service", "apache.service"};
    config.mIdentifiers = {"nginx", "apache"};
    config.mMatchPatterns = {"*error*", "*warning*"};
    config.mKernel = false;
    config.mMaxEntriesPerBatch = 500;
    config.mParsePriority = true;
    config.mParseSyslogFacility = true;
    config.mUseJournalEventTime = false;
    config.mJournalPaths = {"/var/log/journal", "/run/log/journal"};

    // 验证字段设置
    APSARA_TEST_EQUAL(config.mSeekPosition, "head");
    APSARA_TEST_EQUAL(config.mCursorFlushPeriodMs, 10000);
    APSARA_TEST_EQUAL(config.mCursorSeekFallback, "tail");
    APSARA_TEST_EQUAL(config.mUnits.size(), 2);
    APSARA_TEST_EQUAL(config.mUnits[0], "nginx.service");
    APSARA_TEST_EQUAL(config.mUnits[1], "apache.service");
    APSARA_TEST_EQUAL(config.mIdentifiers.size(), 2);
    APSARA_TEST_EQUAL(config.mIdentifiers[0], "nginx");
    APSARA_TEST_EQUAL(config.mIdentifiers[1], "apache");
    APSARA_TEST_EQUAL(config.mMatchPatterns.size(), 2);
    APSARA_TEST_EQUAL(config.mMatchPatterns[0], "*error*");
    APSARA_TEST_EQUAL(config.mMatchPatterns[1], "*warning*");
    APSARA_TEST_FALSE(config.mKernel);
    APSARA_TEST_EQUAL(config.mMaxEntriesPerBatch, 500);
    APSARA_TEST_TRUE(config.mParsePriority);
    APSARA_TEST_TRUE(config.mParseSyslogFacility);
    APSARA_TEST_FALSE(config.mUseJournalEventTime);
    APSARA_TEST_EQUAL(config.mJournalPaths.size(), 2);
    APSARA_TEST_EQUAL(config.mJournalPaths[0], "/var/log/journal");
    APSARA_TEST_EQUAL(config.mJournalPaths[1], "/run/log/journal");
}

void JournalConfigUnittest::TestContextAssignment() {
    JournalConfig config;

    // 创建pipeline context
    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config");

    // 设置context
    config.mCtx = ctx.get();

    // 验证context设置
    APSARA_TEST_TRUE(config.mCtx != nullptr);
    APSARA_TEST_EQUAL(config.mCtx->GetConfigName(), "test_config");

    // 测试queueKey设置
    config.mQueueKey = 12345;
    APSARA_TEST_EQUAL(config.mQueueKey, 12345);
}

void JournalConfigUnittest::TestValidateAndFixConfigEdgeCases() {
    JournalConfig config;

    // 测试边界值
    config.mCursorFlushPeriodMs = 0;
    config.mMaxEntriesPerBatch = 0;

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复这些值
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_TRUE(config.mCursorFlushPeriodMs > 0);
    APSARA_TEST_TRUE(config.mMaxEntriesPerBatch > 0);
}

void JournalConfigUnittest::TestValidateAndFixConfigBoundaryValues() {
    JournalConfig config;

    // 测试最大值边界
    config.mCursorFlushPeriodMs = 300001; // 超过最大值
    config.mMaxEntriesPerBatch = 10001; // 超过最大值

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复这些值
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.mCursorFlushPeriodMs, 300000);
    APSARA_TEST_EQUAL(config.mMaxEntriesPerBatch, 10000);
}

void JournalConfigUnittest::TestValidateAndFixConfigInvalidSeekPosition() {
    JournalConfig config;

    // 测试无效的seek位置
    config.mSeekPosition = "invalid_position";

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复为默认值
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.mSeekPosition, "tail");
}

void JournalConfigUnittest::TestValidateAndFixConfigInvalidCursorSeekFallback() {
    JournalConfig config;

    // 测试无效的cursor seek fallback
    config.mCursorSeekFallback = "invalid_fallback";

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复为默认值
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.mCursorSeekFallback, "head");
}

void JournalConfigUnittest::TestValidateAndFixConfigEmptyStringArrays() {
    JournalConfig config;

    // 测试包含空字符串的数组
    config.mUnits = {"nginx", "", "apache", ""};
    config.mIdentifiers = {"", "nginx", "", "apache"};
    config.mMatchPatterns = {"*error*", "", "*warning*"};

    int fixedCount = config.ValidateAndFixConfig();

    // 应该移除空字符串
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_EQUAL(config.mUnits.size(), 2);
    APSARA_TEST_EQUAL(config.mIdentifiers.size(), 2);
    APSARA_TEST_EQUAL(config.mMatchPatterns.size(), 2);
}

void JournalConfigUnittest::TestValidateAndFixConfigInvalidJournalPaths() {
    JournalConfig config;

    // 测试无效的journal路径
    config.mJournalPaths = {"", "/var/log/journal", std::string(5000, 'a'), "/run/log/journal"};

    int fixedCount = config.ValidateAndFixConfig();

    // 应该移除无效路径
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_TRUE(config.mJournalPaths.size() <= 3); // 至少移除空字符串和超长路径
}

void JournalConfigUnittest::TestIsValidEdgeCases() {
    JournalConfig config;

    // 测试无效配置
    config.mCursorFlushPeriodMs = 0;
    config.mMaxEntriesPerBatch = 0;
    config.mSeekPosition = "";
    config.mCursorSeekFallback = "";

    APSARA_TEST_FALSE(config.IsValid());

    // 测试部分有效配置
    config.mCursorFlushPeriodMs = 5000;
    config.mMaxEntriesPerBatch = 1000;
    config.mSeekPosition = "tail";
    config.mCursorSeekFallback = "head";

    APSARA_TEST_TRUE(config.IsValid());
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

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigEdgeCases) {
    TestValidateAndFixConfigEdgeCases();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigBoundaryValues) {
    TestValidateAndFixConfigBoundaryValues();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigInvalidSeekPosition) {
    TestValidateAndFixConfigInvalidSeekPosition();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigInvalidCursorSeekFallback) {
    TestValidateAndFixConfigInvalidCursorSeekFallback();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigEmptyStringArrays) {
    TestValidateAndFixConfigEmptyStringArrays();
}

TEST_F(JournalConfigUnittest, TestValidateAndFixConfigInvalidJournalPaths) {
    TestValidateAndFixConfigInvalidJournalPaths();
}

TEST_F(JournalConfigUnittest, TestIsValidEdgeCases) {
    TestIsValidEdgeCases();
}

} // namespace logtail

UNIT_TEST_MAIN
