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
#include "pipeline/Pipeline.h"
#include "plugin/PluginRegistry.h"

namespace logtail {

class PipelineUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }
    void TestInit();
};

APSARA_UNIT_TEST_CASE(PipelineUnittest, TestInit, 0);

void PipelineUnittest::TestInit() {
    Pipeline pipeline;
    PipelineConfig config;
    config.mConfigName = "project##config_0";
    config.mProjectName = "project";
    config.mCategory = "category";
    config.mRegion = "cn-shanghai";
    config.mLogType = REGEX_LOG;
    APSARA_TEST_TRUE_FATAL(pipeline.Init(config));
    APSARA_TEST_EQUAL_FATAL(config.mConfigName, pipeline.Name());
    PipelineContext& context = pipeline.GetContext();
    APSARA_TEST_EQUAL_FATAL(config.mProjectName, context.GetProjectName());
    APSARA_TEST_EQUAL_FATAL(config.mCategory, context.GetLogstoreName());
    APSARA_TEST_EQUAL_FATAL(config.mRegion, context.GetRegion());
}

} // namespace logtail

UNIT_TEST_MAIN