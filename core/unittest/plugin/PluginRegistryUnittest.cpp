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

#include "collection_pipeline/plugin/PluginRegistry.h"
#include "collection_pipeline/plugin/creator/StaticFlusherCreator.h"
#include "collection_pipeline/plugin/creator/StaticInputCreator.h"
#include "collection_pipeline/plugin/creator/StaticProcessorCreator.h"
#include "runner/FlusherRunner.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class PluginRegistryUnittest : public testing::Test {
public:
    void TestCreateInput() const;
    void TestCreateProcessor() const;
    void TestCreateFlusher() const;
    void TestValidPlugin() const;
    void TestSingletonInput() const;

protected:
    void SetUp() override { LoadPluginMock(); }
    void TearDown() override { sRegistry->UnloadPlugins(); }

private:
    static PluginRegistry* sRegistry;
};

PluginRegistry* PluginRegistryUnittest::sRegistry = PluginRegistry::GetInstance();

void PluginRegistryUnittest::TestCreateInput() const {
    {
        auto input = sRegistry->CreateInput(InputMock::sName, false, {"0"});
        APSARA_TEST_NOT_EQUAL_FATAL(nullptr, input);
        APSARA_TEST_EQUAL_FATAL("0", input->PluginID());
    }
    {
        auto input = sRegistry->CreateInput(InputMock::sName, true, {"0"});
        APSARA_TEST_NOT_EQUAL_FATAL(nullptr, input);
        APSARA_TEST_EQUAL_FATAL("0", input->PluginID());
    }
}

void PluginRegistryUnittest::TestCreateProcessor() const {
    auto processor = sRegistry->CreateProcessor(ProcessorMock::sName, {"0"});
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, processor);
    APSARA_TEST_EQUAL_FATAL("0", processor->PluginID());
}

void PluginRegistryUnittest::TestCreateFlusher() const {
    auto flusher = sRegistry->CreateFlusher(FlusherMock::sName, {"0"});
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, flusher);
    APSARA_TEST_EQUAL_FATAL("0", flusher->PluginID());
}

void PluginRegistryUnittest::TestValidPlugin() const {
    APSARA_TEST_TRUE(sRegistry->IsValidNativeInputPlugin("input_mock", false));
    APSARA_TEST_TRUE(sRegistry->IsValidNativeInputPlugin("input_mock", true));
    APSARA_TEST_FALSE(sRegistry->IsValidNativeInputPlugin("input_unknown", false));
    APSARA_TEST_TRUE(sRegistry->IsValidNativeProcessorPlugin("processor_mock"));
    APSARA_TEST_FALSE(sRegistry->IsValidNativeProcessorPlugin("processor_unknown"));
    APSARA_TEST_TRUE(sRegistry->IsValidNativeFlusherPlugin("flusher_mock"));
    APSARA_TEST_FALSE(sRegistry->IsValidNativeFlusherPlugin("flusher_unknown"));
    APSARA_TEST_TRUE(sRegistry->IsValidGoPlugin("service_mock"));
    APSARA_TEST_TRUE(sRegistry->IsValidGoPlugin("service_unknown"));
}

void PluginRegistryUnittest::TestSingletonInput() const {
    APSARA_TEST_FALSE(sRegistry->IsGlobalSingletonInputPlugin("input_mock", false));
    APSARA_TEST_FALSE(sRegistry->IsGlobalSingletonInputPlugin("input_mock", true));
}

UNIT_TEST_CASE(PluginRegistryUnittest, TestCreateInput)
UNIT_TEST_CASE(PluginRegistryUnittest, TestCreateProcessor)
UNIT_TEST_CASE(PluginRegistryUnittest, TestCreateFlusher)
UNIT_TEST_CASE(PluginRegistryUnittest, TestValidPlugin)
UNIT_TEST_CASE(PluginRegistryUnittest, TestSingletonInput)

} // namespace logtail

UNIT_TEST_MAIN
