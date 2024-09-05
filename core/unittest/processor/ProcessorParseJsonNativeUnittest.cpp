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
#include "plugin/processor/ProcessorParseJsonNative.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseJsonNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcessJson();
    void TestProcessJsonEscapedNullByte();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestProcessJsonContent();
    void TestProcessJsonRaw();
    void TestMultipleLines();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestInit);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestAddLog);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessJson);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessJsonEscapedNullByte);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessEventKeepUnmatch);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessEventDiscardUnmatch);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessJsonContent);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestProcessJsonRaw);

UNIT_TEST_CASE(ProcessorParseJsonNativeUnittest, TestMultipleLines);

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"testgetPluginID", "testNodeID", "testNodeChildID"};
    return pluginMeta;
}

void ProcessorParseJsonNativeUnittest::TestMultipleLines() {
    // error json
    {
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}\u0000{\"name\":\"Mike\",\"age\":25,\"is_student\":asdfsadf,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
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
                        "rawLog" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}",
                        "time" : "07/Jul/2022:10:30:28",
                        "url" : "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "__raw_log__" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":asdfsadf,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                        "rawLog" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":asdfsadf,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
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
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = true;
            config["CopingRawLog"] = true;
            config["RenamedSourceKey"] = "rawLog";
            config["SplitChar"] = '\0';
            config["AppendingLogPositionMeta"] = false;


            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseJsonNative
            ProcessorParseJsonNative& processorParseJsonNative = *(new ProcessorParseJsonNative);
            ProcessorInstance processorInstance(&processorParseJsonNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseJsonNative.Process(eventGroup);

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
                        "content" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}\u0000{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
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
                        "rawLog" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}",
                        "time" : "07/Jul/2022:10:30:28",
                        "url" : "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                        "age":"25",
                        "courses":"[\"Math\",\"English\",\"Science\"]",
                        "is_student":"false",
                        "name":"Mike",
                        "rawLog" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                        "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
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
            config["KeepingSourceWhenParseFail"] = true;
            config["KeepingSourceWhenParseSucceed"] = true;
            config["CopingRawLog"] = true;
            config["RenamedSourceKey"] = "rawLog";
            config["SplitChar"] = '\0';
            config["AppendingLogPositionMeta"] = false;


            // run function ProcessorSplitLogStringNative
            ProcessorSplitLogStringNative processor;
            processor.SetContext(mContext);
            APSARA_TEST_TRUE_FATAL(processor.Init(config));
            processor.Process(eventGroup);

            // run function ProcessorParseJsonNative
            ProcessorParseJsonNative& processorParseJsonNative = *(new ProcessorParseJsonNative);
            ProcessorInstance processorInstance(&processorParseJsonNative, getPluginMeta());
            APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
            processorParseJsonNative.Process(eventGroup);

            // judge result
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseJsonNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
}

void ProcessorParseJsonNativeUnittest::TestAddLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto logEvent = eventGroup.CreateLogEvent();
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    APSARA_TEST_EQUAL_FATAL(int(strlen(key) + strlen(value) + 5),
                            processor.GetContext().GetProcessProfile().logGroupSize);
}

void ProcessorParseJsonNativeUnittest::TestProcessJsonEscapedNullByte() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
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
                    "content" : "{\"level\":\"ERROR\",\"time\":\"2024-07-04T06:59:23.078Z\",\"msg\":\"expect { or n, but found \\u0000, error found in #0 byte of ...\"}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
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
                    "level": "ERROR",
	                "msg": "expect { or n, but found \u0000, error found in #0 byte of ...",
	                "time": "2024-07-04T06:59:23.078Z"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

void ProcessorParseJsonNativeUnittest::TestProcessJson() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
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
                    "content" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
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
                    "rawLog" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"}",
                    "time" : "07/Jul/2022:10:30:28",
                    "url" : "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                    "age":"25",
                    "courses":"[\"Math\",\"English\",\"Science\"]",
                    "is_student":"false",
                    "name":"Mike",
                    "rawLog" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                    "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

void ProcessorParseJsonNativeUnittest::TestProcessJsonContent() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    // the first event has key "content" in json key with overwrites sourceKey "content"
    // the second event doesn't have key "content" in json
    // after parsing, the first event's content should be the value in json, the original content should be the value of
    // "rawLog" the second event's content should be dropped, the original content should be the value of "rawLog"

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"content\":\"content_test\",\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678902,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
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
                    "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                    "age":"25",
                    "content":"content_test",
                    "courses":"[\"Math\",\"English\",\"Science\"]",
                    "is_student":"false",
                    "name":"Mike",
                    "rawLog" : "{\"content\":\"content_test\",\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                    "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                    "age":"25",
                    "courses":"[\"Math\",\"English\",\"Science\"]",
                    "is_student":"false",
                    "name":"Mike",
                    "rawLog" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                    "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                },
                "timestamp" : 12345678902,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

void ProcessorParseJsonNativeUnittest::TestProcessJsonRaw() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";

    // make events
    // the first event has key "rawLog" in json key with overwrites rawLogTag "rawLog"
    // the second event doesn't have key "rawLog" in json
    // after parsing, the first event's rawLog should be the value in json, the original content should be discarded
    // the second event's rawLog should be the original content
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\"rawLog\":\"content_test\",\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678902,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
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
                    "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                    "age":"25",
                    "courses":"[\"Math\",\"English\",\"Science\"]",
                    "is_student":"false",
                    "name":"Mike",
                    "rawLog" : "content_test",
                    "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "address" : "{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"}",
                    "age":"25",
                    "courses":"[\"Math\",\"English\",\"Science\"]",
                    "is_student":"false",
                    "name":"Mike",
                    "rawLog" : "{\"name\":\"Mike\",\"age\":25,\"is_student\":false,\"address\":{\"city\":\"Hangzhou\",\"postal_code\":\"100000\"},\"courses\":[\"Math\",\"English\",\"Science\"],\"scores\":{\"Math\":90,\"English\":85,\"Science\":95}}",
                    "scores":"{\"Math\":90,\"English\":85,\"Science\":95}"
                },
                "timestamp" : 12345678902,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

void ProcessorParseJsonNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
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
                    "content" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\""
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);


    int count = 1;

    // check observablity
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue
        = "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "rawLog{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": "
                  "\"07/Jul/2022:10:30:28\"";
    APSARA_TEST_EQUAL_FATAL(uint64_t(expectValue.length() * count), processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "rawLog" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\""
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

void ProcessorParseJsonNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
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
                    "content" : "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\""
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseJsonNative& processor = *(new ProcessorParseJsonNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);


    int count = 1;

    // check observablity
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue
        = "{\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\"time\": \"07/Jul/2022:10:30:28\"";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());

    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    APSARA_TEST_GE_FATAL(processorInstance.mProcTimeMS->GetValue(), uint64_t(0));
}

} // namespace logtail

UNIT_TEST_MAIN
