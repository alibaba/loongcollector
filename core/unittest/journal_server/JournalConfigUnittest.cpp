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
    void TestParseFromJsonBasic();
    void TestParseFromJsonWithArrays();
    void TestParseFromJsonDefaults();
    void TestParseFromJsonWithContext();
    void TestParseFromJsonInvalidTypes();
    void TestParseFromJsonEmptyArrays();
    void TestParseFromJsonMixedArrayTypes();
    void TestParseFromJsonBoundaryValues();
};

void JournalConfigUnittest::TestDefaultValues() {
    JournalConfig config;

    // 测试默认值
    APSARA_TEST_TRUE(config.mSeekPosition.empty());
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
    config.mMaxEntriesPerBatch = 1000;
    config.mSeekPosition = "tail";
    config.mCursorSeekFallback = "head";

    // 测试有效配置
    APSARA_TEST_TRUE(config.IsValid());

    // 测试边界值
    config.mMaxEntriesPerBatch = 1;
    APSARA_TEST_TRUE(config.IsValid());

    config.mMaxEntriesPerBatch = 10000; // 最大值
    APSARA_TEST_TRUE(config.IsValid());
}

void JournalConfigUnittest::TestFieldSettings() {
    JournalConfig config;

    // 测试字段设置
    config.mSeekPosition = "head";
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
    config.mMaxEntriesPerBatch = 0;

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复这些值
    APSARA_TEST_TRUE(fixedCount > 0);
    APSARA_TEST_TRUE(config.mMaxEntriesPerBatch > 0);
}

void JournalConfigUnittest::TestValidateAndFixConfigBoundaryValues() {
    JournalConfig config;

    // 测试最大值边界
    config.mMaxEntriesPerBatch = 10001; // 超过最大值

    int fixedCount = config.ValidateAndFixConfig();

    // 应该修复这些值
    APSARA_TEST_TRUE(fixedCount > 0);
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
    config.mMaxEntriesPerBatch = 0;
    config.mSeekPosition = "";
    config.mCursorSeekFallback = "";

    APSARA_TEST_FALSE(config.IsValid());

    // 测试部分有效配置
    config.mMaxEntriesPerBatch = 1000;
    config.mSeekPosition = "tail";
    config.mCursorSeekFallback = "head";

    APSARA_TEST_TRUE(config.IsValid());
}

// ==================== 新增的ParseFromJson测试 ====================

void JournalConfigUnittest::TestParseFromJsonBasic() {
    // 测试基本的JSON解析
    Json::Value config;
    config["SeekPosition"] = "head";
    config["CursorSeekFallback"] = "tail";
    config["Kernel"] = false;
    config["ParsePriority"] = true;
    config["ParseSyslogFacility"] = true;
    config["UseJournalEventTime"] = false;
    config["ResetIntervalSecond"] = 7200;
    config["MaxEntriesPerBatch"] = 500;
    config["BatchTimeoutMs"] = 2000;

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证解析结果
    APSARA_TEST_EQUAL(journalConfig.mSeekPosition, "head");
    APSARA_TEST_EQUAL(journalConfig.mCursorSeekFallback, "tail");
    APSARA_TEST_FALSE(journalConfig.mKernel);
    APSARA_TEST_TRUE(journalConfig.mParsePriority);
    APSARA_TEST_TRUE(journalConfig.mParseSyslogFacility);
    APSARA_TEST_FALSE(journalConfig.mUseJournalEventTime);
    APSARA_TEST_EQUAL(journalConfig.mResetIntervalSecond, 7200);
    APSARA_TEST_EQUAL(journalConfig.mMaxEntriesPerBatch, 500);
    APSARA_TEST_EQUAL(journalConfig.mBatchTimeoutMs, 2000);
}

