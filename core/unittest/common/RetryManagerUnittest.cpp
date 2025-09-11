// Copyright 2025 loongcollector Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/RetryManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class RetryManagerUnittest : public ::testing::Test {
public:
    void TestFixedBackoffStrategy();
    void TestLinearBackoffStrategy();
    void TestExponentialBackoffStrategy();
    void TestCustomBackoffStrategy();
    void TestShouldRetry();
    void TestRecordFailure();
    void TestResetFailureState();
    void TestUpdateConfig();
    void TestSetCustomStrategy();

private:
    RetryManager::RetryConfig
    createTestConfig(BackoffStrategyType strategyType, int64_t baseInterval = 1, int64_t maxInterval = 10);
};

RetryManager::RetryConfig
RetryManagerUnittest::createTestConfig(BackoffStrategyType strategyType, int64_t baseInterval, int64_t maxInterval) {
    RetryManager::RetryConfig config;
    config.baseRetryInterval = baseInterval;
    config.maxRetryInterval = maxInterval;
    config.strategyType = strategyType;
    return config;
}

void RetryManagerUnittest::TestFixedBackoffStrategy() {
    FixedBackoffStrategy strategy;
    APSARA_TEST_EQUAL(5, strategy.CalculateInterval(1, 5, 100));
    APSARA_TEST_EQUAL(5, strategy.CalculateInterval(5, 5, 100));
    APSARA_TEST_EQUAL(5, strategy.CalculateInterval(10, 5, 100));
}

void RetryManagerUnittest::TestLinearBackoffStrategy() {
    LinearBackoffStrategy strategy;
    APSARA_TEST_EQUAL(2, strategy.CalculateInterval(1, 2, 20));
    APSARA_TEST_EQUAL(4, strategy.CalculateInterval(2, 2, 20));
    APSARA_TEST_EQUAL(6, strategy.CalculateInterval(3, 2, 20));
    APSARA_TEST_EQUAL(20, strategy.CalculateInterval(15, 2, 20));
}

void RetryManagerUnittest::TestExponentialBackoffStrategy() {
    ExponentialBackoffStrategy strategy;
    APSARA_TEST_EQUAL(1, strategy.CalculateInterval(1, 1, 16));
    APSARA_TEST_EQUAL(2, strategy.CalculateInterval(2, 1, 16));
    APSARA_TEST_EQUAL(8, strategy.CalculateInterval(4, 1, 16));
    APSARA_TEST_EQUAL(16, strategy.CalculateInterval(5, 1, 16));
    APSARA_TEST_EQUAL(16, strategy.CalculateInterval(6, 1, 16));
}

void RetryManagerUnittest::TestCustomBackoffStrategy() {
    // 测试自定义策略：平方退避
    auto customFunc = [](int32_t failureCount, int64_t baseInterval, int64_t maxInterval) -> int64_t {
        int64_t interval = baseInterval * failureCount * failureCount;
        return interval > maxInterval ? maxInterval : interval;
    };

    CustomBackoffStrategy strategy(customFunc);

    APSARA_TEST_EQUAL(1, strategy.CalculateInterval(1, 1, 100));
    APSARA_TEST_EQUAL(4, strategy.CalculateInterval(2, 1, 100));
    APSARA_TEST_EQUAL(9, strategy.CalculateInterval(3, 1, 100));
    APSARA_TEST_EQUAL(100, strategy.CalculateInterval(11, 1, 100));
}

void RetryManagerUnittest::TestShouldRetry() {
    auto config = createTestConfig(BackoffStrategyType::FIXED, 1, 10);
    RetryManager retryManager(config);

    APSARA_TEST_TRUE(retryManager.ShouldRetry());

    retryManager.RecordFailure();
    APSARA_TEST_FALSE(retryManager.ShouldRetry());

    retryManager.ResetFailureState();
    APSARA_TEST_TRUE(retryManager.ShouldRetry());
}

void RetryManagerUnittest::TestRecordFailure() {
    auto config = createTestConfig(BackoffStrategyType::FIXED, 1, 10);
    RetryManager retryManager(config);

    APSARA_TEST_EQUAL(0, retryManager.GetFailureCount());

    retryManager.RecordFailure();
    APSARA_TEST_EQUAL(1, retryManager.GetFailureCount());

    retryManager.RecordFailure();
    APSARA_TEST_EQUAL(2, retryManager.GetFailureCount());

    retryManager.ResetFailureState();
    APSARA_TEST_EQUAL(0, retryManager.GetFailureCount());
}

void RetryManagerUnittest::TestResetFailureState() {
    auto config = createTestConfig(BackoffStrategyType::EXPONENTIAL, 1, 10);
    RetryManager retryManager(config);

    retryManager.RecordFailure();
    retryManager.RecordFailure();
    retryManager.RecordFailure();

    APSARA_TEST_EQUAL(3, retryManager.GetFailureCount());
    APSARA_TEST_FALSE(retryManager.ShouldRetry());

    retryManager.ResetFailureState();

    APSARA_TEST_EQUAL(0, retryManager.GetFailureCount());
    APSARA_TEST_TRUE(retryManager.ShouldRetry());
}

void RetryManagerUnittest::TestUpdateConfig() {
    auto config1 = createTestConfig(BackoffStrategyType::FIXED, 1, 10);
    RetryManager retryManager(config1);

    retryManager.RecordFailure();
    APSARA_TEST_EQUAL(1, retryManager.GetFailureCount());

    auto config2 = createTestConfig(BackoffStrategyType::LINEAR, 2, 20);
    retryManager.UpdateConfig(config2);

    APSARA_TEST_EQUAL(1, retryManager.GetFailureCount());
}

void RetryManagerUnittest::TestSetCustomStrategy() {
    auto config = createTestConfig(BackoffStrategyType::FIXED, 1, 10);
    RetryManager retryManager(config);

    auto customFunc = [](int32_t failureCount, int64_t baseInterval, int64_t maxInterval) -> int64_t {
        int64_t interval = baseInterval * failureCount * failureCount;
        return interval > maxInterval ? maxInterval : interval;
    };

    retryManager.SetCustomStrategy(customFunc);

    retryManager.RecordFailure();
    retryManager.RecordFailure();
    retryManager.RecordFailure();

    // 允许2秒的时间误差
    APSARA_TEST_TRUE(retryManager.GetNextRetryTime() >= std::time(nullptr) + 7);
}


UNIT_TEST_CASE(RetryManagerUnittest, TestFixedBackoffStrategy)
UNIT_TEST_CASE(RetryManagerUnittest, TestLinearBackoffStrategy)
UNIT_TEST_CASE(RetryManagerUnittest, TestExponentialBackoffStrategy)
UNIT_TEST_CASE(RetryManagerUnittest, TestCustomBackoffStrategy)
UNIT_TEST_CASE(RetryManagerUnittest, TestShouldRetry)
UNIT_TEST_CASE(RetryManagerUnittest, TestRecordFailure)
UNIT_TEST_CASE(RetryManagerUnittest, TestResetFailureState)
UNIT_TEST_CASE(RetryManagerUnittest, TestUpdateConfig)
UNIT_TEST_CASE(RetryManagerUnittest, TestSetCustomStrategy)

} // namespace logtail

UNIT_TEST_MAIN
