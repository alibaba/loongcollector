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

#include <json/value.h>

#include <future>
#include <string>

#include "common/Flags.h"
#include "forward/GrpcInputManager.h"
#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "unittest/Unittest.h"
#include "unittest/forward/MockServiceImpl.h"

using namespace std;
using namespace logtail;

DECLARE_FLAG_INT32(grpc_server_stop_timeout);

namespace logtail {

class GrpcInputManagerUnittest : public ::testing::Test {
public:
    void TestUpdateListenInputNewAddressSuccess() const;
    void TestUpdateListenInputExistingAddressSameTypeUpdateConfig() const;
    void TestUpdateListenInputExistingAddressDifferentTypeError() const;
    void TestRemoveListenInputNormalRemove() const;
    void TestRemoveListenInputAddressNotFoundError() const;
    void TestRemoveListenInputAddressWithMultipleConfigs() const;

    void TestHasRegisteredPluginsEmpty() const;
    void TestHasRegisteredPluginsNotEmpty() const;

    void TestShutdownGrpcServer() const;

protected:
    void SetUp() override { GrpcInputManager::GetInstance()->Stop(); }
};

void GrpcInputManagerUnittest::TestUpdateListenInputNewAddressSuccess() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50051";
    bool ret = runner->AddListenInput<MockServiceImpl>("configA", address, config);
    APSARA_TEST_TRUE_FATAL(ret);
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.size() == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.find(address) != runner->mListenAddressToInputMap.end());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService->Name() == "MockServiceImpl");
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mReferenceCount == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mServer);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mInFlightCnt->load() == 0);
    runner->Stop();
}

void GrpcInputManagerUnittest::TestUpdateListenInputExistingAddressSameTypeUpdateConfig() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50052";
    runner->AddListenInput<MockServiceImpl>("configA", address, config);
    auto* serverPointer = runner->mListenAddressToInputMap[address].mServer.get();
    config["test"] = 2;
    bool ret = runner->AddListenInput<MockServiceImpl>("configA", address, config);
    APSARA_TEST_TRUE_FATAL(ret);
    APSARA_TEST_EQUAL_FATAL(runner->mListenAddressToInputMap.size(), 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.find(address) != runner->mListenAddressToInputMap.end());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService->Name() == "MockServiceImpl");
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mReferenceCount == 2);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mServer);
    APSARA_TEST_EQUAL_FATAL(runner->mListenAddressToInputMap[address].mServer.get(), serverPointer);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mInFlightCnt->load() == 0);
    runner->Stop();
}

void GrpcInputManagerUnittest::TestUpdateListenInputExistingAddressDifferentTypeError() const {
    MockServiceImpl mockService;
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["MatchRule"]["Key"] = "key";
    config["MatchRule"]["Value"] = "value";
    config["QueueKey"] = 1;
    config["InputIndex"] = 1;
    config["Protocol"] = "LoongSuite";
    const std::string address = "0.0.0.0:50053";
    bool ret = runner->AddListenInput<LoongSuiteForwardServiceImpl>("configA", address, config);
    APSARA_TEST_TRUE_FATAL(ret);
    ret = runner->AddListenInput<MockServiceImpl>("configB", address, config);
    APSARA_TEST_FALSE_FATAL(ret);
    APSARA_TEST_EQUAL_FATAL(runner->mListenAddressToInputMap.size(), 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.find(address) != runner->mListenAddressToInputMap.end());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService);
    APSARA_TEST_EQUAL_FATAL(runner->mListenAddressToInputMap[address].mService->Name(),
                            LoongSuiteForwardServiceImpl::sName);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mReferenceCount == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mServer);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mInFlightCnt->load() == 0);
    runner->Stop();
}

void GrpcInputManagerUnittest::TestRemoveListenInputNormalRemove() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50054";
    runner->AddListenInput<MockServiceImpl>("configA", address, config);
    bool ret = runner->RemoveListenInput("configA", address, config);
    APSARA_TEST_TRUE_FATAL(ret);
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void GrpcInputManagerUnittest::TestRemoveListenInputAddressNotFoundError() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50055";
    bool ret = runner->RemoveListenInput("configA", address, config);
    APSARA_TEST_FALSE_FATAL(ret);
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void GrpcInputManagerUnittest::TestRemoveListenInputAddressWithMultipleConfigs() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50056";
    runner->AddListenInput<MockServiceImpl>("configA", address, config);
    runner->AddListenInput<MockServiceImpl>("configB", address, config);
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.size() == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.find(address) != runner->mListenAddressToInputMap.end());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService->Name() == "MockServiceImpl");
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mReferenceCount == 2);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mServer);

    bool ret = runner->RemoveListenInput("configA", address, config);
    APSARA_TEST_TRUE_FATAL(ret);
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.size() == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap.find(address) != runner->mListenAddressToInputMap.end());
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mService->Name() == "MockServiceImpl");
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mReferenceCount == 1);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mServer);
    APSARA_TEST_TRUE_FATAL(runner->mListenAddressToInputMap[address].mInFlightCnt->load() == 0);
    runner->Stop();
}

void GrpcInputManagerUnittest::TestHasRegisteredPluginsEmpty() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    APSARA_TEST_FALSE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void GrpcInputManagerUnittest::TestHasRegisteredPluginsNotEmpty() const {
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    runner->AddListenInput<MockServiceImpl>("configA", "0.0.0.0:50056", config);
    APSARA_TEST_TRUE_FATAL(runner->HasRegisteredPlugins());
    runner->Stop();
}

void GrpcInputManagerUnittest::TestShutdownGrpcServer() const {
    INT32_FLAG(grpc_server_stop_timeout) = 1;
    auto* runner = GrpcInputManager::GetInstance();
    runner->Init();
    Json::Value config;
    config["test"] = 1;
    const std::string address = "0.0.0.0:50057";
    runner->AddListenInput<MockServiceImpl>("configA", address, config);
    auto& inFlightCnt = runner->mListenAddressToInputMap[address].mInFlightCnt;
    auto future = std::async(std::launch::async, [&inFlightCnt]() {
        for (int j = 0; j < 10; j++) {
            inFlightCnt->fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    future.wait();
    APSARA_TEST_FALSE_FATAL(
        runner->ShutdownGrpcServer(runner->mListenAddressToInputMap[address].mServer.get(), inFlightCnt));
    inFlightCnt = 0;
    APSARA_TEST_TRUE_FATAL(
        runner->ShutdownGrpcServer(runner->mListenAddressToInputMap[address].mServer.get(), inFlightCnt));
    runner->Stop();
}

UNIT_TEST_CASE(GrpcInputManagerUnittest, TestUpdateListenInputNewAddressSuccess);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestUpdateListenInputExistingAddressSameTypeUpdateConfig);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestUpdateListenInputExistingAddressDifferentTypeError);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestRemoveListenInputNormalRemove);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestRemoveListenInputAddressNotFoundError);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestRemoveListenInputAddressWithMultipleConfigs);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestHasRegisteredPluginsEmpty);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestHasRegisteredPluginsNotEmpty);
UNIT_TEST_CASE(GrpcInputManagerUnittest, TestShutdownGrpcServer);

} // namespace logtail

UNIT_TEST_MAIN
