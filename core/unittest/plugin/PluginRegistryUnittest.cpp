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
#include "plugin/PluginRegistry.h"
#include "processor/ProcessorParseRegexNative.h"

namespace logtail {

class PluginRegistryUnittest : public ::testing::Test {
public:
    void TestLoadStaticPlugins();
    void TestUnloadPlugins();
    void TestCreateProcessor();
};

APSARA_UNIT_TEST_CASE(PluginRegistryUnittest, TestLoadStaticPlugins, 0);
APSARA_UNIT_TEST_CASE(PluginRegistryUnittest, TestCreateProcessor, 0);

void LoadStaticPlugins();
void UnloadPlugins();
std::unique_ptr<ProcessorInstance> CreateProcessor(const std::string& name, const std::string& pluginId);

void PluginRegistryUnittest::TestLoadStaticPlugins() {
    PluginRegistry::GetInstance()->LoadStaticPlugins();
    APSARA_TEST_FALSE_FATAL(PluginRegistry::GetInstance()->mPluginDict.empty());
    PluginRegistry::GetInstance()->UnloadPlugins();
    APSARA_TEST_TRUE_FATAL(PluginRegistry::GetInstance()->mPluginDict.empty());
}

void PluginRegistryUnittest::TestCreateProcessor() {
    PluginRegistry::GetInstance()->LoadStaticPlugins();
    auto processorParseRegexNative = PluginRegistry::GetInstance()->CreateProcessor(
        ProcessorParseRegexNative::sName, "0");
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, processorParseRegexNative.get());
    APSARA_TEST_EQUAL_FATAL("0", processorParseRegexNative->Id());
}

} // namespace logtail

UNIT_TEST_MAIN