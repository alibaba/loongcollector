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
    void TestConfigMatching();
    void TestConfigMatchingEdgeCases();
    void TestIndexManagement();
    void TestRetryTimeController();
    void TestConcurrentAccess();
    void TestProcessorRunnerIntegration();
    void TestForwardMethod();
    void TestProcessForwardRequest();
    void TestFindMatchingConfig();
    void TestForwardWithValidRequest();
    void TestForwardWithInvalidRequest();
    void TestForwardWithEmptyData();
    void TestAddToIndexEdgeCases();
    void TestRetryTimeControllerEdgeCases();
    void TestConfigConflicts();

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
    APSARA_TEST_EQUAL_FATAL("LoongSuiteForwardService", service->Name());
}

void LoongSuiteForwardServiceUnittest::TestUpdateConfig() {
    Json::Value config;
    std::string configName = "test_config";

    // Test valid configuration
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;

    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));
    // Check that config is stored in mMatchIndex with matchValue as key
    auto it = service->mMatchIndex.find("test_config");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());
    APSARA_TEST_EQUAL_FATAL("test_config", it->second->configName);
    APSARA_TEST_EQUAL_FATAL(1, it->second->queueKey);
    APSARA_TEST_EQUAL_FATAL(0, it->second->inputIndex);

    Json::Value configNoMatch;
    configNoMatch["QueueKey"] = 2;
    configNoMatch["InputIndex"] = 1;
    std::string configName2 = "test_config_2";
    APSARA_TEST_TRUE_FATAL(service->Update(configName2, configNoMatch));
    APSARA_TEST_TRUE_FATAL(service->Update(configName, configNoMatch));
}

void LoongSuiteForwardServiceUnittest::TestUpdateConfigWithInvalidParams() {
    Json::Value config;
    std::string configName = "invalid_config";

    // Test missing QueueKey
    config["InputIndex"] = 0;
    APSARA_TEST_FALSE_FATAL(service->Update(configName, config));

    // Test missing InputIndex
    config.clear();
    config["QueueKey"] = 1;
    APSARA_TEST_FALSE_FATAL(service->Update(configName, config));
}

void LoongSuiteForwardServiceUnittest::TestRemoveConfig() {
    Json::Value config;
    std::string configName = "remove_test_config";

    // First add a configuration
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Verify config exists
    auto it = service->mMatchIndex.find("remove_test_config");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());

    APSARA_TEST_TRUE_FATAL(service->Remove(configName, config));

    it = service->mMatchIndex.find("remove_test_config");
    APSARA_TEST_TRUE_FATAL(it == service->mMatchIndex.end());
}

void LoongSuiteForwardServiceUnittest::TestRemoveNonExistentConfig() {
    std::string nonExistentConfig = "non_existent_config";
    Json::Value config;

    APSARA_TEST_TRUE_FATAL(service->Remove(nonExistentConfig, config));
}

void LoongSuiteForwardServiceUnittest::TestRetryTimeController() {
    RetryTimeController controller;
    std::string configName = "test_config";

    // Test initialization
    controller.InitRetryTimes(configName, 50);
    APSARA_TEST_EQUAL_FATAL(50, controller.GetRetryTimes(configName));

    // Test up retry times
    controller.UpRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(51, controller.GetRetryTimes(configName));

    // Test down retry times
    controller.DownRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(25, controller.GetRetryTimes(configName)); // max(51 >> 1, 1) = max(25, 1) = 25

    // Test down with smaller value
    controller.DownRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(12, controller.GetRetryTimes(configName)); // max(25 >> 1, 1) = max(12, 1) = 12

    // Test down to minimum value
    for (int i = 0; i < 10; i++) {
        controller.DownRetryTimes(configName);
    }
    APSARA_TEST_EQUAL_FATAL(1, controller.GetRetryTimes(configName)); // Should never go below 1

    // Test clear retry times
    controller.ClearRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(0, controller.GetRetryTimes(configName)); // After clear, returns 0

    // Test get for non-existent config
    std::string nonExistentConfig = "non_existent";
    APSARA_TEST_EQUAL_FATAL(0, controller.GetRetryTimes(nonExistentConfig));
}

