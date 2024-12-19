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
#include "config/PipelineConfig.h"
#include "models/LogEvent.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "plugin/processor/ProcessorParseDelimiterNative.h"
#include "plugin/processor/inner/ProcessorMergeMultilineLogNative.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseDelimiterNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        BOOL_FLAG(ilogtail_discard_old_data) = false;
    }

    void TestInit();
    void TestMultipleLinesWithProcessorMergeMultilineLogNative();
    void TestMultipleLines();
    void TestProcessWholeLine();
    void TestProcessQuote();
    void TestProcessDoubleQuote();
    void TestProcessKeyOverwritten();
    void TestUploadRawLog();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestAllowingShortenedFields();
    void TestExtend();
    void TestEmpty();
    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestMultipleLinesWithProcessorMergeMultilineLogNative);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestMultipleLines);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessWholeLine);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessQuote);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessDoubleQuote);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessKeyOverwritten);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestUploadRawLog);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestAddLog);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessEventDiscardUnmatch);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestAllowingShortenedFields);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestExtend);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestEmpty);

PluginInstance::PluginMeta getPluginMeta() {
    PluginInstance::PluginMeta pluginMeta{"1"};
    return pluginMeta;
}

void ProcessorParseDelimiterNativeUnittest::TestAllowingShortenedFields() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = true;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "method": "POST",
                    "rawLog": "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
                    "request_time": "0.024",
                    "time": "2013-10-31 21:03:49",
                    "url": "PutData?Category=YunOsAccountOpLog"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "rawLog": "value1",
                    "time": "value1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // AllowingShortenedFields false
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@45
012@@34"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__raw__": "123@@45"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__raw__": "012@@34"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
    // AllowingShortenedFields true
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@45
012@@34"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "a": "123",
                        "b": "45"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "a": "012",
                        "b": "34"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = true;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = true;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseDelimiterNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;

    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
}

void ProcessorParseDelimiterNativeUnittest::TestExtend() {
    // not Extend
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@1@@2@@3
012@@345@@1@@2@@3"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__column3__": "@@2@@3",
                        "a": "123",
                        "b": "456",
                        "c": "1"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__column3__": "@@2@@3",
                        "a": "012",
                        "b": "345",
                        "c": "1"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["OverflowedFieldsTreatment"] = "keep";
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["OverflowedFieldsTreatment"] = "keep";
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
    // not Extend
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@1@@2@@3
012@@345@@1@@2@@3"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__column3__": "2",
                        "__column4__": "3",
                        "a": "123",
                        "b": "456",
                        "c": "1"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__column3__": "2",
                        "__column4__": "3",
                        "a": "012",
                        "b": "345",
                        "c": "1"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseDelimiterNativeUnittest::TestMultipleLines() {
    // case < field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456
012@@345"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__raw__": "123@@456"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__raw__": "012@@345"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;


            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }

    // case > field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@789
012@@345@@678"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__column2__": "789",
                        "a": "123",
                        "b": "456"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__column2__": "678",
                        "a": "012",
                        "b": "345"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }

    // case = field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@789
012@@345@@678"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "a": "123",
                        "b": "456",
                        "c": "789"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "a": "012",
                        "b": "345",
                        "c": "678"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseDelimiterNativeUnittest::TestMultipleLinesWithProcessorMergeMultilineLogNative() {
    // case < field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456
012@@345"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__raw__": "123@@456"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__raw__": "012@@345"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;


            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorMergeMultilineLogNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[123|012].*";
            config["MergeType"] = "regex";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processorSplitLogStringNative;
            processorSplitLogStringNative.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
            processorSplitLogStringNative.Process(eventGroup);

            // run function ProcessorMergeMultilineLogNative
            ProcessorMergeMultilineLogNative processorMergeMultilineLogNative;
            processorMergeMultilineLogNative.SetContext(mContext);
            processorMergeMultilineLogNative.SetMetricsRecordRef(ProcessorMergeMultilineLogNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processorMergeMultilineLogNative.Init(config));
            processorMergeMultilineLogNative.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }

    // case > field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@789
