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

#include "task_pipeline/TaskPipelineManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class TaskPipelineManagerUnittest : public testing::Test {
public:
    void TestPipelineManagement() const;
};

void TaskPipelineManagerUnittest::TestPipelineManagement() const {
    TaskPipelineManager::GetInstance()->mPipelineNameEntityMap["test1"] = make_unique<TaskPipeline>();
    TaskPipelineManager::GetInstance()->mPipelineNameEntityMap["test2"] = make_unique<TaskPipeline>();

    APSARA_TEST_EQUAL(2U, TaskPipelineManager::GetInstance()->GetAllPipelineNames().size());
    APSARA_TEST_NOT_EQUAL(nullptr, TaskPipelineManager::GetInstance()->FindPipelineByName("test1"));
    APSARA_TEST_EQUAL(nullptr, TaskPipelineManager::GetInstance()->FindPipelineByName("test3"));
}

UNIT_TEST_CASE(TaskPipelineManagerUnittest, TestPipelineManagement)

} // namespace logtail

UNIT_TEST_MAIN
