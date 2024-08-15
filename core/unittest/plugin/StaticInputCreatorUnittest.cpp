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

#include "plugin/creator/StaticInputCreator.h"
#include "plugin/instance/PluginInstance.h"
#include "unittest/plugin/PluginMock.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class StaticInputCreatorUnittest : public testing::Test {
public:
    void TestName();
    void TestIsDynamic();
    void TestCreate();
};

void StaticInputCreatorUnittest::TestName() {
    StaticInputCreator<InputMock> creator;
    APSARA_TEST_STREQ("input_mock", creator.Name());
}

void StaticInputCreatorUnittest::TestIsDynamic() {
    StaticInputCreator<InputMock> creator;
    APSARA_TEST_FALSE(creator.IsDynamic());
}

void StaticInputCreatorUnittest::TestCreate() {
    StaticInputCreator<InputMock> creator;
    unique_ptr<PluginInstance> inputMock = creator.Create({"0", "0", "1"});
    APSARA_TEST_NOT_EQUAL(nullptr, inputMock.get());
    APSARA_TEST_EQUAL_FATAL("0", inputMock->PluginID());
    APSARA_TEST_EQUAL_FATAL("0", inputMock->NodeID());
    APSARA_TEST_EQUAL_FATAL("1", inputMock->ChildNodeID());
}

UNIT_TEST_CASE(StaticInputCreatorUnittest, TestName)
UNIT_TEST_CASE(StaticInputCreatorUnittest, TestIsDynamic)
UNIT_TEST_CASE(StaticInputCreatorUnittest, TestCreate)

} // namespace logtail

UNIT_TEST_MAIN
