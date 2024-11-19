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

#include <json/json.h>

#include <memory>
#include <string>

#include "common/JsonUtil.h"
#include "config/TaskConfig.h"
#include "task_pipeline/TaskRegistry.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class TaskConfigUnittest : public testing::Test {
public:
    void HandleValidConfig() const;
    void HandleInvalidCreateTime() const;
    void HandleInvalidTask() const;

protected:
    static void SetUpTestCase() { LoadTaskMock(); }
    static void TearDownTestCase() { TaskRegistry::GetInstance()->UnloadPlugins(); }

private:
    const string configName = "test";
};

void TaskConfigUnittest::HandleValidConfig() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;

    configStr = R"(
        {
            "createTime": 1234567890,
            "task": {
                "Type": "task_mock"
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(configName, config->mName);
    APSARA_TEST_EQUAL(1234567890U, config->mCreateTime);
}

void TaskConfigUnittest::HandleInvalidCreateTime() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;

    configStr = R"(
        {
            "createTime": "1234567890",
            "task": {
                "Type": "task_mock"
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    APSARA_TEST_EQUAL(0U, config->mCreateTime);
}

void TaskConfigUnittest::HandleInvalidTask() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;

    // task is not of type object
    configStr = R"(
        {
            "createTime": "123456789",
            "task": []
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // no Type
    configStr = R"(
        {
            "createTime": "123456789",
            "task": {
                "Name": "task_mock"
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // Type is not of type string
    configStr = R"(
        {
            "createTime": "123456789",
            "task": {
                "Type": true
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());

    // unsupported Task
    configStr = R"(
        {
            "createTime": "123456789",
            "task": {
                "Type": "task_unknown"
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_FALSE(config->Parse());
}

UNIT_TEST_CASE(TaskConfigUnittest, HandleValidConfig)
UNIT_TEST_CASE(TaskConfigUnittest, HandleInvalidCreateTime)
UNIT_TEST_CASE(TaskConfigUnittest, HandleInvalidTask)

} // namespace logtail

UNIT_TEST_MAIN
