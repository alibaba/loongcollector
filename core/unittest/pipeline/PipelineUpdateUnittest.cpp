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

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "common/JsonUtil.h"
#include "config/PipelineConfig.h"
#include "file_server/FileServer.h"
#include "file_server/event_handler/LogInput.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "unittest/Unittest.h"
#include "unittest/config/PipelineManagerMock.h"

using namespace std;

namespace logtail {

class PipelineUpdateUnittest : public testing::Test {
public:
    void TestFileServerStart() const;

protected:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }

    static void TearDownTestCase() {
        PluginRegistry::GetInstance()->UnloadPlugins();
        FileServer::GetInstance()->Stop();
    }

    void SetUp() override {}

    void TearDown() override {}

private:
    Json::Value GeneratePipelineConfigJson(const string& inputConfig,
                                           const string& processorConfig,
                                           const string& flusherConfig) const {
        Json::Value json;
        string errorMsg;
        ParseJsonTable(R"(
            {
                "valid": true,
                "inputs": [)"
                           + inputConfig + R"(],
                "processors": [)"
                           + processorConfig + R"(],
                "flushers": [)"
                           + flusherConfig + R"(]
            })",
                       json,
                       errorMsg);
        return json;
    }
    string nativeInputConfig = R"(
        {
            "Type": "input_file"
        })";
    string nativeProcessorConfig = R"(
        {
            "Type": "processor_parse_regex_native"
        })";
    string nativeFlusherConfig = R"(
        {
            "Type": "flusher_sls"
        })";
    string goInputConfig = R"(
        {
            "Type": "input_docker_stdout"
        })";
    string goProcessorConfig = R"(
        {
            "Type": "processor_regex"
        })";
    string goFlusherConfig = R"(
        {
            "Type": "flusher_stdout"
        })";
};

void PipelineUpdateUnittest::TestFileServerStart() const {
    Json::Value nativePipelineConfigJson
        = GeneratePipelineConfigJson(nativeInputConfig, nativeProcessorConfig, nativeFlusherConfig);
    Json::Value goPipelineConfigJson = GeneratePipelineConfigJson(goInputConfig, goProcessorConfig, goFlusherConfig);
    auto pipelineManager = PipelineManagerMock::GetInstance();
    PipelineConfigDiff diff;
    PipelineConfig nativePipelineConfigObj
        = PipelineConfig("test1", make_unique<Json::Value>(nativePipelineConfigJson));
    nativePipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(nativePipelineConfigObj));
    PipelineConfig goPipelineConfigObj = PipelineConfig("test2", make_unique<Json::Value>(goPipelineConfigJson));
    goPipelineConfigObj.Parse();
    diff.mAdded.push_back(std::move(goPipelineConfigObj));

    pipelineManager->UpdatePipelines(diff);
    APSARA_TEST_EQUAL_FATAL(2U, pipelineManager->GetAllPipelines().size());
    APSARA_TEST_EQUAL_FATAL(false, LogInput::GetInstance()->mInteruptFlag);
}

UNIT_TEST_CASE(PipelineUpdateUnittest, TestFileServerStart)

} // namespace logtail

UNIT_TEST_MAIN
