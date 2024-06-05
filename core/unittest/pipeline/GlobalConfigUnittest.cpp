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

#include <memory>
#include <string>

#include <json/json.h>

#include "common/JsonUtil.h"
#include "pipeline/GlobalConfig.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class GlobalConfigUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;

protected:
    void SetUp() override { ctx.SetConfigName("test_config"); }

private:
    PipelineContext ctx;
};

void GlobalConfigUnittest::OnSuccessfulInit() const {
    unique_ptr<GlobalConfig> config;
    Json::Value configJson, extendedParamJson, extendedParams;
    string configStr, extendedParamStr, errorMsg;

    // only mandatory param
    config.reset(new GlobalConfig());
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);
    APSARA_TEST_EQUAL(0U, config->mProcessPriority);
    APSARA_TEST_FALSE(config->mEnableTimestampNanosecond);
    APSARA_TEST_FALSE(config->mUsingOldContentTag);

    // valid optional param
    configStr = R"(
        {
            "TopicType": "custom",
            "TopicFormat": "test_topic",
            "ProcessPriority": 1,
            "EnableTimestampNanosecond": true,
            "UsingOldContentTag": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_TRUE(extendedParams.isNull());
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::CUSTOM, config->mTopicType);
    APSARA_TEST_EQUAL("test_topic", config->mTopicFormat);
    APSARA_TEST_EQUAL(1U, config->mProcessPriority);
    APSARA_TEST_TRUE(config->mEnableTimestampNanosecond);
    APSARA_TEST_TRUE(config->mUsingOldContentTag);

    // invalid optional param
    configStr = R"(
        {
            "TopicType": true,
            "TopicFormat": true,
            "ProcessPriority": "1",
            "EnableTimestampNanosecond": "true",
            "UsingOldContentTag": "true"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_TRUE(extendedParams.isNull());
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);
    APSARA_TEST_EQUAL(0U, config->mProcessPriority);
    APSARA_TEST_FALSE(config->mEnableTimestampNanosecond);
    APSARA_TEST_FALSE(config->mUsingOldContentTag);

    // topicFormat
    configStr = R"(
        {
            "TopicType": "custom"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "machine_group_topic",
            "TopicFormat": "test_topic"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::MACHINE_GROUP_TOPIC, config->mTopicType);
    APSARA_TEST_EQUAL("test_topic", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "machine_group_topic"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    configStr = R"""(
        {
            "TopicType": "filepath",
            "TopicFormat": "/home/(.*)"
        }
    )""";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::FILEPATH, config->mTopicType);
    APSARA_TEST_EQUAL("/home/(.*)", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "filepath"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "filepath",
            "TopicFormat": "\\d+[s"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "default",
            "TopicFormat": "test_topic"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::DEFAULT, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    configStr = R"(
        {
            "TopicType": "unknown",
            "TopicFormat": "test_topic"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(GlobalConfig::TopicType::NONE, config->mTopicType);
    APSARA_TEST_EQUAL("", config->mTopicFormat);

    // ProcessPriority
    configStr = R"(
        {
            "ProcessPriority": 5
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_EQUAL(0U, config->mProcessPriority);

    // extendedParam
    configStr = R"(
        {
            "StructureType": "v2"
        }
    )";
    extendedParamStr = R"(
        {
            "StructureType": "v2"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(extendedParamStr, extendedParamJson, errorMsg));
    config.reset(new GlobalConfig());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, extendedParams));
    APSARA_TEST_TRUE(extendedParamJson == extendedParams);
}

UNIT_TEST_CASE(GlobalConfigUnittest, OnSuccessfulInit)

} // namespace logtail

UNIT_TEST_MAIN
