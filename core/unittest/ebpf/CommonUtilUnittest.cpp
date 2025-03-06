// Copyright 2023 iLogtail Authors
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

#include <cstddef>

#include <array>
#include <regex>
#include <set>
#include <string>

#include "ebpf/util/FrequencyManager.h"
#include "ebpf/util/TraceId.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class CommonUtilUnittest : public ::testing::Test {
public:
    void TestTraceIDGeneration();
    void TestTraceIDFormat();
    void TestTraceIDUniqueness();
    void TestSpanIDGeneration();
    void TestSpanIDFormat();
    void TestSpanIDUniqueness();
    void TestTraceIDConversion();
    void TestSpanIDConversion();

    void TestInitialState();
    void TestPeriodSetting();
    void TestExpiredCheck();
    void TestReset();
    void TestCycleCount();
    void TestMultipleCycles();

protected:
    void SetUp() override {}
    void TearDown() override {}

    // 辅助函数：检查十六进制字符串格式
    bool IsValidHexString(const std::string& str) {
        std::regex hexRegex("^[0-9a-f]+$");
        return std::regex_match(str, hexRegex);
    }
};

void CommonUtilUnittest::TestTraceIDGeneration() {
    // 测试生成的 TraceID 数组大小
    auto traceId = GenerateTraceID();
    APSARA_TEST_EQUAL(traceId.size(), 32UL);

    // 测试生成的数组不全为0
    bool allZero = true;
    for (const auto& byte : traceId) {
        if (byte != 0) {
            allZero = false;
            break;
        }
    }
    APSARA_TEST_FALSE(allZero);
}

void CommonUtilUnittest::TestTraceIDFormat() {
    auto traceId = GenerateTraceID();
    std::string hexString = FromTraceId(traceId);

    // 验证转换后的字符串长度（32字节 = 64个十六进制字符）
    APSARA_TEST_EQUAL(hexString.length(), 64UL);

    // 验证是否为有效的十六进制字符串
    APSARA_TEST_TRUE(IsValidHexString(hexString));
}

void CommonUtilUnittest::TestTraceIDUniqueness() {
    // 生成多个 TraceID 并验证唯一性
    std::set<std::string> traceIds;
    const int numIds = 1000;

    for (int i = 0; i < numIds; ++i) {
        auto traceId = GenerateTraceID();
        std::string hexString = FromTraceId(traceId);
        traceIds.insert(hexString);
    }

    // 验证没有重复的 TraceID
    APSARA_TEST_EQUAL(traceIds.size(), size_t(numIds));
}

void CommonUtilUnittest::TestSpanIDGeneration() {
    // 测试生成的 SpanID 数组大小
    auto spanId = GenerateSpanID();
    APSARA_TEST_EQUAL(spanId.size(), 16UL);

    // 测试生成的数组不全为0
    bool allZero = true;
    for (const auto& byte : spanId) {
        if (byte != 0) {
            allZero = false;
            break;
        }
    }
    APSARA_TEST_FALSE(allZero);
}

void CommonUtilUnittest::TestSpanIDFormat() {
    auto spanId = GenerateSpanID();
    std::string hexString = FromSpanId(spanId);

    // 验证转换后的字符串长度（16字节 = 32个十六进制字符）
    APSARA_TEST_EQUAL(hexString.length(), 32UL);

    // 验证是否为有效的十六进制字符串
    APSARA_TEST_TRUE(IsValidHexString(hexString));
}

void CommonUtilUnittest::TestSpanIDUniqueness() {
    // 生成多个 SpanID 并验证唯一性
    std::set<std::string> spanIds;
    const int numIds = 1000;

    for (int i = 0; i < numIds; ++i) {
        auto spanId = GenerateSpanID();
        std::string hexString = FromSpanId(spanId);
        spanIds.insert(hexString);
    }

    // 验证没有重复的 SpanID
    APSARA_TEST_EQUAL(spanIds.size(), size_t(numIds));
}

void CommonUtilUnittest::TestTraceIDConversion() {
    // 创建一个已知的 TraceID 数组
    std::array<uint8_t, 32> traceId = {};
    for (size_t i = 0; i < traceId.size(); ++i) {
        traceId[i] = static_cast<uint8_t>(i);
    }

    // 转换为字符串
    std::string hexString = FromTraceId(traceId);

    // 验证转换结果
    std::string expected;
    for (size_t i = 0; i < traceId.size(); ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", traceId[i]);
        expected += buf;
    }
    APSARA_TEST_EQUAL(hexString, expected);
}

void CommonUtilUnittest::TestSpanIDConversion() {
    // 创建一个已知的 SpanID 数组
    std::array<uint8_t, 16> spanId = {};
    for (size_t i = 0; i < spanId.size(); ++i) {
        spanId[i] = static_cast<uint8_t>(i);
    }

    // 转换为字符串
    std::string hexString = FromSpanId(spanId);

    // 验证转换结果
    std::string expected;
    for (size_t i = 0; i < spanId.size(); ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", spanId[i]);
        expected += buf;
    }
    APSARA_TEST_EQUAL(hexString, expected);
}


