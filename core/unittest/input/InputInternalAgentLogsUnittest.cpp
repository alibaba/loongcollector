// Copyright 2024 iLogtail Authors
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

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
#include "common/RuntimeUtil.h"
#include "config/CollectionConfig.h"
#include "file_server/StaticFileServer.h"
#include "monitor/Monitor.h"
#include "plugin/input/InputInternalAgentLogs.h"
#include "plugin/input/InputStaticFile.h"
#include "plugin/processor/ProcessorParseApsaraNative.h"
#include "plugin/processor/ProcessorParseRegexNative.h"
#include "plugin/processor/ProcessorParseTimestampNative.h"
#include "plugin/processor/ProcessorTimestampFilterNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

using namespace std;
namespace fs = std::filesystem;

namespace logtail {

class InputInternalAgentLogsUnittest : public testing::Test {
public:
    void TestInferArtifact();
    void TestSkipWhenIPNotMatch();
    void TestExpandGroupsAndProcessors();
    void TestCompressedLogAlarm();

protected:
    static void SetUpTestCase() {
        LoongCollectorMonitor::GetInstance()->Init();
        PluginRegistry::GetInstance()->LoadPlugins();
    }

    static void TearDownTestCase() {
        PluginRegistry::GetInstance()->UnloadPlugins();
        LoongCollectorMonitor::GetInstance()->Stop();
    }

    void SetUp() override {
        LoongCollectorMonitor::mIpAddr = "10.0.1.11";
        AppConfig::GetInstance()->SetLoongcollectorConfDir(GetProcessExecutionDir());
        p.mName = "onetime-al-test";
        ctx.SetConfigName("onetime-al-test");
        p.mPluginID.store(0);
        ctx.SetPipeline(p);
        writeFile(Join(GetAgentLogDir(), "loongcollector.LOG"), "[2026-08-24 13:52:01.123456]\t[info]\tstarted\n");
        writeFile(Join(GetAgentLogDir(), "loongcollector.LOG.1"), "[2026-08-24 12:00:00.000000]\t[info]\trotated\n");
        writeFile(Join(GetAgentLogDir(), "loongcollector.LOG.2.gz"), "compressed");
        writeFile(Join(GetAgentRunDir(), "app_info.json"), "{\"ip\":\"10.0.1.11\"}\n");
        writeFile(GetCheckPointFileName(), "{\"version\":1}\n");
        fs::create_directories(fs::path(GetProcessExecutionDir()) / "continuous_pipeline_config" / "local");
        writeFile(Join(Join(GetProcessExecutionDir(), "continuous_pipeline_config/local"), "demo.json"), "{}\n");
    }

    void TearDown() override {
        StaticFileServer::GetInstance()->Clear();
        removeFile(Join(GetAgentLogDir(), "loongcollector.LOG"));
        removeFile(Join(GetAgentLogDir(), "loongcollector.LOG.1"));
        removeFile(Join(GetAgentLogDir(), "loongcollector.LOG.2.gz"));
        removeFile(Join(GetAgentRunDir(), "app_info.json"));
        removeFile(GetCheckPointFileName());
        error_code ec;
        fs::remove_all(fs::path(GetProcessExecutionDir()) / "continuous_pipeline_config", ec);
    }

private:
    static string Join(const string& dir, const string& name) { return (fs::path(dir) / name).string(); }

    static void writeFile(const string& path, const string& content) {
        ofstream fout(path, ios::trunc);
        fout << content;
    }

    static void removeFile(const string& path) {
        error_code ec;
        fs::remove(path, ec);
    }