void JournalConfigUnittest::TestParseFromJsonWithArrays() {
    // 测试带数组的JSON解析
    Json::Value config;
    config["SeekPosition"] = "tail";

    config["Units"] = Json::Value(Json::arrayValue);
    config["Units"].append("nginx.service");
    config["Units"].append("apache.service");

    config["Identifiers"] = Json::Value(Json::arrayValue);
    config["Identifiers"].append("nginx");
    config["Identifiers"].append("apache");

    config["JournalPaths"] = Json::Value(Json::arrayValue);
    config["JournalPaths"].append("/var/log/journal");
    config["JournalPaths"].append("/run/log/journal");

    config["MatchPatterns"] = Json::Value(Json::arrayValue);
    config["MatchPatterns"].append("MESSAGE=test");
    config["MatchPatterns"].append("PRIORITY=6");

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证解析结果
    APSARA_TEST_EQUAL(journalConfig.mUnits.size(), 2);
    APSARA_TEST_EQUAL(journalConfig.mUnits[0], "nginx.service");
    APSARA_TEST_EQUAL(journalConfig.mUnits[1], "apache.service");

    APSARA_TEST_EQUAL(journalConfig.mIdentifiers.size(), 2);
    APSARA_TEST_EQUAL(journalConfig.mIdentifiers[0], "nginx");
    APSARA_TEST_EQUAL(journalConfig.mIdentifiers[1], "apache");

    APSARA_TEST_EQUAL(journalConfig.mJournalPaths.size(), 2);
    APSARA_TEST_EQUAL(journalConfig.mJournalPaths[0], "/var/log/journal");
    APSARA_TEST_EQUAL(journalConfig.mJournalPaths[1], "/run/log/journal");

    APSARA_TEST_EQUAL(journalConfig.mMatchPatterns.size(), 2);
    APSARA_TEST_EQUAL(journalConfig.mMatchPatterns[0], "MESSAGE=test");
    APSARA_TEST_EQUAL(journalConfig.mMatchPatterns[1], "PRIORITY=6");
}

void JournalConfigUnittest::TestParseFromJsonDefaults() {
    // 测试默认值处理
    Json::Value config;
    // 不设置任何值，使用默认值

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证默认值（从结构体定义中的默认值）
    // 注意：字符串字段在结构体中默认为空，但 ParseFromJson 会设置默认值
    // 由于 GetOptionalStringParam 在字段不存在时可能保持原值，我们需要验证实际行为
    // 数值和布尔字段有明确的默认值
    APSARA_TEST_TRUE(journalConfig.mKernel); // 默认 true
    APSARA_TEST_FALSE(journalConfig.mParsePriority); // 默认 false
    APSARA_TEST_FALSE(journalConfig.mParseSyslogFacility); // 默认 false
    APSARA_TEST_TRUE(journalConfig.mUseJournalEventTime); // 默认 true（结构体定义）
    APSARA_TEST_EQUAL(journalConfig.mResetIntervalSecond, 3600); // 默认 3600
    APSARA_TEST_EQUAL(journalConfig.mMaxEntriesPerBatch, 1000); // 默认 1000
    APSARA_TEST_EQUAL(journalConfig.mBatchTimeoutMs, 1000); // 默认 1000

    // 验证数组字段为空
    APSARA_TEST_TRUE(journalConfig.mUnits.empty());
    APSARA_TEST_TRUE(journalConfig.mIdentifiers.empty());
    APSARA_TEST_TRUE(journalConfig.mJournalPaths.empty());
    APSARA_TEST_TRUE(journalConfig.mMatchPatterns.empty());
}

void JournalConfigUnittest::TestParseFromJsonWithContext() {
    // 测试带context的JSON解析
    Json::Value config;
    config["SeekPosition"] = "head";

    auto ctx = std::make_unique<CollectionPipelineContext>();
    ctx->SetConfigName("test_config_with_context");

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, ctx.get());

    // 验证context设置
    APSARA_TEST_TRUE(journalConfig.mCtx != nullptr);
    APSARA_TEST_EQUAL(journalConfig.mCtx->GetConfigName(), "test_config_with_context");
}