void CommonUtilUnittest::TestInitialState() {
    // 测试初始状态
    FrequencyManager manager;

    // 验证初始周期为0
    APSARA_TEST_EQUAL(manager.Period().count(), 0);

    // 验证初始计数为0
    APSARA_TEST_EQUAL(manager.Count(), 0U);

    // 验证初始状态下已过期（因为周期为0）
    auto now = std::chrono::steady_clock::now();
    APSARA_TEST_TRUE(manager.Expired(now));
}

void CommonUtilUnittest::TestPeriodSetting() {
    // 测试周期设置
    FrequencyManager manager;

    // 设置1秒的周期
    std::chrono::milliseconds period(1000);
    manager.SetPeriod(period);

    // 验证周期设置正确
    APSARA_TEST_EQUAL(manager.Period().count(), 1000);
}

void CommonUtilUnittest::TestExpiredCheck() {
    FrequencyManager manager;

    // 设置100ms的周期
    std::chrono::milliseconds period(100);
    manager.SetPeriod(period);

    auto now = std::chrono::steady_clock::now();
    manager.Reset(now);

    // 验证刚重置后未过期
    APSARA_TEST_FALSE(manager.Expired(now));

    // 验证周期内未过期
    APSARA_TEST_FALSE(manager.Expired(now + std::chrono::milliseconds(50)));

    // 验证到达周期后过期
    APSARA_TEST_TRUE(manager.Expired(now + std::chrono::milliseconds(100)));

    // 验证超过周期后过期
    APSARA_TEST_TRUE(manager.Expired(now + std::chrono::milliseconds(150)));
}

void CommonUtilUnittest::TestReset() {
    FrequencyManager manager;

    // 设置100ms的周期
    std::chrono::milliseconds period(100);
    manager.SetPeriod(period);

    auto now = std::chrono::steady_clock::now();

    // 第一次重置
    manager.Reset(now);
    APSARA_TEST_EQUAL(manager.Next(), now + period);
    APSARA_TEST_EQUAL(manager.Count(), 1);

    // 第二次重置
    auto nextTime = now + std::chrono::milliseconds(200);
    manager.Reset(nextTime);
    APSARA_TEST_EQUAL(manager.Next(), nextTime + period);
    APSARA_TEST_EQUAL(manager.Count(), 2);
}

void CommonUtilUnittest::TestCycleCount() {
    FrequencyManager manager;

    // 设置100ms的周期
    std::chrono::milliseconds period(100);
    manager.SetPeriod(period);

    auto now = std::chrono::steady_clock::now();

    // 验证初始计数
    APSARA_TEST_EQUAL(manager.Count(), 0);

    // 连续重置几次，验证计数增加
    manager.Reset(now);
    APSARA_TEST_EQUAL(manager.Count(), 1);

    manager.Reset(now + std::chrono::milliseconds(100));
    APSARA_TEST_EQUAL(manager.Count(), 2);

    manager.Reset(now + std::chrono::milliseconds(200));
    APSARA_TEST_EQUAL(manager.Count(), 3);
}

void CommonUtilUnittest::TestMultipleCycles() {
    FrequencyManager manager;

    // 设置100ms的周期
    std::chrono::milliseconds period(100);
    manager.SetPeriod(period);

    auto now = std::chrono::steady_clock::now();
    manager.Reset(now);

    // 模拟多个周期的场景
    for (int i = 1; i <= 5; i++) {
        auto cycleTime = now + std::chrono::milliseconds(i * 100);
        APSARA_TEST_TRUE(manager.Expired(cycleTime));
        manager.Reset(cycleTime);
        APSARA_TEST_EQUAL(manager.Count(), i + 1);
        APSARA_TEST_EQUAL(manager.Next(), cycleTime + period);
    }
}

// for trace id util
UNIT_TEST_CASE(CommonUtilUnittest, TestTraceIDGeneration);
UNIT_TEST_CASE(CommonUtilUnittest, TestTraceIDFormat);
UNIT_TEST_CASE(CommonUtilUnittest, TestTraceIDUniqueness);
UNIT_TEST_CASE(CommonUtilUnittest, TestSpanIDGeneration);
UNIT_TEST_CASE(CommonUtilUnittest, TestSpanIDFormat);
UNIT_TEST_CASE(CommonUtilUnittest, TestSpanIDUniqueness);
UNIT_TEST_CASE(CommonUtilUnittest, TestTraceIDConversion);
UNIT_TEST_CASE(CommonUtilUnittest, TestSpanIDConversion);

// for freq manager
UNIT_TEST_CASE(CommonUtilUnittest, TestInitialState);
UNIT_TEST_CASE(CommonUtilUnittest, TestPeriodSetting);
UNIT_TEST_CASE(CommonUtilUnittest, TestExpiredCheck);
UNIT_TEST_CASE(CommonUtilUnittest, TestReset);
UNIT_TEST_CASE(CommonUtilUnittest, TestCycleCount);
UNIT_TEST_CASE(CommonUtilUnittest, TestMultipleCycles);

// for exec id util


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
