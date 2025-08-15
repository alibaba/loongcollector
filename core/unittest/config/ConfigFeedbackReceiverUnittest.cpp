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

#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ConfigFeedbackableMock : public ConfigFeedbackable {
public:
    void FeedbackContinuousPipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status) override {
        mContinuousPipelineConfigStatusMap[name] = status;
    }

    void FeedbackOnetimePipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status) override {
        mOnetimePipelineConfigStatusMap[name] = status;
    }

    void FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status) override {
        mInstanceConfigStatusMap[name] = status;
    }

    std::unordered_map<std::string, ConfigFeedbackStatus> mContinuousPipelineConfigStatusMap;
    std::unordered_map<std::string, ConfigFeedbackStatus> mOnetimePipelineConfigStatusMap;
    std::unordered_map<std::string, ConfigFeedbackStatus> mInstanceConfigStatusMap;
};

class ConfigFeedbackReceiverUnittest : public testing::Test {
public:
    void TestContinuousPipelineConfig();
    void TestOnetimePipelineConfig();
    void TestInstanceConfig();

private:
    ConfigFeedbackReceiver& mReceiver = ConfigFeedbackReceiver::GetInstance();
};

void ConfigFeedbackReceiverUnittest::TestContinuousPipelineConfig() {
    auto feedback = unique_ptr<ConfigFeedbackableMock>(new ConfigFeedbackableMock());

    mReceiver.RegisterContinuousPipelineConfig("test_config", feedback.get());
    APSARA_TEST_EQUAL(1U, mReceiver.mContinuousPipelineConfigFeedbackableMap.size());
    APSARA_TEST_EQUAL(mReceiver.mContinuousPipelineConfigFeedbackableMap["test_config"], feedback.get());

    mReceiver.FeedbackContinuousPipelineConfigStatus("test_config", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mContinuousPipelineConfigStatusMap.size());
    APSARA_TEST_EQUAL(feedback->mContinuousPipelineConfigStatusMap["test_config"], ConfigFeedbackStatus::APPLIED);

    mReceiver.FeedbackContinuousPipelineConfigStatus("test_config_1", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mContinuousPipelineConfigStatusMap.size());

    mReceiver.UnregisterContinuousPipelineConfig("test_config");
    APSARA_TEST_EQUAL(0U, mReceiver.mContinuousPipelineConfigFeedbackableMap.size());
}

void ConfigFeedbackReceiverUnittest::TestOnetimePipelineConfig() {
    auto feedback = unique_ptr<ConfigFeedbackableMock>(new ConfigFeedbackableMock());

    mReceiver.RegisterOnetimePipelineConfig("test_config", feedback.get());
    APSARA_TEST_EQUAL(1U, mReceiver.mOnetimePipelineConfigFeedbackableMap.size());
    APSARA_TEST_EQUAL(mReceiver.mOnetimePipelineConfigFeedbackableMap["test_config"], feedback.get());

    mReceiver.FeedbackOnetimePipelineConfigStatus("test_config", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mOnetimePipelineConfigStatusMap.size());
    APSARA_TEST_EQUAL(feedback->mOnetimePipelineConfigStatusMap["test_config"], ConfigFeedbackStatus::APPLIED);

    mReceiver.FeedbackOnetimePipelineConfigStatus("test_config_1", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mOnetimePipelineConfigStatusMap.size());

    mReceiver.UnregisterOnetimePipelineConfig("test_config");
    APSARA_TEST_EQUAL(0U, mReceiver.mOnetimePipelineConfigFeedbackableMap.size());
}

void ConfigFeedbackReceiverUnittest::TestInstanceConfig() {
    auto feedback = unique_ptr<ConfigFeedbackableMock>(new ConfigFeedbackableMock());

    mReceiver.RegisterInstanceConfig("test_config", feedback.get());
    APSARA_TEST_EQUAL(1U, mReceiver.mInstanceConfigFeedbackableMap.size());
    APSARA_TEST_EQUAL(mReceiver.mInstanceConfigFeedbackableMap["test_config"], feedback.get());

    mReceiver.FeedbackInstanceConfigStatus("test_config", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mInstanceConfigStatusMap.size());
    APSARA_TEST_EQUAL(feedback->mInstanceConfigStatusMap["test_config"], ConfigFeedbackStatus::APPLIED);

    mReceiver.FeedbackInstanceConfigStatus("test_config_1", ConfigFeedbackStatus::APPLIED);
    APSARA_TEST_EQUAL(1U, feedback->mInstanceConfigStatusMap.size());

    mReceiver.UnregisterInstanceConfig("test_config");
    APSARA_TEST_EQUAL(0U, mReceiver.mInstanceConfigFeedbackableMap.size());
}

UNIT_TEST_CASE(ConfigFeedbackReceiverUnittest, TestContinuousPipelineConfig)
UNIT_TEST_CASE(ConfigFeedbackReceiverUnittest, TestOnetimePipelineConfig)
UNIT_TEST_CASE(ConfigFeedbackReceiverUnittest, TestInstanceConfig)

} // namespace logtail

UNIT_TEST_MAIN
