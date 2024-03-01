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

#include "common/JsonUtil.h"
#include "config/Config.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "processor/ProcessorParseRegexNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseRegexNativeUnittest : public ::testing::Test {
public:
    void TestInit();
    void OnSuccessfulInit();
    void TestProcessWholeLine();
    void TestProcessRegex();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestProcessEventKeyCountUnmatch();
    void TestProcessRegexRaw();
    void TestProcessRegexContent();

protected:
    void SetUp() override { ctx.SetConfigName("test_config"); }

private:
    PipelineContext ctx;
};

void ProcessorParseRegexNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = "(.*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("content");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
}

void ProcessorParseRegexNativeUnittest::OnSuccessfulInit() {
    // Keys
    std::unique_ptr<ProcessorParseRegexNative> processor;
    Json::Value configJson;
    std::string configStr, errorMsg;

    configStr = R"""(
        {
            "Type": "processor_parse_regex_native",
            "SourceKey": "content",
            "Keys": [
                "k1,k2"
            ],
            "Regex": "(\\d+)\\s+(\\d+)"
        }
    )""";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    processor.reset(new ProcessorParseRegexNative());
    processor->SetContext(ctx);
    processor->SetMetricsRecordRef(ProcessorParseRegexNative::sName, "1");
    APSARA_TEST_TRUE(processor->Init(configJson));
    APSARA_TEST_EQUAL(2, processor->mKeys.size());
    APSARA_TEST_EQUAL("k1", processor->mKeys[0]);
    APSARA_TEST_EQUAL("k2", processor->mKeys[1]);
}

void ProcessorParseRegexNativeUnittest::TestProcessWholeLine() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = "(.*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("content");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "line1\nline2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line3\nline4"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
    // metric
    APSARA_TEST_EQUAL_FATAL(2, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "line1\nline2";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * 2, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(2, processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "contentline1\nline2";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * 2, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(0, processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcParseErrorTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcKeyCountNotMatchErrorTotal->GetValue());
}

void ProcessorParseRegexNativeUnittest::TestProcessRegex() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("key1");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value3\tvalue4"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1",
                    "key2" : "value2",
                    "rawLog" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "value3",
                    "key2" : "value4",
                    "rawLog" : "value3\tvalue4"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GT_FATAL(processorInstance.mProcTimeMS->GetValue(), 0);
}

void ProcessorParseRegexNativeUnittest::TestProcessRegexRaw() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("rawLog");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value3\tvalue4"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key2" : "value2",
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key2" : "value4",
                    "rawLog" : "value3"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GT_FATAL(processorInstance.mProcTimeMS->GetValue(), 0);
}

void ProcessorParseRegexNativeUnittest::TestProcessRegexContent() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("content");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value3\tvalue4"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1",
                    "key2" : "value2",
                    "rawLog" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value3",
                    "key2" : "value4",
                    "rawLog" : "value3\tvalue4"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GT_FATAL(processorInstance.mProcTimeMS->GetValue(), 0);
}

void ProcessorParseRegexNativeUnittest::TestAddLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("content");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));

    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto logEvent = eventGroup.CreateLogEvent();
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    APSARA_TEST_EQUAL_FATAL(strlen(key) + strlen(value) + 5, processor.GetContext().GetProcessProfile().logGroupSize);
}

void ProcessorParseRegexNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("key1");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);


    int count = 5;

    // check observablity
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().regexMatchFailures);
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);

    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "rawLogvalue1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(0, processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(count, processor.mProcParseErrorTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcKeyCountNotMatchErrorTotal->GetValue());
}

void ProcessorParseRegexNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("key1");
    config["Keys"].append("key2");
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);


    int count = 5;

    // check observablity
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().regexMatchFailures);
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);

    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(0, processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(count, processor.mProcParseErrorTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcKeyCountNotMatchErrorTotal->GetValue());
}

void ProcessorParseRegexNativeUnittest::TestProcessEventKeyCountUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Regex"] = R"((\w+)\t(\w+).*)";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("key1");
    config["Keys"].append("key2");
    config["Keys"].append("key3");
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1\tvalue2"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);

    // run function
    ProcessorParseRegexNative& processor = *(new ProcessorParseRegexNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, ctx));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);


    int count = 5;
    // check observablity
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().regexMatchFailures);
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);

    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1\tvalue2";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(0, processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processor.mProcDiscardRecordsTotal->GetValue());

    // mProcKeyCountNotMatchErrorTotal should equal count
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcParseErrorTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processor.mProcKeyCountNotMatchErrorTotal->GetValue());
}

UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessWholeLine)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessRegex)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestAddLog)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessEventKeepUnmatch)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessEventDiscardUnmatch)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessEventKeyCountUnmatch)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessRegexRaw)
UNIT_TEST_CASE(ProcessorParseRegexNativeUnittest, TestProcessRegexContent)

} // namespace logtail

UNIT_TEST_MAIN
