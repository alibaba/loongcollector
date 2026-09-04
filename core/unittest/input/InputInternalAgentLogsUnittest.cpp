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

#include <ctime>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "common/JsonUtil.h"
#include "common/RuntimeUtil.h"
#include "config/CollectionConfig.h"
#include "constants/TagConstants.h"
#include "file_server/StaticFileServer.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/Monitor.h"
#include "plugin/input/InputInternalAgentLogs.h"
#include "plugin/input/InputStaticFile.h"
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
    void TestRuntimeLogHeaderParse();
    void TestGoMockPipelineAndTimeFilter();
    void TestPipelineNameOnConfigFiles();
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
        writeFile(Join(GetAgentLogDir(), GetAgentLogName()),
                  "[2026-08-24 13:52:01.123456]\t[info]\t[1]\tAppConfig.cpp:1\t\tstarted\n");
        writeFile(Join(GetAgentLogDir(), GetAgentLogName() + ".1"),
                  "[2026-08-24 12:00:00.000000]\t[info]\t[1]\tAppConfig.cpp:1\t\trotated\n");
        writeFile(Join(GetAgentLogDir(), GetAgentLogName() + ".2.gz"), "compressed");
        writeFile(Join(GetAgentLogDir(), GetPluginLogName()),
                  "2026-08-24 13:52:01 [info] [metric_mock.go:42] [Start] mock go pipeline started\n"
                  "2026-08-24 13:52:02 [info] [metric_mock.go:88] [Collect] emit 1 points\n");
        writeFile(GetAgentAppInfoFile(), "{\"ip\":\"10.0.1.11\"}\n");
        {
            const fs::path checkpoint(GetCheckPointFileName());
            if (checkpoint.has_parent_path()) {
                fs::create_directories(checkpoint.parent_path());
            }
            writeFile(checkpoint.string(), "{\"version\":1}\n");
        }
        const fs::path pipelineLocal
            = fs::path(AppConfig::GetInstance()->GetLoongcollectorConfDir()) / GetContinuousPipelineConfigDir() / "local";
        fs::create_directories(pipelineLocal);
        writeFile((pipelineLocal / "demo.json").string(), "{}\n");
        writeFile((pipelineLocal / "mock_go_pipeline.json").string(),
                  R"({
  "inputs": [{"Type": "metric_mock", "IntervalMs": 1000}],
  "flushers": [{"Type": "flusher_stdout"}]
})");
    }

    void TearDown() override {
        StaticFileServer::GetInstance()->Clear();
        removeFile(Join(GetAgentLogDir(), GetAgentLogName()));
        removeFile(Join(GetAgentLogDir(), GetAgentLogName() + ".1"));
        removeFile(Join(GetAgentLogDir(), GetAgentLogName() + ".2.gz"));
        removeFile(Join(GetAgentLogDir(), GetPluginLogName()));
        removeFile(GetAgentAppInfoFile());
        removeFile(GetCheckPointFileName());
        error_code ec;
        fs::remove_all(fs::path(AppConfig::GetInstance()->GetLoongcollectorConfDir()) / GetContinuousPipelineConfigDir(),
                       ec);
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
    APSARA_TEST_EQUAL("onetime_pipeline_config",
                      InputInternalAgentLogs::InferArtifact(
                          "/opt/loongcollector/conf/onetime_pipeline_config/local/onetime-al-sls-verify.yaml"));
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
    APSARA_TEST_EQUAL(6U, pipeline->GetInputs().size());
    APSARA_TEST_EQUAL(InputInternalAgentLogs::sName, pipeline->GetInputs()[0]->Name());
    for (size_t i = 1; i < pipeline->GetInputs().size(); ++i) {
        APSARA_TEST_EQUAL(InputStaticFile::sName, pipeline->GetInputs()[i]->Name());
    }

    const auto& cppProcessors = pipeline->GetInputs()[1]->GetInnerProcessors();
    APSARA_TEST_TRUE(cppProcessors.size() >= 6U);
    APSARA_TEST_EQUAL(ProcessorSplitMultilineLogStringNative::sName, cppProcessors[0]->Name());
    APSARA_TEST_EQUAL("processor_agent_log_tag", cppProcessors[1]->Name());
    APSARA_TEST_EQUAL(ProcessorParseRegexNative::sName, cppProcessors[2]->Name());
    APSARA_TEST_EQUAL(ProcessorParseTimestampNative::sName, cppProcessors[3]->Name());
    APSARA_TEST_EQUAL("processor_agent_log_microtime", cppProcessors[4]->Name());
    APSARA_TEST_EQUAL(ProcessorTimestampFilterNative::sName, cppProcessors[5]->Name());

    const auto& goProcessors = pipeline->GetInputs()[2]->GetInnerProcessors();
    APSARA_TEST_TRUE(goProcessors.size() >= 6U);
    APSARA_TEST_EQUAL(ProcessorParseRegexNative::sName, goProcessors[2]->Name());
    APSARA_TEST_EQUAL(ProcessorTimestampFilterNative::sName, goProcessors[5]->Name());

    const auto& wholeProcessors = pipeline->GetInputs()[3]->GetInnerProcessors();
    APSARA_TEST_EQUAL(1U, wholeProcessors.size());
    APSARA_TEST_EQUAL("processor_agent_log_tag", wholeProcessors[0]->Name());
}

