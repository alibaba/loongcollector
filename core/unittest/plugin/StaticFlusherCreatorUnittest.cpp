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

#include "plugin/creator/StaticFlusherCreator.h"
#include "plugin/instance/PluginInstance.h"
#include "unittest/plugin/PluginMock.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class StaticFlusherCreatorUnittest : public testing::Test {
public:
    void TestName();
    void TestIsDynamic();
    void TestCreate();
};

void StaticFlusherCreatorUnittest::TestName() {
    StaticFlusherCreator<FlusherMock> creator;
    APSARA_TEST_STREQ("flusher_mock", creator.Name());
}

void StaticFlusherCreatorUnittest::TestIsDynamic() {
    StaticFlusherCreator<FlusherMock> creator;
    APSARA_TEST_FALSE(creator.IsDynamic());
}

void StaticFlusherCreatorUnittest::TestCreate() {
    StaticFlusherCreator<FlusherMock> creator;
    unique_ptr<PluginInstance> flusherMock = creator.Create({"0", "0", "1"});
    APSARA_TEST_NOT_EQUAL(nullptr, flusherMock.get());
    APSARA_TEST_EQUAL_FATAL("0", flusherMock->PluginID());
    APSARA_TEST_EQUAL_FATAL("0", flusherMock->NodeID());
    APSARA_TEST_EQUAL_FATAL("1", flusherMock->ChildNodeID());
}

UNIT_TEST_CASE(StaticFlusherCreatorUnittest, TestName)
UNIT_TEST_CASE(StaticFlusherCreatorUnittest, TestIsDynamic)
UNIT_TEST_CASE(StaticFlusherCreatorUnittest, TestCreate)

} // namespace logtail

UNIT_TEST_MAIN
