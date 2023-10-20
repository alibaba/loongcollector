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
    void TestProcess();
};

APSARA_UNIT_TEST_CASE(PipelineUnittest, TestInit, 0);
APSARA_UNIT_TEST_CASE(PipelineUnittest, TestProcess, 1);


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

void PipelineUnittest::TestProcess() {
    Pipeline pipeline;
    PipelineConfig config;
    config.mConfigName = "project##config_0";
    config.mProjectName = "project";
    config.mCategory = "category";
    config.mRegion = "cn-shanghai";
    config.mLogType = REGEX_LOG;
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mRegs = std::make_shared<std::list<std::string> >();
    config.mRegs->emplace_back("(.*)");
    config.mKeys = std::make_shared<std::list<std::string> >();
    config.mKeys->emplace_back("content");

    APSARA_TEST_TRUE_FATAL(pipeline.Init(config));
    
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "line1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line2",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "tags" : {
            "__tag__:taiye": "123"
        }
    })";
    eventGroup.FromJsonString(inJson);

    std::vector<PipelineEventGroup> outputList;

    pipeline.Process(std::move(eventGroup), outputList);
    APSARA_TEST_EQUAL_FATAL(1, outputList.size());

    APSARA_TEST_EQUAL_FATAL(2, outputList[0].GetEvents().size());
    //APSARA_TEST_EQUAL_FATAL(1, outputList[0].GetTags().size());
}

} // namespace logtail

UNIT_TEST_MAIN