void InputInternalAgentLogsUnittest::TestRuntimeLogHeaderParse() {
    const string configStr = R"(
        {
            "global": {
                "ExcutionTimeout": 3600
            },
            "inputs": [
                {
                    "Type": "input_internal_agent_logs_onetime"
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
    APSARA_TEST_TRUE(pipeline->GetInputs().size() >= 3U);
    const auto& cppProcessors = pipeline->GetInputs()[1]->GetInnerProcessors();
    const auto& goProcessors = pipeline->GetInputs()[2]->GetInnerProcessors();
    APSARA_TEST_TRUE(cppProcessors.size() >= 5U);
    APSARA_TEST_TRUE(goProcessors.size() >= 5U);

    auto runThrough = [&](const auto& processors, vector<PipelineEventGroup>& groups) {
        processors[2]->Process(groups);
        processors[3]->Process(groups);
        processors[4]->Process(groups);
    };

    auto runHeaderParsers = [&](const string& raw) -> string {
        vector<PipelineEventGroup> groups;
        groups.emplace_back(make_shared<SourceBuffer>());
        groups[0].AddLogEvent()->SetContent(string("content"), raw);
        runThrough(cppProcessors, groups);
        APSARA_TEST_EQUAL(1U, groups.size());
        APSARA_TEST_EQUAL(1U, groups[0].GetEvents().size());
        if (groups.empty() || groups[0].GetEvents().empty()) {
            return "";
        }
        return groups[0].GetEvents()[0].Cast<LogEvent>().GetContent("content").to_string();
    };

    {
        vector<PipelineEventGroup> groups;
        groups.emplace_back(make_shared<SourceBuffer>());
        groups[0].AddLogEvent()->SetContent(
            string("content"),
            string("[2026-08-26 08:52:00.890493]\t[info]\t[3396]\t/src/core/app_config/AppConfig.cpp:607\t\t"
                   "project:xuanyang-test\tlogstore:agent-log-test"));
        runThrough(cppProcessors, groups);
        APSARA_TEST_EQUAL(1U, groups[0].GetEvents().size());
        const auto& ev = groups[0].GetEvents()[0].Cast<LogEvent>();
        APSARA_TEST_EQUAL("2026-08-26 08:52:00.890493", ev.GetContent("time").to_string());
        APSARA_TEST_EQUAL("info", ev.GetContent("level").to_string());
        APSARA_TEST_EQUAL("3396", ev.GetContent("__THREAD__").to_string());
        APSARA_TEST_EQUAL("/src/core/app_config/AppConfig.cpp:607", ev.GetContent("__FILE__").to_string());
        APSARA_TEST_EQUAL("project:xuanyang-test\tlogstore:agent-log-test", ev.GetContent("content").to_string());
        APSARA_TEST_TRUE(ev.GetContent("function").empty());
        APSARA_TEST_TRUE(ev.GetTimestamp() > 0);
        APSARA_TEST_EQUAL(to_string(static_cast<int64_t>(ev.GetTimestamp()) * 1000000 + 890493),
                          ev.GetContent("microtime").to_string());
    }

    {
        vector<PipelineEventGroup> groups;
        groups.emplace_back(make_shared<SourceBuffer>());
        groups[0].AddLogEvent()->SetContent(string("content"),
                                            string("2026-08-26 08:52:00 [info] [plugin.go:88] [Start] listen :18689"));
        runThrough(goProcessors, groups);
        APSARA_TEST_EQUAL(1U, groups[0].GetEvents().size());
        const auto& ev = groups[0].GetEvents()[0].Cast<LogEvent>();
        APSARA_TEST_EQUAL("2026-08-26 08:52:00", ev.GetContent("time").to_string());
        APSARA_TEST_EQUAL("info", ev.GetContent("level").to_string());
        APSARA_TEST_TRUE(ev.GetContent("__THREAD__").empty());
        APSARA_TEST_EQUAL("plugin.go:88", ev.GetContent("__FILE__").to_string());
        APSARA_TEST_EQUAL("Start", ev.GetContent("function").to_string());
        APSARA_TEST_EQUAL("listen :18689", ev.GetContent("content").to_string());
        APSARA_TEST_TRUE(ev.GetTimestamp() > 0);
        APSARA_TEST_EQUAL(to_string(static_cast<int64_t>(ev.GetTimestamp()) * 1000000),
                          ev.GetContent("microtime").to_string());
    }

    APSARA_TEST_EQUAL("not a structured log line", runHeaderParsers("not a structured log line"));
}

void InputInternalAgentLogsUnittest::TestGoMockPipelineAndTimeFilter() {
    Json::Value optionalGoPipeline, configJson;
    string errorMsg;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"Type":"input_internal_agent_logs_onetime"})", configJson, errorMsg));
    InputInternalAgentLogs input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputInternalAgentLogs::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    const string cppPaths
        = input.buildRuntimeLogsConfig(InputInternalAgentLogs::RuntimeLogKind::Cpp)["FilePaths"].toStyledString();
    const string goPaths
        = input.buildRuntimeLogsConfig(InputInternalAgentLogs::RuntimeLogKind::Go)["FilePaths"].toStyledString();
    APSARA_TEST_TRUE(cppPaths.find(GetAgentLogName()) != string::npos);
    APSARA_TEST_TRUE(cppPaths.find(GetPluginLogName()) == string::npos);
    APSARA_TEST_TRUE(goPaths.find(GetPluginLogName()) != string::npos);
    APSARA_TEST_TRUE(goPaths.find(GetAgentLogName()) == string::npos);

    tm t{};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 7;
    t.tm_mday = 26;
    t.tm_hour = 8;
    t.tm_min = 52;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    const time_t goSec = mktime(&t);
    APSARA_TEST_TRUE(goSec > 0);

    auto makePipeline = [&](int64_t start, int64_t end) {
        return initPipeline(string("{\n") + "  \"global\": {\"ExcutionTimeout\": 3600},\n" + "  \"inputs\": [{\n"
                            + "    \"Type\": \"input_internal_agent_logs_onetime\",\n"
                            + "    \"StartTime\": " + to_string(start) + ",\n" + "    \"EndTime\": " + to_string(end)
                            + "\n" + "  }],\n" + "  \"flushers\": [{\"Type\": \"flusher_blackhole\"}]\n" + "}");
    };

    const string goLine = "2026-08-26 08:52:00 [info] [metric_mock.go:42] [Start] mock go pipeline started";
    const string cppLine = "[2026-08-26 08:52:00.890493]\t[info]\t[8]\tAppConfig.cpp:1\t\tcompanion cpp line";

    {
        auto pipeline = makePipeline(static_cast<int64_t>(goSec) - 10, static_cast<int64_t>(goSec) + 10);
        APSARA_TEST_NOT_EQUAL_FATAL(nullptr, pipeline);
        APSARA_TEST_TRUE(pipeline->GetInputs().size() >= 3U);
        const auto& goProcessors = pipeline->GetInputs()[2]->GetInnerProcessors();
        const auto& cppProcessors = pipeline->GetInputs()[1]->GetInnerProcessors();
        APSARA_TEST_TRUE(goProcessors.size() >= 6U);
        APSARA_TEST_TRUE(cppProcessors.size() >= 6U);

        vector<PipelineEventGroup> goGroups;
        goGroups.emplace_back(make_shared<SourceBuffer>());
        goGroups[0].AddLogEvent()->SetContent(string("content"), goLine);
        for (size_t i = 2; i < 6; ++i) {
            goProcessors[i]->Process(goGroups);
        }
        APSARA_TEST_EQUAL(1U, goGroups[0].GetEvents().size());
        const auto& goEv = goGroups[0].GetEvents()[0].Cast<LogEvent>();
        APSARA_TEST_EQUAL("2026-08-26 08:52:00", goEv.GetContent("time").to_string());
        APSARA_TEST_EQUAL("Start", goEv.GetContent("function").to_string());
        APSARA_TEST_EQUAL("mock go pipeline started", goEv.GetContent("content").to_string());
        APSARA_TEST_EQUAL(goSec, goEv.GetTimestamp());
        APSARA_TEST_EQUAL(to_string(static_cast<int64_t>(goSec) * 1000000), goEv.GetContent("microtime").to_string());

        vector<PipelineEventGroup> cppGroups;
        cppGroups.emplace_back(make_shared<SourceBuffer>());
        cppGroups[0].AddLogEvent()->SetContent(string("content"), cppLine);
        for (size_t i = 2; i < 6; ++i) {
            cppProcessors[i]->Process(cppGroups);
        }
        APSARA_TEST_EQUAL(1U, cppGroups[0].GetEvents().size());
        const auto& cppEv = cppGroups[0].GetEvents()[0].Cast<LogEvent>();
        APSARA_TEST_EQUAL(goSec, cppEv.GetTimestamp());
        APSARA_TEST_EQUAL(to_string(static_cast<int64_t>(goSec) * 1000000 + 890493),
                          cppEv.GetContent("microtime").to_string());
    }

    {
        auto pipeline = makePipeline(static_cast<int64_t>(goSec) + 60, static_cast<int64_t>(goSec) + 120);
        APSARA_TEST_NOT_EQUAL_FATAL(nullptr, pipeline);
        const auto& goProcessors = pipeline->GetInputs()[2]->GetInnerProcessors();
        vector<PipelineEventGroup> groups;
        groups.emplace_back(make_shared<SourceBuffer>());
        groups[0].AddLogEvent()->SetContent(string("content"), goLine);
        for (size_t i = 2; i < 6; ++i) {
            goProcessors[i]->Process(groups);
        }
        APSARA_TEST_EQUAL(0U, groups[0].GetEvents().size());
    }
}

void InputInternalAgentLogsUnittest::TestPipelineNameOnConfigFiles() {
    const string configStr = R"(
        {
            "global": {"ExcutionTimeout": 3600},
            "inputs": [{"Type": "input_internal_agent_logs_onetime"}],
            "flushers": [{"Type": "flusher_blackhole"}]
        }
    )";
    auto pipeline = initPipeline(configStr);
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, pipeline);
    ProcessorInstance* tagProcessor = nullptr;
    for (size_t i = 3; i < pipeline->GetInputs().size(); ++i) {
        auto& procs = pipeline->GetInputs()[i]->GetInnerProcessors();
        if (procs.size() == 1 && procs[0]->Name() == "processor_agent_log_tag") {
            tagProcessor = procs[0].get();
            break;
        }
    }
    APSARA_TEST_NOT_EQUAL_FATAL(nullptr, tagProcessor);

    auto runTag = [&](const string& path, const string& body) {
        vector<PipelineEventGroup> groups;
        groups.emplace_back(make_shared<SourceBuffer>());
        groups[0].SetTag(DEFAULT_LOG_TAG_FILE_PATH, path);
        groups[0].AddLogEvent()->SetContent(string("content"), body);
        tagProcessor->Process(groups);
        return std::move(groups[0]);
    };

    {
        auto group
            = runTag("/src/.vscode/lc-agent-logs-sls/conf/onetime_pipeline_config/local/onetime-al-sls-verify.yaml",
                     "enable: true\n");
        APSARA_TEST_EQUAL("onetime_pipeline_config", group.GetTag("artifact").to_string());
        APSARA_TEST_EQUAL("onetime-al-sls-verify",
                          group.GetEvents()[0].Cast<LogEvent>().GetContent("pipeline_name").to_string());
    }
    {
        auto group = runTag("/opt/loongcollector/conf/continuous_pipeline_config/local/file_simple.yaml", "{}\n");
        APSARA_TEST_EQUAL("pipeline_config", group.GetTag("artifact").to_string());
        APSARA_TEST_EQUAL("file_simple", group.GetEvents()[0].Cast<LogEvent>().GetContent("pipeline_name").to_string());
    }
    {
        auto group = runTag("/opt/loongcollector/run/app_info.json", "{\"ip\":\"10.0.1.11\"}\n");
        APSARA_TEST_EQUAL("app_info", group.GetTag("artifact").to_string());
        APSARA_TEST_TRUE(group.GetEvents()[0].Cast<LogEvent>().GetContent("pipeline_name").empty());
    }
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
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestRuntimeLogHeaderParse)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestGoMockPipelineAndTimeFilter)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestPipelineNameOnConfigFiles)
UNIT_TEST_CASE(InputInternalAgentLogsUnittest, TestCompressedLogAlarm)

} // namespace logtail

UNIT_TEST_MAIN
