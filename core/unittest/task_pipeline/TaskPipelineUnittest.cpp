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

#include "common/JsonUtil.h"
#include "task_pipeline/TaskPipeline.h"
#include "task_pipeline/TaskRegistry.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class TaskPipelineUnittest : public ::testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnFailedInit() const;
    void OnUpdate() const;

protected:
    static void SetUpTestCase() { LoadTaskMock(); }

    static void TearDownTestCase() { TaskRegistry::GetInstance()->UnloadPlugins(); }

private:
    const string configName = "test_config";
};

void TaskPipelineUnittest::OnSuccessfulInit() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;
    unique_ptr<TaskPipeline> task;

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
    auto configPtr = configJson.get();
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    task.reset(new TaskPipeline());
    APSARA_TEST_TRUE(task->Init(std::move(*config)));
    APSARA_TEST_EQUAL(configName, task->Name());
    APSARA_TEST_EQUAL(configPtr, &task->GetConfig());
    APSARA_TEST_EQUAL(1234567890U, task->mCreateTime);
    APSARA_TEST_NOT_EQUAL(nullptr, task->mPlugin);
    APSARA_TEST_EQUAL(TaskMock::sName, task->mPlugin->Name());
}

void TaskPipelineUnittest::OnFailedInit() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;
    unique_ptr<TaskPipeline> task;

    configStr = R"(
        {
            "createTime": 1234567890,
            "task": {
                "Type": "task_mock",
                "Valid": false
            }
        }
    )";
    configJson.reset(new Json::Value());
    APSARA_TEST_TRUE(ParseJsonTable(configStr, *configJson, errorMsg));
    config.reset(new TaskConfig(configName, std::move(configJson)));
    APSARA_TEST_TRUE(config->Parse());
    task.reset(new TaskPipeline());
    APSARA_TEST_FALSE(task->Init(std::move(*config)));
}

void TaskPipelineUnittest::OnUpdate() const {
    unique_ptr<Json::Value> configJson;
    string configStr, errorMsg;
    unique_ptr<TaskConfig> config;
    unique_ptr<TaskPipeline> task;

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
    task.reset(new TaskPipeline());
    APSARA_TEST_TRUE(task->Init(std::move(*config)));

    auto ptr = static_cast<TaskMock*>(task->mPlugin.get());
    task->Start();
    APSARA_TEST_TRUE(ptr->mIsRunning);
    task->Stop(true);
    APSARA_TEST_FALSE(ptr->mIsRunning);
}

UNIT_TEST_CASE(TaskPipelineUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(TaskPipelineUnittest, OnFailedInit)
UNIT_TEST_CASE(TaskPipelineUnittest, OnUpdate)

} // namespace logtail

UNIT_TEST_MAIN
