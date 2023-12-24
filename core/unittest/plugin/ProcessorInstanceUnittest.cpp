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

#include "unittest/plugin/PluginMock.h"
#include "plugin/instance/ProcessorInstance.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessorInstanceUnittest : public testing::Test {
public:
    void TestName() const;
    void TestInit() const;
};

void ProcessorInstanceUnittest::TestName() const {
    unique_ptr<ProcessorInstance> processor
        = unique_ptr<ProcessorInstance>(new ProcessorInstance(new ProcessorMock(), "0"));
    APSARA_TEST_EQUAL(ProcessorMock::sName, processor->Name());
}

void ProcessorInstanceUnittest::TestInit() const {
    unique_ptr<ProcessorInstance> processor
        = unique_ptr<ProcessorInstance>(new ProcessorInstance(new ProcessorMock(), "0"));
    Json::Value config;
    PipelineContext context;
    APSARA_TEST_TRUE(processor->Init(config, context));
    APSARA_TEST_EQUAL(&context, &processor->mPlugin->GetContext());
}

UNIT_TEST_CASE(ProcessorInstanceUnittest, TestName)
UNIT_TEST_CASE(ProcessorInstanceUnittest, TestInit)

} // namespace logtail

UNIT_TEST_MAIN
