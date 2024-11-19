// Copyright 2024 iLogtail Authors
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

#include "task_pipeline/TaskRegistry.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class TaskRegistryUnittest : public testing::Test {
public:
    void TestCreateTask() const;
    void TestValidPlugin() const;

protected:
    void SetUp() override { LoadTaskMock(); }
    void TearDown() override { TaskRegistry::GetInstance()->UnloadPlugins(); }
};

void TaskRegistryUnittest::TestCreateTask() const {
    auto input = TaskRegistry::GetInstance()->CreateTask(TaskMock::sName);
    APSARA_TEST_NOT_EQUAL(nullptr, input);
}

void TaskRegistryUnittest::TestValidPlugin() const {
    APSARA_TEST_TRUE(TaskRegistry::GetInstance()->IsValidPlugin("task_mock"));
    APSARA_TEST_FALSE(TaskRegistry::GetInstance()->IsValidPlugin("task_unknown"));
}

UNIT_TEST_CASE(TaskRegistryUnittest, TestCreateTask)
UNIT_TEST_CASE(TaskRegistryUnittest, TestValidPlugin)

} // namespace logtail

UNIT_TEST_MAIN
