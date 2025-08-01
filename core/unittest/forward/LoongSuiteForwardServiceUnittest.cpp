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

#include <json/value.h>

#include <memory>
#include <string>
#include <thread>

#include "common/Flags.h"
#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "protobuf/forward/loongsuite.grpc.pb.h"
#include "runner/ProcessorRunner.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(grpc_server_forward_max_retry_times);

using namespace std;

namespace logtail {

class LoongSuiteForwardServiceUnittest : public testing::Test {
public:
    void TestServiceName();
    void TestUpdateConfig();
    void TestUpdateConfigWithInvalidParams();
    void TestRemoveConfig();
    void TestRemoveNonExistentConfig();
    void TestForwardRequest();
    void TestForwardRequestWithInvalidData();
    void TestForwardRequestWithoutMatchingConfig();
    void TestConfigMatching();
    void TestConfigMatchingEdgeCases();
    void TestIndexManagement();
    void TestRetryTimeController();
    void TestConcurrentAccess();
    void TestProcessorRunnerIntegration();

protected:
    void SetUp() override {
        service = std::unique_ptr<LoongSuiteForwardServiceImpl>(new LoongSuiteForwardServiceImpl());
        // Initialize ProcessorRunner for testing
        ProcessorRunner::GetInstance()->Init();
    }

    void TearDown() override {
        service.reset();
        ProcessorRunner::GetInstance()->Stop();
    }

    static void SetUpTestCase() {
        // Set test flag values
        INT32_FLAG(grpc_server_forward_max_retry_times) = 100;
    }

    static void TearDownTestCase() {}

private:
    std::unique_ptr<LoongSuiteForwardServiceImpl> service;
};

void LoongSuiteForwardServiceUnittest::TestServiceName() {
    APSARA_TEST_EQUAL("LoongSuiteForwardService", service->Name());
}

void LoongSuiteForwardServiceUnittest::TestUpdateConfig() {
    Json::Value config;
    std::string configName = "test_config";

    // Test valid configuration
    config["MatchRule"]["Key"] = "service";
    config["MatchRule"]["Value"] = "test-service";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;

    APSARA_TEST_TRUE(service->Update(configName, config));
    APSARA_TEST_EQUAL("test_config", service->mConfigs["test_config"].configName);
    APSARA_TEST_EQUAL("service", service->mConfigs["test_config"].matchKey);
    APSARA_TEST_EQUAL("test-service", service->mConfigs["test_config"].matchValue);
    APSARA_TEST_EQUAL(1, service->mConfigs["test_config"].queueKey);
    APSARA_TEST_EQUAL(0, service->mConfigs["test_config"].inputIndex);
    APSARA_TEST_EQUAL("test_config", service->mMatchIndex["service"]["test-service"]);

    // Test same match rule
    Json::Value configNoMatch;
    configNoMatch["MatchRule"]["Key"] = "service";
    configNoMatch["MatchRule"]["Value"] = "test-service";
    configNoMatch["QueueKey"] = 2;
    configNoMatch["InputIndex"] = 1;
    std::string configName2 = "test_config_2";
    APSARA_TEST_FALSE(service->Update(configName2, configNoMatch));
}

void LoongSuiteForwardServiceUnittest::TestUpdateConfigWithInvalidParams() {
    Json::Value config;
    std::string configName = "invalid_config";

    // Test missing QueueKey
    config["MatchRule"]["Key"] = "service";
    config["MatchRule"]["Value"] = "test-service";
    config["InputIndex"] = 0;
    APSARA_TEST_FALSE(service->Update(configName, config));

    // Test missing InputIndex
    config.clear();
    config["QueueKey"] = 1;
    config["MatchRule"]["Key"] = "service";
    APSARA_TEST_FALSE(service->Update(configName, config));

    // Test with invalid MatchRule structure
    config.clear();
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    config["MatchRule"]["Key"] = "service";
    // Missing Value in MatchRule should fail
    APSARA_TEST_FALSE(service->Update(configName, config));
}

void LoongSuiteForwardServiceUnittest::TestRemoveConfig() {
    Json::Value config;
    std::string configName = "remove_test_config";

    // First add a configuration
    config["MatchRule"]["Key"] = "service";
    config["MatchRule"]["Value"] = "test-service";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE(service->Update(configName, config));

    // Then remove it
    APSARA_TEST_TRUE(service->Remove(configName));

    // Remove should always return true even for duplicate calls
    APSARA_TEST_TRUE(service->Remove(configName));
}

void LoongSuiteForwardServiceUnittest::TestRemoveNonExistentConfig() {
    std::string nonExistentConfig = "non_existent_config";

    // Should not fail when removing non-existent config
    APSARA_TEST_TRUE(service->Remove(nonExistentConfig));
}

void LoongSuiteForwardServiceUnittest::TestRetryTimeController() {
    RetryTimeController controller;
    std::string configName = "test_config";

    // Test initialization
    controller.InitRetryTimes(configName, 50);
    APSARA_TEST_EQUAL(50, controller.GetRetryTimes(configName));

    // Test up retry times
    controller.UpRetryTimes(configName);
    APSARA_TEST_EQUAL(51, controller.GetRetryTimes(configName));

    // Test down retry times
    controller.DownRetryTimes(configName);
    APSARA_TEST_EQUAL(25, controller.GetRetryTimes(configName)); // 51 >> 1 = 25

    // Test down with minimum value
    controller.DownRetryTimes(configName);
    APSARA_TEST_EQUAL(12, controller.GetRetryTimes(configName)); // 25 >> 1 = 12

    // Test clear retry times
    controller.ClearRetryTimes(configName);
    APSARA_TEST_EQUAL(0, controller.GetRetryTimes(configName));

    // Test get for non-existent config
    std::string nonExistentConfig = "non_existent";
    APSARA_TEST_EQUAL(0, controller.GetRetryTimes(nonExistentConfig));
}

UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestServiceName)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestUpdateConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestUpdateConfigWithInvalidParams)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRemoveConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRemoveNonExistentConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRetryTimeController)

} // namespace logtail

UNIT_TEST_MAIN