    unique_ptr<CollectionPipeline> initPipeline(const string& configStr) {
        string errorMsg;
        unique_ptr<Json::Value> detail = make_unique<Json::Value>();
        if (!ParseJsonTable(configStr, *detail, errorMsg)) {
            return nullptr;
        }
        CollectionConfig config("onetime-al-test", std::move(detail), "onetime-al-test.json");
        if (!config.Parse()) {
            return nullptr;
        }
        auto pipeline = make_unique<CollectionPipeline>();
        if (!pipeline->Init(std::move(config))) {
            return nullptr;
        }
        return pipeline;
    }

    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputInternalAgentLogsUnittest::TestInferArtifact() {
    APSARA_TEST_EQUAL("cpp_log", InputInternalAgentLogs::InferArtifact("/opt/loongcollector/log/loongcollector.LOG"));
    APSARA_TEST_EQUAL("cpp_log", InputInternalAgentLogs::InferArtifact("/opt/loongcollector/log/loongcollector.LOG.1"));
    APSARA_TEST_EQUAL("cpp_log", InputInternalAgentLogs::InferArtifact("/home/ilogtail.LOG.2"));
    APSARA_TEST_EQUAL("go_log", InputInternalAgentLogs::InferArtifact("/opt/loongcollector/log/go_plugin.LOG"));
    APSARA_TEST_EQUAL("go_log", InputInternalAgentLogs::InferArtifact("/home/logtail_plugin.LOG.1"));
    APSARA_TEST_EQUAL("app_info", InputInternalAgentLogs::InferArtifact("/opt/loongcollector/run/app_info.json"));
    APSARA_TEST_EQUAL("file_checkpoint",
                      InputInternalAgentLogs::InferArtifact("/opt/loongcollector/data/file_check_point"));
    APSARA_TEST_EQUAL("file_checkpoint", InputInternalAgentLogs::InferArtifact("/home/checkpoint/logtail_check_point"));
    APSARA_TEST_EQUAL(
        "pipeline_config",
        InputInternalAgentLogs::InferArtifact("/opt/loongcollector/conf/continuous_pipeline_config/local/a.json"));
    APSARA_TEST_EQUAL("agent_file", InputInternalAgentLogs::InferArtifact("/tmp/unknown.bin"));
}

void InputInternalAgentLogsUnittest::TestSkipWhenIPNotMatch() {
    Json::Value configJson, optionalGoPipeline;
    string errorMsg;
    const string configStr = R"(
        {
            "Type": "input_internal_agent_logs_onetime",
            "IPList": ["10.0.1.99"],
            "StartTime": 1756032000,
            "EndTime": 1756035600
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    InputInternalAgentLogs input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputInternalAgentLogs::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input.mSkipCollect);

    vector<unique_ptr<InputInstance>> extras;
    APSARA_TEST_TRUE(input.ExpandAdditionalInputs(1, extras));
    APSARA_TEST_EQUAL(0U, extras.size());
    APSARA_TEST_TRUE(input.Start());
}

void InputInternalAgentLogsUnittest::TestExpandGroupsAndProcessors() {
    const string configStr = R"(
        {
            "global": {
                "ExcutionTimeout": 3600,
                "ForceRerunWhenUpdate": true
            },
            "inputs": [
                {
                    "Type": "input_internal_agent_logs_onetime",
                    "Aliuid": "123456",
                    "IPList": ["10.0.1.11"],
                    "StartTime": 1756032000,
                    "EndTime": 1756035600
                }
            ],
            "flushers": [
                {
                    "Type": "flusher_blackhole"
                }
            ]
        }
    )";
    auto pipeline = initPipeline(configStr);
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, pipeline);
    APSARA_TEST_EQUAL(5U, pipeline->GetInputs().size());
    APSARA_TEST_EQUAL(InputInternalAgentLogs::sName, pipeline->GetInputs()[0]->Name());
    for (size_t i = 1; i < pipeline->GetInputs().size(); ++i) {
        APSARA_TEST_EQUAL(InputStaticFile::sName, pipeline->GetInputs()[i]->Name());
    }

    const auto& runtimeProcessors = pipeline->GetInputs()[1]->GetInnerProcessors();
    APSARA_TEST_TRUE(runtimeProcessors.size() >= 6U);
    APSARA_TEST_EQUAL(ProcessorSplitMultilineLogStringNative::sName, runtimeProcessors[0]->Name());
    APSARA_TEST_EQUAL("processor_agent_log_tag", runtimeProcessors[1]->Name());
    APSARA_TEST_EQUAL(ProcessorParseApsaraNative::sName, runtimeProcessors[2]->Name());
    APSARA_TEST_EQUAL(ProcessorParseRegexNative::sName, runtimeProcessors[3]->Name());
    APSARA_TEST_EQUAL(ProcessorParseTimestampNative::sName, runtimeProcessors[4]->Name());
    APSARA_TEST_EQUAL(ProcessorTimestampFilterNative::sName, runtimeProcessors[5]->Name());

    const auto& wholeProcessors = pipeline->GetInputs()[2]->GetInnerProcessors();
    APSARA_TEST_EQUAL(1U, wholeProcessors.size());
    APSARA_TEST_EQUAL("processor_agent_log_tag", wholeProcessors[0]->Name());
}

void InputInternalAgentLogsUnittest::TestCompressedLogAlarm() {
    Json::Value configJson, optionalGoPipeline;
    string errorMsg;
    const string configStr = R"(
        {
            "Type": "input_internal_agent_logs_onetime",
            "IPList": ["10.0.1.11"]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    InputInternalAgentLogs input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputInternalAgentLogs::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input.mSkipCollect);
    APSARA_TEST_TRUE(input.Start());
}

UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestInferArtifact)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestSkipWhenIPNotMatch)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestExpandGroupsAndProcessors)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestCompressedLogAlarm)

} // namespace logtail

UNIT_TEST_MAIN