012@@345@@678"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "__column2__": "789",
                        "a": "123",
                        "b": "456"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "__column2__": "678",
                        "a": "012",
                        "b": "345"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorMergeMultilineLogNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[123|012].*";
            config["MergeType"] = "regex";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processorSplitLogStringNative;
            processorSplitLogStringNative.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
            processorSplitLogStringNative.Process(eventGroup);

            // run function ProcessorMergeMultilineLogNative
            ProcessorMergeMultilineLogNative processorMergeMultilineLogNative;
            processorMergeMultilineLogNative.SetContext(mContext);
            processorMergeMultilineLogNative.SetMetricsRecordRef(ProcessorMergeMultilineLogNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processorMergeMultilineLogNative.Init(config));
            processorMergeMultilineLogNative.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }

    // case = field
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "123@@456@@789
012@@345@@678"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "a": "123",
                        "b": "456",
                        "c": "789"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "a": "012",
                        "b": "345",
                        "c": "678"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";

        // ProcessorSplitLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["SplitChar"] = '\n';
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
        // ProcessorMergeMultilineLogNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "@@";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("a");
            config["Keys"].append("b");
            config["Keys"].append("c");
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = false;
            config["StartPattern"] = "[123|012].*";
            config["MergeType"] = "regex";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processorSplitLogStringNative;
            processorSplitLogStringNative.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
            processorSplitLogStringNative.Process(eventGroup);

            // run function ProcessorMergeMultilineLogNative
            ProcessorMergeMultilineLogNative processorMergeMultilineLogNative;
            processorMergeMultilineLogNative.SetContext(mContext);
            processorMergeMultilineLogNative.SetMetricsRecordRef(ProcessorMergeMultilineLogNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processorMergeMultilineLogNative.Init(config));
            processorMergeMultilineLogNative.Process(eventGroup);
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);
            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseDelimiterNativeUnittest::TestProcessWholeLine() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,PutData?Category=YunOsAccountOpLog,0.024"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:04:49,POST,PutData?Category=YunOsAccountOpLog,0.024"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "method": "POST",
                    "request_time": "0.024",
                    "time": "2013-10-31 21:03:49",
                    "url": "PutData?Category=YunOsAccountOpLog"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "method": "POST",
                    "request_time": "0.024",
                    "time": "2013-10-31 21:04:49",
                    "url": "PutData?Category=YunOsAccountOpLog"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseDelimiterNativeUnittest::TestProcessQuote() {
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "'-' 'file0' '947113' '192.168.0.3' '2024-08-15T15:25:24.195264681' 'PUT
/dir/resource.txt
HTTP/2.0' '200' '154' 'go-sdk'"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "1": "-",
                        "2": "file0",
                        "3": "947113",
                        "4": "192.168.0.3",
                        "5": "2024-08-15T15:25:24.195264681",
                        "6": "PUT\n/dir/resource.txt\nHTTP/2.0",
                        "7": "200",
                        "8": "154",
                        "9": "go-sdk"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = " ";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("1");
            config["Keys"].append("2");
            config["Keys"].append("3");
            config["Keys"].append("4");
            config["Keys"].append("5");
            config["Keys"].append("6");
            config["Keys"].append("7");
            config["Keys"].append("8");
            config["Keys"].append("9");
            config["KeepingSourceWhenParseFail"] = false;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = true;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            std::string pluginId = "testID";
            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
    {
        std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : " 2023-12-25 1|zdfvzdfv zfdv|zfdvzdfv zfd|fzdvzdfvzdfvz|zfvzfdzv zfdb|zfdvzdfbvzb|zdfvzdfbvzdb|'advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb '|zdfbvzbszfbsfb
2023-12-25 1|zdfvzdfv zfdv|zfdvzdfv zfd|fzdvzdfvzdfvz|zfvzfdzv zfdb|zfdvzdfbvzb|zdfvzdfbvzdb|'advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb '|zdfbvzbszfbsfb
    2023-12-25 1|zdfvzdfv zfdv|zfdvzdfv zfd|fzdvzdfvzdfvz|zfvzfdzv zfdb|zfdvzdfbvzb|zdfvzdfbvzdb|'advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb '|zdfbvzbszfbsfb
        2023-12-25 1|zdfvzdfv zfdv|zfdvzdfv zfd|fzdvzdfvzdfvz|zfvzfdzv zfdb|zfdvzdfbvzb|zdfvzdfbvzdb|'advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb '|zdfbvzbszfbsfb"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
        })";

        std::string expectJson = R"({
            "events": [
                {
                    "contents": {
                        "1": "2023-12-25 1",
                        "2": "zdfvzdfv zfdv",
                        "3": "zfdvzdfv zfd",
                        "4": "fzdvzdfvzdfvz",
                        "5": "zfvzfdzv zfdb",
                        "6": "zfdvzdfbvzb",
                        "7": "zdfvzdfbvzdb",
                        "8": "advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb ",
                        "9": "zdfbvzbszfbsfb"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "1": "2023-12-25 1",
                        "2": "zdfvzdfv zfdv",
                        "3": "zfdvzdfv zfd",
                        "4": "fzdvzdfvzdfvz",
                        "5": "zfvzfdzv zfdb",
                        "6": "zfdvzdfbvzb",
                        "7": "zdfvzdfbvzdb",
                        "8": "advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb ",
                        "9": "zdfbvzbszfbsfb"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "1": "2023-12-25 1",
                        "2": "zdfvzdfv zfdv",
                        "3": "zfdvzdfv zfd",
                        "4": "fzdvzdfvzdfvz",
                        "5": "zfvzfdzv zfdb",
                        "6": "zfdvzdfbvzb",
                        "7": "zdfvzdfbvzdb",
                        "8": "advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb ",
                        "9": "zdfbvzbszfbsfb"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                },
                {
                    "contents": {
                        "1": "2023-12-25 1",
                        "2": "zdfvzdfv zfdv",
                        "3": "zfdvzdfv zfd",
                        "4": "fzdvzdfvzdfvz",
                        "5": "zfvzfdzv zfdb",
                        "6": "zfdvzdfbvzb",
                        "7": "zdfvzdfbvzdb",
                        "8": "advfawevaevb|dvzdfvzdbfazdb|zdfvbzdfb ",
                        "9": "zdfbvzbszfbsfb"
                    },
                    "timestamp": 12345678901,
                    "timestampNanosecond": 0,
                    "type": 1
                }
            ]
        })";
        // ProcessorSplitMultilineLogStringNative
        {
            // make events
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            // make config
            Json::Value config;
            config["SourceKey"] = "content";
            config["Separator"] = "|";
            config["Quote"] = "'";
            config["Keys"] = Json::arrayValue;
            config["Keys"].append("1");
            config["Keys"].append("2");
            config["Keys"].append("3");
            config["Keys"].append("4");
            config["Keys"].append("5");
            config["Keys"].append("6");
            config["Keys"].append("7");
            config["Keys"].append("8");
            config["Keys"].append("9");
            config["Keys"].append("10");
            config["Keys"].append("11");
            config["KeepingSourceWhenParseFail"] = false;
            config["KeepingSourceWhenParseSucceed"] = false;
            config["CopingRawLog"] = false;
            config["RenamedSourceKey"] = "__raw__";
            config["AllowingShortenedFields"] = true;
            config["StartPattern"] = "[a-zA-Z0-9]*";
            config["UnmatchedContentTreatment"] = "single_line";
            config["AppendingLogPositionMeta"] = false;

            // run function ProcessorSplitMultilineLogStringNative
            ProcessorSplitMultilineLogStringNative processor;
            processor.SetContext(mContext);
            processor.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseDelimiterNative
            ProcessorParseDelimiterNative& processorParseDelimiterNative = *(new ProcessorParseDelimiterNative);
            ProcessorInstance processorInstance(&processorParseDelimiterNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseDelimiterNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
    {
        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Separator"] = ",";
        config["Quote"] = "'";
        config["Keys"] = Json::arrayValue;
        config["Keys"].append("time");
        config["Keys"].append("method");
        config["Keys"].append("url");
        config["Keys"].append("request_time");
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["RenamedSourceKey"] = "rawLog";
        config["AllowingShortenedFields"] = false;
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOs'AccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "0.024",
                        "time": "2013-10-31 21:03:49",
                        "url": "PutData?Category=YunOsAccountOpLog"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,'PutData?Category=YunOs'AccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseDelimiterNativeUnittest::TestProcessDoubleQuote() {
    {
        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Separator"] = ",";
        config["Quote"] = "'";
        config["Keys"] = Json::arrayValue;
        config["Keys"].append("time");
        config["Keys"].append("method");
        config["Keys"].append("url");
        config["Keys"].append("request_time");
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["RenamedSourceKey"] = "rawLog";
        config["AllowingShortenedFields"] = false;
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,''PutData?Category=YunOsAccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,PutData?Category=YunOs''AccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,PutData?Category=YunOsAccountOpLog'',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,''PutData?Category=YunOsAccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=Yun'Os'AccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOs''AccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog'',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'''PutData?Category=YunOs''AccountOpLog''',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,''PutData?Category=YunOsAccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,PutData?Category=YunOs''AccountOpLog,0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,PutData?Category=YunOsAccountOpLog'',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,''PutData?Category=YunOsAccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,'PutData?Category=Yun'Os'AccountOpLog',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "0.024",
                        "time": "2013-10-31 21:03:49",
                        "url": "PutData?Category=YunOs'AccountOpLog"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog'',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "0.024",
                        "time": "2013-10-31 21:03:49",
                        "url": "'PutData?Category=YunOs'AccountOpLog'"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseDelimiterNativeUnittest::TestProcessKeyOverwritten() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("rawLog");
    config["Keys"].append("content");
    config["Keys"].append("__raw_log__");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__raw_log__": "0.024",
                    "content": "PutData?Category=YunOsAccountOpLog",
                    "rawLog": "POST",
                    "time": "2013-10-31 21:03:49"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseDelimiterNativeUnittest::TestUploadRawLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "method": "POST",
                    "rawLog" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
                    "request_time": "0.024",
                    "time": "2013-10-31 21:03:49",
                    "url": "PutData?Category=YunOsAccountOpLog"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseDelimiterNativeUnittest::TestAddLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");

    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto logEvent = eventGroup.CreateLogEvent();
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    // Todo: add metrics for check result
}

void ProcessorParseDelimiterNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;
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
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
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
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mInEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mOutEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseDelimiterNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["Separator"] = ",";
    config["Quote"] = "'";
    config["Keys"] = Json::arrayValue;
    config["Keys"].append("time");
    config["Keys"].append("method");
    config["Keys"].append("url");
    config["Keys"].append("request_time");
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["AllowingShortenedFields"] = false;
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
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mInEventsTotal->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mOutEventsTotal->GetValue());
    // event group size is not 0
    APSARA_TEST_NOT_EQUAL_FATAL(uint64_t(0), processorInstance.mOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseDelimiterNativeUnittest::TestEmpty() {
    {
        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Separator"] = ",";
        config["Quote"] = "'";
        config["Keys"] = Json::arrayValue;
        config["Keys"].append("time");
        config["Keys"].append("method");
        config["Keys"].append("url");
        config["Keys"].append("request_time");
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["RenamedSourceKey"] = "rawLog";
        config["AllowingShortenedFields"] = false;
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ",POST,,"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,''',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,'''',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2013-10-31 21:03:49,POST,',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run function
        ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);
        std::string expectJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "",
                        "time": "",
                        "url": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "0.024",
                        "time": "2013-10-31 21:03:49",
                        "url": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,''',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "method": "POST",
                        "request_time": "0.024",
                        "time": "2013-10-31 21:03:49",
                        "url": "'"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "rawLog": "2013-10-31 21:03:49,POST,',0.024"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond": 0,
                    "type" : 1
                }
            ]
        })";
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

} // namespace logtail

UNIT_TEST_MAIN