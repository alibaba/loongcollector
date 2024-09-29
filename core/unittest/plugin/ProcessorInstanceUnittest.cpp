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

#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class ProcessorInstanceUnittest : public testing::Test {
public:
    void TestName() const;
    void TestInit() const;
    void TestProcess() const;
};

void ProcessorInstanceUnittest::TestName() const {
    unique_ptr<ProcessorInstance> processor = make_unique<ProcessorInstance>(new ProcessorMock(), PluginInstance::PluginMeta("0"));
    APSARA_TEST_EQUAL(ProcessorMock::sName, processor->Name());
}

void ProcessorInstanceUnittest::TestInit() const {
    unique_ptr<ProcessorInstance> processor = make_unique<ProcessorInstance>(new ProcessorMock(), PluginInstance::PluginMeta("0"));
    Json::Value config;
    PipelineContext context;
    APSARA_TEST_TRUE(processor->Init(config, context));
    APSARA_TEST_EQUAL(&context, &processor->mPlugin->GetContext());
}

void ProcessorInstanceUnittest::TestProcess() const {
    unique_ptr<ProcessorInstance> processor = make_unique<ProcessorInstance>(new ProcessorMock(), PluginInstance::PluginMeta("0"));
    Json::Value config;
    PipelineContext context;
    processor->Init(config, context);

    vector<PipelineEventGroup> groups;

    // empty group
    processor->Process(groups);
    APSARA_TEST_EQUAL(0U, static_cast<ProcessorMock*>(processor->mPlugin.get())->mCnt);

    // non-empty group
    groups.emplace_back(make_shared<SourceBuffer>());
    processor->Process(groups);
    APSARA_TEST_EQUAL(1U, static_cast<ProcessorMock*>(processor->mPlugin.get())->mCnt);
}

UNIT_TEST_CASE(ProcessorInstanceUnittest, TestName)
UNIT_TEST_CASE(ProcessorInstanceUnittest, TestInit)
UNIT_TEST_CASE(ProcessorInstanceUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN
