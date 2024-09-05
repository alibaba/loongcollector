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

#include "pipeline/plugin/creator/StaticProcessorCreator.h"
#include "pipeline/plugin/instance/PluginInstance.h"
#include "unittest/plugin/PluginMock.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class StaticProcessorCreatorUnittest : public testing::Test {
public:
    void TestName();
    void TestIsDynamic();
    void TestCreate();
};

void StaticProcessorCreatorUnittest::TestName() {
    StaticProcessorCreator<ProcessorMock> creator;
    APSARA_TEST_STREQ("processor_mock", creator.Name());
}

void StaticProcessorCreatorUnittest::TestIsDynamic() {
    StaticProcessorCreator<ProcessorMock> creator;
    APSARA_TEST_FALSE(creator.IsDynamic());
}

void StaticProcessorCreatorUnittest::TestCreate() {
    StaticProcessorCreator<ProcessorMock> creator;
    unique_ptr<PluginInstance> processorMock = creator.Create({"0", "0", "1"});
    APSARA_TEST_NOT_EQUAL(nullptr, processorMock.get());
    APSARA_TEST_EQUAL_FATAL("0", processorMock->PluginID());
    APSARA_TEST_EQUAL_FATAL("0", processorMock->NodeID());
    APSARA_TEST_EQUAL_FATAL("1", processorMock->ChildNodeID());
}

UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestName)
UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestIsDynamic)
UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestCreate)

} // namespace logtail

UNIT_TEST_MAIN