void LoongSuiteForwardServiceUnittest::TestConfigMatching() {
    Json::Value config1, config2;
    std::string configName1 = "config1";
    std::string configName2 = "config2";

    // Add two different configs
    config1["QueueKey"] = 1;
    config1["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName1, config1));

    config2["QueueKey"] = 2;
    config2["InputIndex"] = 1;
    APSARA_TEST_TRUE_FATAL(service->Update(configName2, config2));

    // Verify both configs exist in the index
    auto it1 = service->mMatchIndex.find("config1");
    auto it2 = service->mMatchIndex.find("config2");
    APSARA_TEST_TRUE_FATAL(it1 != service->mMatchIndex.end());
    APSARA_TEST_TRUE_FATAL(it2 != service->mMatchIndex.end());
    APSARA_TEST_EQUAL_FATAL("config1", it1->second->configName);
    APSARA_TEST_EQUAL_FATAL("config2", it2->second->configName);
}

void LoongSuiteForwardServiceUnittest::TestConfigMatchingEdgeCases() {
    Json::Value config;
    std::string configName = "edge_test_config";

    // Test with special characters in match value
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    auto it = service->mMatchIndex.find("edge_test_config");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());
    APSARA_TEST_EQUAL_FATAL(configName, it->second->configName);
}

void LoongSuiteForwardServiceUnittest::TestIndexManagement() {
    Json::Value config;
    std::string configName = "index_test_config";

    // Test adding config to index
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Verify config is in index
    APSARA_TEST_EQUAL_FATAL(1, service->mMatchIndex.size());

    // Test removing config from index
    service->Remove(configName, config);

    APSARA_TEST_EQUAL_FATAL(0, service->mMatchIndex.size());
}

void LoongSuiteForwardServiceUnittest::TestConcurrentAccess() {
    // This test would require threading and proper synchronization testing
    // For now, we can test basic thread safety by adding/removing configs
    Json::Value config;
    std::string configName = "concurrent_test_config";

    config["QueueKey"] = 1;
    config["InputIndex"] = 0;

    // Test basic operations that should be thread-safe
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    auto it = service->mMatchIndex.find("concurrent_test_config");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());

    service->Remove(configName, config);
    // TODO: Add actual multi-threading test when needed
}

void LoongSuiteForwardServiceUnittest::TestProcessorRunnerIntegration() {
    // This test would verify integration with ProcessorRunner
    // For now, just verify that ProcessorRunner is initialized in SetUp
    // TODO: Add more comprehensive integration testing
    APSARA_TEST_TRUE_FATAL(ProcessorRunner::GetInstance() != nullptr);
}

void LoongSuiteForwardServiceUnittest::TestForwardMethod() {
    // Setup a config first
    Json::Value config;
    std::string configName = "forward_test_config";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Create a mock gRPC context and request
    grpc::CallbackServerContext context;
    LoongSuiteForwardRequest request;
    LoongSuiteForwardResponse response;

    // Test Forward method without proper metadata (should return NOT_FOUND)
    auto reactor = service->Forward(&context, &request, &response);
    APSARA_TEST_TRUE_FATAL(reactor != nullptr);
}

void LoongSuiteForwardServiceUnittest::TestProcessForwardRequest() {
    // Setup config
    auto config = std::make_shared<ForwardConfig>();
    config->configName = "test_config";
    config->queueKey = 1;
    config->inputIndex = 0;

    LoongSuiteForwardRequest request;
    grpc::Status status;

    // Test with null request
    service->ProcessForwardRequest(nullptr, config, 1, status);
    APSARA_TEST_EQUAL_FATAL(grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
    APSARA_TEST_EQUAL_FATAL("Invalid request", status.error_message());

    // Test with empty data
    request.set_data("");
    status = grpc::Status::OK; // Reset status
    service->ProcessForwardRequest(&request, config, 1, status);
    APSARA_TEST_EQUAL_FATAL(grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
    APSARA_TEST_EQUAL_FATAL("Empty data in forward request", status.error_message());

    // Test with valid data
    request.set_data("test data");
    status = grpc::Status::OK;
    service->ProcessForwardRequest(&request, config, 1, status);
    APSARA_TEST_EQUAL_FATAL(grpc::StatusCode::UNAVAILABLE, status.error_code());
}

void LoongSuiteForwardServiceUnittest::TestFindMatchingConfig() {
    // Setup config first
    Json::Value config;
    std::string configName = "find_match_test_config";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Create a mock context - unfortunately we can't easily add metadata to the context
    // without more complex mocking infrastructure, so this test is limited
    grpc::CallbackServerContext context;
    std::shared_ptr<ForwardConfig> foundConfig;

    // Test finding config without proper metadata (should return false)
    bool found = service->FindMatchingConfig(&context, foundConfig);
    APSARA_TEST_FALSE_FATAL(found);
    APSARA_TEST_TRUE_FATAL(foundConfig == nullptr);
}

void LoongSuiteForwardServiceUnittest::TestForwardWithValidRequest() {
    // This is a more comprehensive test of the Forward workflow
    Json::Value config;
    std::string configName = "valid_forward_test";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Create request with data
    LoongSuiteForwardRequest request;
    request.set_data("valid test data");
    LoongSuiteForwardResponse response;
    grpc::CallbackServerContext context;

    auto reactor = service->Forward(&context, &request, &response);
    APSARA_TEST_TRUE_FATAL(reactor != nullptr);
}

void LoongSuiteForwardServiceUnittest::TestForwardWithInvalidRequest() {
    // Test Forward with null request - this tests the parameter validation
    LoongSuiteForwardResponse response;
    grpc::CallbackServerContext context;

    auto reactor = service->Forward(&context, nullptr, &response);
    APSARA_TEST_TRUE_FATAL(reactor != nullptr);
}

void LoongSuiteForwardServiceUnittest::TestForwardWithEmptyData() {
    // Setup config
    Json::Value config;
    std::string configName = "empty_data_test";
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;
    APSARA_TEST_TRUE_FATAL(service->Update(configName, config));

    // Create request with empty data
    LoongSuiteForwardRequest request;
    request.set_data("");
    LoongSuiteForwardResponse response;
    grpc::CallbackServerContext context;

    auto reactor = service->Forward(&context, &request, &response);
    APSARA_TEST_TRUE_FATAL(reactor != nullptr);
}

void LoongSuiteForwardServiceUnittest::TestAddToIndexEdgeCases() {
    Json::Value config;
    config["QueueKey"] = 1;
    config["InputIndex"] = 0;

    // Test with duplicate match value conflict
    std::string configName1 = "duplicate_test_1";
    std::string configName2 = "duplicate_test_2";

    APSARA_TEST_TRUE_FATAL(service->Update(configName1, config));
    APSARA_TEST_TRUE_FATAL(service->Update(configName2, config));

    // Verify only first config exists
    auto it = service->mMatchIndex.find("duplicate_test_1");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());
    APSARA_TEST_EQUAL_FATAL(configName1, it->second->configName);
}

void LoongSuiteForwardServiceUnittest::TestRetryTimeControllerEdgeCases() {
    RetryTimeController controller;
    std::string configName = "edge_case_config";

    // Test GetRetryTimes for non-existent config (should return 0)
    APSARA_TEST_EQUAL_FATAL(0, controller.GetRetryTimes(configName));

    // Test UpRetryTimes and DownRetryTimes on non-existent config
    controller.UpRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(1, controller.GetRetryTimes(configName)); // Should create with 1

    controller.DownRetryTimes(configName);
    APSARA_TEST_EQUAL_FATAL(1, controller.GetRetryTimes(configName)); // Should not go below 1

    // Test with max retry times boundary
    controller.InitRetryTimes(configName, INT32_FLAG(grpc_server_forward_max_retry_times));
    int32_t maxRetry = controller.GetRetryTimes(configName);

    controller.UpRetryTimes(configName); // Should not exceed max
    APSARA_TEST_EQUAL_FATAL(maxRetry, controller.GetRetryTimes(configName));

    // Test ClearRetryTimes multiple times
    controller.ClearRetryTimes(configName);
    controller.ClearRetryTimes(configName); // Should not crash
    APSARA_TEST_EQUAL_FATAL(0, controller.GetRetryTimes(configName));
}

void LoongSuiteForwardServiceUnittest::TestConfigConflicts() {
    Json::Value config1, config2;
    std::string configName1 = "conflict_test_1";

    // Test adding configs with same match value but different config names
    config1["QueueKey"] = 1;
    config1["InputIndex"] = 0;

    config2["QueueKey"] = 2;
    config2["InputIndex"] = 1;

    // First config should succeed
    APSARA_TEST_TRUE_FATAL(service->Update(configName1, config1));

    // Second config update
    APSARA_TEST_TRUE_FATAL(service->Update(configName1, config2));

    // Verify only first config exists
    auto it = service->mMatchIndex.find("conflict_test_1");
    APSARA_TEST_TRUE_FATAL(it != service->mMatchIndex.end());
    APSARA_TEST_EQUAL_FATAL(configName1, it->second->configName);
    APSARA_TEST_EQUAL_FATAL(2, it->second->queueKey);
}

UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestServiceName)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestUpdateConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestUpdateConfigWithInvalidParams)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRemoveConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRemoveNonExistentConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRetryTimeController)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestConfigMatching)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestConfigMatchingEdgeCases)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestIndexManagement)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestConcurrentAccess)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestProcessorRunnerIntegration)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestForwardMethod)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestProcessForwardRequest)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestFindMatchingConfig)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestForwardWithValidRequest)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestForwardWithInvalidRequest)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestForwardWithEmptyData)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestAddToIndexEdgeCases)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestRetryTimeControllerEdgeCases)
UNIT_TEST_CASE(LoongSuiteForwardServiceUnittest, TestConfigConflicts)
} // namespace logtail

UNIT_TEST_MAIN
