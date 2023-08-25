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

#include <cstdlib>
#include "unittest/Unittest.h"
#include "plugin/StaticProcessorCreator.h"

namespace logtail {

class ProcessorMock : public Processor {
public:
    static const char* Name() { return "processor_mock"; }
    bool Init(const ComponentConfig&) override { return true; }
    void Process(PipelineEventGroup&) override {}
    bool IsSupportedEvent(const PipelineEventPtr& e) override { return true; }
};

class StaticProcessorCreatorUnittest : public ::testing::Test {
public:
    void TestName();
    void TestIsDynamic();
    void TestCreate();
};

APSARA_UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestName, 0);
APSARA_UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestIsDynamic, 0);
APSARA_UNIT_TEST_CASE(StaticProcessorCreatorUnittest, TestCreate, 0);

void StaticProcessorCreatorUnittest::TestName() {
    StaticProcessorCreator<ProcessorMock> creator;
    APSARA_TEST_STREQ_FATAL("processor_mock", creator.Name());
}

void StaticProcessorCreatorUnittest::TestIsDynamic() {
    StaticProcessorCreator<ProcessorMock> creator;
    APSARA_TEST_FALSE_FATAL(creator.IsDynamic());
}

void StaticProcessorCreatorUnittest::TestCreate() {
    StaticProcessorCreator<ProcessorMock> creator;
    auto processorMock = creator.Create(std::string(ProcessorMock::Name()) + "/0");
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, processorMock.get());
    APSARA_TEST_EQUAL_FATAL(std::string(ProcessorMock::Name()) + "/0", processorMock->Id());
}

} // namespace logtail

UNIT_TEST_MAIN