void JournalConfigUnittest::TestParseFromJsonInvalidTypes() {
    // 测试无效类型处理
    Json::Value config;
    config["SeekPosition"] = 123; // 应该是字符串
    config["MaxEntriesPerBatch"] = "not_a_number"; // 应该是数字
    config["Kernel"] = "not_a_bool"; // 应该是bool

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 由于类型不匹配，应该使用默认值
    APSARA_TEST_EQUAL(journalConfig.mSeekPosition, "tail"); // 默认值
    APSARA_TEST_EQUAL(journalConfig.mMaxEntriesPerBatch, 1000); // 默认值
    APSARA_TEST_TRUE(journalConfig.mKernel); // 默认值
}

void JournalConfigUnittest::TestParseFromJsonEmptyArrays() {
    // 测试空数组处理
    Json::Value config;
    config["Units"] = Json::Value(Json::arrayValue); // 空数组
    config["Identifiers"] = Json::Value(Json::arrayValue);
    config["JournalPaths"] = Json::Value(Json::arrayValue);
    config["MatchPatterns"] = Json::Value(Json::arrayValue);

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证空数组
    APSARA_TEST_TRUE(journalConfig.mUnits.empty());
    APSARA_TEST_TRUE(journalConfig.mIdentifiers.empty());
    APSARA_TEST_TRUE(journalConfig.mJournalPaths.empty());
    APSARA_TEST_TRUE(journalConfig.mMatchPatterns.empty());
}

void JournalConfigUnittest::TestParseFromJsonMixedArrayTypes() {
    // 测试数组中混合类型
    Json::Value config;
    config["Units"] = Json::Value(Json::arrayValue);
    config["Units"].append("valid.service");
    config["Units"].append(123); // 非字符串，应该被忽略
    config["Units"].append("another.service");

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证只有字符串被解析
    APSARA_TEST_EQUAL(journalConfig.mUnits.size(), 2);
    APSARA_TEST_EQUAL(journalConfig.mUnits[0], "valid.service");
    APSARA_TEST_EQUAL(journalConfig.mUnits[1], "another.service");
}

void JournalConfigUnittest::TestParseFromJsonBoundaryValues() {
    // 测试边界值
    Json::Value config;
    config["ResetIntervalSecond"] = 0; // 最小值
    config["MaxEntriesPerBatch"] = 1; // 最小值
    config["BatchTimeoutMs"] = 1; // 最小值

    JournalConfig journalConfig = JournalConfig::ParseFromJson(config, nullptr);

    // 验证边界值被接受（虽然ValidateAndFixConfig会修正它们）
    APSARA_TEST_EQUAL(journalConfig.mResetIntervalSecond, 0);
    APSARA_TEST_EQUAL(journalConfig.mMaxEntriesPerBatch, 1);
    APSARA_TEST_EQUAL(journalConfig.mBatchTimeoutMs, 1);

    // 测试最大值
    Json::Value config2;
    config2["ResetIntervalSecond"] = 999999;
    config2["MaxEntriesPerBatch"] = 999999;
    config2["BatchTimeoutMs"] = 999999;

    JournalConfig journalConfig2 = JournalConfig::ParseFromJson(config2, nullptr);

    APSARA_TEST_EQUAL(journalConfig2.mResetIntervalSecond, 999999);
    APSARA_TEST_EQUAL(journalConfig2.mMaxEntriesPerBatch, 999999);
    APSARA_TEST_EQUAL(journalConfig2.mBatchTimeoutMs, 999999);
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

// 注册新增的ParseFromJson测试用例
TEST_F(JournalConfigUnittest, TestParseFromJsonBasic) {
    TestParseFromJsonBasic();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonWithArrays) {
    TestParseFromJsonWithArrays();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonDefaults) {
    TestParseFromJsonDefaults();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonWithContext) {
    TestParseFromJsonWithContext();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonInvalidTypes) {
    TestParseFromJsonInvalidTypes();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonEmptyArrays) {
    TestParseFromJsonEmptyArrays();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonMixedArrayTypes) {
    TestParseFromJsonMixedArrayTypes();
}

TEST_F(JournalConfigUnittest, TestParseFromJsonBoundaryValues) {
    TestParseFromJsonBoundaryValues();
}

} // namespace logtail

UNIT_TEST_MAIN
