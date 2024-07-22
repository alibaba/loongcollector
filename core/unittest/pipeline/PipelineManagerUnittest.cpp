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

#include "pipeline/Pipeline.h"
#include "pipeline/PipelineConfigManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PipelineManagerUnittest : public testing::Test {
public:
    void TestPipelineManagement() const;
};

void PipelineManagerUnittest::TestPipelineManagement() const {
    PipelineManager::GetInstance()->mPipelineNameEntityMap["test1"] = make_shared<Pipeline>();
    PipelineManager::GetInstance()->mPipelineNameEntityMap["test2"] = make_shared<Pipeline>();

    APSARA_TEST_EQUAL(2U, PipelineManager::GetInstance()->GetAllConfigNames().size());
    APSARA_TEST_NOT_EQUAL(nullptr, PipelineManager::GetInstance()->FindConfigByName("test1"));
    APSARA_TEST_EQUAL(nullptr, PipelineManager::GetInstance()->FindConfigByName("test3"));
}

UNIT_TEST_CASE(PipelineManagerUnittest, TestPipelineManagement)

} // namespace logtail

UNIT_TEST_MAIN
