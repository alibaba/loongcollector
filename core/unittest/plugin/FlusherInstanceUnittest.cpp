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
#include "plugin/instance/FlusherInstance.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FlusherInstanceUnittest : public testing::Test {
public:
    void TestName() const;
    void TestInit() const;
    void TestStart() const;
    void TestStop() const;
};

void FlusherInstanceUnittest::TestName() const {
    unique_ptr<FlusherInstance> flusher = unique_ptr<FlusherInstance>(new FlusherInstance(new FlusherMock(), "0"));
    APSARA_TEST_EQUAL(FlusherMock::sName, flusher->Name());
}

void FlusherInstanceUnittest::TestInit() const {
    unique_ptr<FlusherInstance> flusher = unique_ptr<FlusherInstance>(new FlusherInstance(new FlusherMock(), "0"));
    Json::Value config, opt;
    PipelineContext context;
    APSARA_TEST_TRUE(flusher->Init(config, context, opt));
    APSARA_TEST_EQUAL(&context, &flusher->GetPlugin()->GetContext());
}

void FlusherInstanceUnittest::TestStart() const {
    unique_ptr<FlusherInstance> flusher = unique_ptr<FlusherInstance>(new FlusherInstance(new FlusherMock(), "0"));
    APSARA_TEST_TRUE(flusher->Start());
}

void FlusherInstanceUnittest::TestStop() const {
    unique_ptr<FlusherInstance> flusher = unique_ptr<FlusherInstance>(new FlusherInstance(new FlusherMock(), "0"));
    APSARA_TEST_TRUE(flusher->Stop(true));
}

UNIT_TEST_CASE(FlusherInstanceUnittest, TestName)
UNIT_TEST_CASE(FlusherInstanceUnittest, TestInit)
UNIT_TEST_CASE(FlusherInstanceUnittest, TestStart)
UNIT_TEST_CASE(FlusherInstanceUnittest, TestStop)

} // namespace logtail

UNIT_TEST_MAIN
