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
#include "common/ExceptionBase.h"
#include "common/JsonUtil.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "plugin/processor/ProcessorFilterNative.h"
#include "unittest/Unittest.h"

using boost::regex;
using namespace std;

namespace logtail {

class ProcessorFilterNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void OnSuccessfulInit();
    void OnFailedInit();
    void TestLogFilterRule();
    void TestBaseFilter();
    void TestFilterNoneUtf8();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorFilterNativeUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, OnFailedInit)
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestLogFilterRule)
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestBaseFilter)
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestFilterNoneUtf8)

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"testgetPluginID", "testNodeID", "testNodeChildID"};
    return pluginMeta;
}

void ProcessorFilterNativeUnittest::OnSuccessfulInit() {
    unique_ptr<ProcessorFilterNative> processor;
    Json::Value configJson;
    string configStr, errorMsg;

    // FilterKey + FilterRegex
    configStr = R"(
        {
            "Type": "processor_filter_regex_native",
            "FilterKey": [
                "a"
            ],
            "FilterRegex": [
                "b"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    processor.reset(new ProcessorFilterNative());
    processor->SetContext(mContext);
    processor->SetMetricsRecordRef(ProcessorFilterNative::sName, "1", "1", "1");
    APSARA_TEST_TRUE(processor->Init(configJson));
    APSARA_TEST_EQUAL(1, processor->mFilterRule->FilterKeys.size());
    APSARA_TEST_EQUAL(1, processor->mFilterRule->FilterRegs.size());
}

void ProcessorFilterNativeUnittest::OnFailedInit() {
    unique_ptr<ProcessorFilterNative> processor;
    Json::Value configJson;
    string configStr, errorMsg;

    // FilterKey + FilterRegex
    configStr = R"(
        {
            "Type": "processor_filter_regex_native",
            "FilterKey": [
                "a",
                "c"
            ],
            "FilterRegex": [
                "b"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    processor.reset(new ProcessorFilterNative());
    processor->SetContext(mContext);
    processor->SetMetricsRecordRef(ProcessorFilterNative::sName, "1", "1", "1");
    APSARA_TEST_FALSE(processor->Init(configJson));

    configStr = R"(
        {
            "Type": "processor_filter_regex_native",
            "FilterKey": [
                "a",
                "a"
            ],
            "FilterRegex": [
                "b",
                "[a"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    processor.reset(new ProcessorFilterNative());
    processor->SetContext(mContext);
    processor->SetMetricsRecordRef(ProcessorFilterNative::sName, "1", "1", "1");
    APSARA_TEST_FALSE(processor->Init(configJson));
}

// To test bool ProcessorFilterNative::Filter(LogEvent& sourceEvent, const LogFilterRule* filterRule)
void ProcessorFilterNativeUnittest::TestLogFilterRule() {
    Json::Value config;
    config["Include"] = Json::Value(Json::objectValue);
    config["Include"]["key1"] = ".*value1";
    config["Include"]["key2"] = "value2.*";
    config["DiscardingNonUTF8"] = true;

    // run function
    ProcessorFilterNative& processor = *(new ProcessorFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    // case 1 : the field are all provided,  only one matched
    auto sourceBuffer1 = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup1(sourceBuffer1);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1xxxxx",
                    "key2" : "value2xxxxx"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup1.FromJsonString(inJson);
    // run function
    std::vector<PipelineEventGroup> eventGroupList1;
    eventGroupList1.emplace_back(std::move(eventGroup1));
    processorInstance.Process(eventGroupList1);

    std::string outJson = eventGroupList1[0].ToJsonString();
    // judge result
    std::string expectJson = R"({
        "events" : 
        [
            {
                "contents" : 
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());

    // case 2 : NOT all fields exist, it
    auto sourceBuffer2 = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup2(sourceBuffer2);
    inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "abcvalue1"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup2.FromJsonString(inJson);
    // run function
    std::vector<PipelineEventGroup> eventGroupList2;
    eventGroupList2.emplace_back(std::move(eventGroup2));
    processorInstance.Process(eventGroupList2);

    outJson = eventGroupList2[0].ToJsonString();
    // judge result
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
}
// To test bool ProcessorFilterNative::Filter(LogEvent& sourceEvent, const BaseFilterNodePtr& node)
void ProcessorFilterNativeUnittest::TestBaseFilter() {
    // case 1
    {
        Json::Value root;

        root["operator"] = "and";

        Json::Value operands1;
        operands1["key"] = "key1";
        operands1["exp"] = ".*value1";
        operands1["type"] = "regex";

        Json::Value operands2;
        operands2["key"] = "key2";
        operands2["exp"] = "value2.*";
        operands2["type"] = "regex";

        root["operands"].append(operands1);
        root["operands"].append(operands2);

        Json::Value config;
        config["ConditionExp"] = root;
        config["DiscardingNonUTF8"] = true;

        // run function
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        // case 1 : the field are all provided,  only one matched
        auto sourceBuffer1 = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup1(sourceBuffer1);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "key1" : "value1xxxxx",
                        "key2" : "value2xxxxx"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "key1" : "abcdeavalue1",
                        "key2" : "value2xxxxx"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup1.FromJsonString(inJson);
        // run function
        std::vector<PipelineEventGroup> eventGroupList1;
        eventGroupList1.emplace_back(std::move(eventGroup1));
        processorInstance.Process(eventGroupList1);

        std::string outJson = eventGroupList1[0].ToJsonString();
        // judge result
        std::string expectJson = R"({
            "events" : 
            [
                {
                    "contents" : 
                    {
                        "key1" : "abcdeavalue1",
                        "key2" : "value2xxxxx"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());

        // case 2 : NOT all fields exist, it
        auto sourceBuffer2 = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup2(sourceBuffer2);
        inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "key1" : "abcvalue1"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup2.FromJsonString(inJson);
        // run function
        std::vector<PipelineEventGroup> eventGroupList2;
        eventGroupList2.emplace_back(std::move(eventGroup2));
        processorInstance.Process(eventGroupList2);

        outJson = eventGroupList2[0].ToJsonString();
        // judge result
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
    // case 2
    {
        // a: int, b: string c: ip, d: date
        const char* jsonStr
            = "{\n"
              "  \"operator\": \"and\",\n"
              "  \"operands\": [\n"
              "    {\n"
              "      \"operator\": \"and\",\n"
              "      \"operands\": [\n"
              "        {\n"
              "          \"type\": \"regex\",\n"
              "          \"key\": \"a\",\n"
              "          \"exp\": \"\\\\d+\"\n"
              "        },\n"
              "      \t{\n"
              "      \t  \"operator\": \"not\",\n"
              "      \t  \"operands\": [\n"
              "      \t    {\n"
              "      \t      \"type\": \"regex\",\n"
              "              \"key\": \"d\",\n"
              "              \"exp\": \"20\\\\d{1,2}-\\\\d{1,2}-\\\\d{1,2}\"\n"
              "      \t    }\n"
              "      \t  ]\n"
              "      \t}\n"
              "      ]\n"
              "    },\n"
              "    {\n"
              "      \"operator\": \"or\",\n"
              "      \"operands\": [\n"
              "        {\n"
              "          \"type\": \"regex\",\n"
              "          \"key\": \"b\",\n"
              "          \"exp\": \"\\\\S+\"\n"
              "        },\n"
              "        {\n"
              "          \"type\": \"regex\",\n"
              "          \"key\": \"c\",\n"
              "          \"exp\": "
              "\"((2[0-4]\\\\d|25[0-5]|[01]?\\\\d\\\\d?)\\\\.){3}(2[0-4]\\\\d|25[0-5]|[01]?\\\\d\\\\d?)\"\n"
              "        }\n"
              "      ]\n"
              "    }\n"
              "  ]\n"
              "}";

        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        // (a and not d) and (b or c)
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        auto sourceBuffer1 = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup1(sourceBuffer1);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "a" : "100",
                        "b" : "xxx",
                        "c" : "192.168.1.1",
                        "d" : "2008-08-08"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "a" : "100",
                        "b" : "xxx",
                        "c" : "888.168.1.1",
                        "d" : "1999-1-1"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "a" : "aaa",
                        "b" : "xxx",
                        "c" : "8.8.8.8",
                        "d" : "2222-22-22"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup1.FromJsonString(inJson);
        // run function
        std::vector<PipelineEventGroup> eventGroupList1;
        eventGroupList1.emplace_back(std::move(eventGroup1));
        processorInstance.Process(eventGroupList1);

        std::string outJson = eventGroupList1[0].ToJsonString();
        // judge result
        // judge result
        std::string expectJson = R"({
            "events" : 
            [
                {
                    "contents" : 
                    {
                        "a" : "100",
                        "b" : "xxx",
                        "c" : "888.168.1.1",
                        "d" : "1999-1-1"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());

        APSARA_TEST_EQUAL_FATAL(2, processor.mProcFilterRecordsTotal->GetValue());
    }
    {
        const char* jsonStr = "{\n"
                              "  \"operator\": \"and\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"a\",\n"
                              "      \"exp\": \"regex1\"\n"
                              "    },\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"b\",\n"
                              "      \"exp\": \"regex2\"\n"
                              "    }\n"
                              "  ]\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    }

    {
        const char* jsonStr = "{\n"
                              "    \"key\": \"a\",\n"
                              "    \"exp\": \"xxx\",\n"
                              "    \"type\": \"regex\"\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    }

    {
        // not operator
        const char* jsonStr = "{\n"
                              "  \"operator\": \"not\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"a\",\n"
                              "      \"exp\": \"regex1\"\n"
                              "    }\n"
                              "  ]\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    }

    {
        // missing reg
        const char* jsonStr = "{\n"
                              "  \"operator\": \"and\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"a\",\n"
                              "      \"exp\": \"regex1\"\n"
                              "    },\n"
                              "    {\n"
                              "      \"operator\": \"or\",\n"
                              "      \"operands\": [\n"
                              "        {\n"
                              "          \"type\": \"regex\",\n"
                              "          \"key\": \"b\"\n"
                              "        },\n"
                              "        {\n"
                              "          \"type\": \"regex\",\n"
                              "          \"key\": \"c\",\n"
                              "          \"exp\": \"regex3\"\n"
                              "        }\n"
                              "      ]\n"
                              "    }\n"
                              "  ]\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(!processorInstance.Init(config, mContext));
    }

    {
        // missing right
        const char* jsonStr = "{\n"
                              "  \"operator\": \"and\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"a\",\n"
                              "      \"exp\": \"regex1\"\n"
                              "    },\n"
                              "    {\n"
                              "      \"operator\": \"or\",\n"
                              "      \"operands\": [\n"
                              "        {\n"
                              "          \"type\": \"regex\",\n"
                              "          \"key\": \"b\",\n"
                              "          \"exp\": \"regex2\"\n"
                              "        }\n"
                              "      ]\n"
                              "    }\n"
                              "  ]\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(!processorInstance.Init(config, mContext));
    }

    {
        // missing op
        const char* jsonStr = "{\n"
                              "  \"operator\": \"and\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"a\",\n"
                              "      \"exp\": \"regex1\"\n"
                              "    },\n"
                              "    {\n"
                              "      \"operands\": [\n"
                              "        {\n"
                              "          \"type\": \"regex\",\n"
                              "          \"key\": \"b\",\n"
                              "          \"exp\": \"regex2\"\n"
                              "        },\n"
                              "        {\n"
                              "          \"type\": \"regex\",\n"
                              "          \"key\": \"c\",\n"
                              "          \"exp\": \"regex3\"\n"
                              "        }\n"
                              "      ]\n"
                              "    }\n"
                              "  ]\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(!processorInstance.Init(config, mContext));
    }

    // redundant fields
    {
        const char* jsonStr = "{\n"
                              "  \"operator\": \"and\",\n"
                              "  \"operands\": [\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"b\",\n"
                              "      \"exp\": \"regex2\"\n"
                              "    },\n"
                              "    {\n"
                              "      \"type\": \"regex\",\n"
                              "      \"key\": \"c\",\n"
                              "      \"exp\": \"regex3\"\n"
                              "    }\n"
                              "  ],\n"
                              "  \"type\": \"regex\",\n"
                              "  \"key\": \"c\",\n"
                              "  \"exp\": \"regex3\"\n"
                              "}";
        Json::Reader reader;
        Json::Value rootNode;
        APSARA_TEST_TRUE_FATAL(reader.parse(jsonStr, rootNode));

        // init
        Json::Value config;
        config["ConditionExp"] = rootNode;
        config["DiscardingNonUTF8"] = true;
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        APSARA_TEST_TRUE(processor.mConditionExp->GetNodeType() == OPERATOR_NODE);
    }
}

static const char UTF8_BYTE_PREFIX = 0x80;
static const char UTF8_BYTE_MASK = 0xc0;

void ProcessorFilterNativeUnittest::TestFilterNoneUtf8() {
    const int caseCharacterNum = 80; // ten charactor for every situation
    std::string characterSet[caseCharacterNum];
    bool isBlunk[caseCharacterNum][4]; // every charactor has 4 byte atmost

    // generate one byte utf8;

    for (int i = 0; i < 10; ++i) {
        char tmp;
        do {
            tmp = rand() & 0xff;
            tmp &= 0x7f;
        } while (tmp == 32 || tmp == 9); // tmp should not be space or \t

        characterSet[i] = std::string(1, tmp);
        isBlunk[i][0] = false;
    }

    // generate one byte none utf8
    for (int i = 10; i < 20; ++i) {
        char tmp;
        do {
            tmp = rand() & 0xff;
            tmp |= 0x80;
            tmp &= 0xbf;
        } while (tmp == 32 || tmp == 9); // tmp shoud be 10xx xxxx;
        characterSet[i] = std::string(1, tmp);
        isBlunk[i][0] = true;
    }

    // generate two byte utf8

    for (int i = 20; i < 30; ++i) {
        char tmp1, tmp2;
        uint16_t unicode;
        do {
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp1 |= 0xc0;
            tmp1 &= 0xdf; // tmp1 should be 0x 110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf; // tmp2 should be 0x 10xx xxxx
            unicode = (((tmp1 & 0x1f) << 6) | (tmp2 & 0x3f));
        } while (!(unicode >= 0x80 && unicode <= 0x7ff));

        characterSet[i] = std::string(1, tmp1) + std::string(1, tmp2);
        isBlunk[i][0] = false;
        isBlunk[i][1] = false;
    }

    // generate two byte noneutf8
    char randArr1[10], randArr2[10], randArr3[10], randArr4[10];
    for (int i = 0; i < 10; ++i) {
        char tmp1, tmp2;
        tmp1 = rand() & 0xff;
        tmp2 = rand() & 0xff;
        tmp1 |= 0xc0;
        tmp1 &= 0xdf; // tmp1 should be 0x110x xxxx
        tmp2 |= 0x80;
        tmp2 &= 0xbf;
        randArr1[i] = tmp1;
        randArr2[i] = tmp2;
    }
    // five case with second binary 0xxx xxxx;
    for (int i = 0; i < 5; ++i) {
        do {
            randArr2[i] = rand() & 0xff;
            randArr2[i] &= 0x7f;
        } while (randArr2[i] == 32);
    }

    for (int index = 30; index < 35; ++index) {
        characterSet[index] = std::string(1, randArr1[index - 30]) + std::string(1, randArr2[index - 30]);
        isBlunk[index][0] = true;
        isBlunk[index][1] = false;
    }
    // five case of the situation thar only the format is utf8, but not unicode;

    for (int index = 35; index < 40; ++index) {
        randArr1[index - 30] &= 0xe1; // unicode must in rand [0x80,0x7fff]; ant two byte has 11 bits ,so the
                                      // situation can only be < 0x80
        characterSet[index] = std::string(1, randArr1[index - 30]) + std::string(1, randArr2[index - 30]);
        isBlunk[index][0] = true;
        isBlunk[index][1] = true;
    }

    // generate three bytes utf8

    for (int i = 40; i < 50; ++i) {
        char tmp1, tmp2, tmp3;
        uint16_t unicode;
        do {
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp3 = rand() & 0xff;
            tmp1 |= 0xe0;
            tmp1 &= 0xef; // tmp1 should be 0x 1110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf; // tmp2 should be 10xx xxxx
            tmp3 |= 0x80;
            tmp3 &= 0xbf; // tmp3 should be 10xx xxxx
            unicode = (((tmp1 & 0x0f) << 12) | ((tmp2 & 0x3f) << 6) | (tmp3 & 0x3f));
        } while (!(unicode >= 0x800));

        characterSet[i] = std::string(1, tmp1) + std::string(1, tmp2) + std::string(1, tmp3);
        isBlunk[i][0] = false;
        isBlunk[i][1] = false;
        isBlunk[i][2] = false;
    }

    // generate three bytes none utf8
    for (int i = 50; i < 60; ++i) {
        char tmp1, tmp2, tmp3;
        tmp1 = rand() & 0xff;
        tmp2 = rand() & 0xff;
        tmp3 = rand() & 0xff;
        tmp1 |= 0xe0;
        tmp1 &= 0xef; // tmp1 should be 0x 1110x xxxx
        tmp2 |= 0x80;
        tmp2 &= 0xbf; // tmp2 should be 10xx xxxx
        tmp3 |= 0x80;
        tmp3 &= 0xbf; // tmp3 should be 10xx xxxx
        randArr1[i - 50] = tmp1;
        randArr2[i - 50] = tmp2;
        randArr3[i - 50] = tmp3;
    }

    // the situation of 1110xxxx 0xxxxxxx 10xxxxxxx
    for (int i = 50; i < 52; ++i) {
        do {
            randArr2[i - 50] = rand() & 0xff;
            randArr2[i - 50] &= 0x7f; // second bytes is 0xxx xxxx;
        } while (randArr2[i - 50] == 32);
        characterSet[i]
            = std::string(1, randArr1[i - 50]) + std::string(1, randArr2[i - 50]) + std::string(1, randArr3[i - 50]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = false;
        isBlunk[i][2] = true;
    }
    // the situation of 1110xxxx 10xxxxxx 0xxxxxxx
    for (int i = 52; i < 54; ++i) {
        do {
            randArr3[i - 50] = rand() & 0xff;
            randArr3[i - 50] &= 0x7f; // second bytes is 0xxx xxxx;
        } while (randArr3[i - 50] == 32);
        characterSet[i]
            = std::string(1, randArr1[i - 50]) + std::string(1, randArr2[i - 50]) + std::string(1, randArr3[i - 50]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = false;
    }
    // the situation of 1110xxxx 0xxxxxxx 0xxxxxxx

    for (int i = 54; i < 56; ++i) {
        do {
            randArr2[i - 50] = rand() & 0xff;
            randArr2[i - 50] &= 0x7f;
            randArr3[i - 50] = rand() & 0xff;
            randArr3[i - 50] &= 0x7f; // second bytes is 0xxx xxxx
        } while (randArr3[i - 50] == 32 || randArr2[i - 50] == 32);
        characterSet[i]
            = std::string(1, randArr1[i - 50]) + std::string(1, randArr2[i - 50]) + std::string(1, randArr3[i - 50]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = false;
        isBlunk[i][2] = false;
    }

    // the situation of only format in utf8;
    for (int i = 56; i < 60; ++i) {
        randArr1[i - 50] &= 0xf0;
        randArr2[i - 50] &= 0xdf; // 1110 0000  100xxxxx 10xxxxxx

        characterSet[i]
            = std::string(1, randArr1[i - 50]) + std::string(1, randArr2[i - 50]) + std::string(1, randArr3[i - 50]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = true;
    }
    // generate four bytes utf8

    for (int i = 60; i < 70; ++i) {
        char tmp1, tmp2, tmp3, tmp4;
        uint32_t unicode;
        do {
            tmp1 = rand() & 0xff;
            tmp2 = rand() & 0xff;
            tmp3 = rand() & 0xff;
            tmp4 = rand() & 0xff;
            tmp1 |= 0xf0;
            tmp1 &= 0xf7; // tmp1 should be 0x 11110x xxxx
            tmp2 |= 0x80;
            tmp2 &= 0xbf; // tmp2 should be 10xx xxxx
            tmp3 |= 0x80;
            tmp3 &= 0xbf; // tmp3 should be 10xx xxxx
            tmp4 |= 0x80;
            tmp4 &= 0xbf; // tmp3 should be 10xx xxxx
            unicode = ((tmp1 & 0x07) << 18) | ((tmp2 & 0x3f) << 12) | ((tmp3 & 0x3f) << 6) | (tmp4 & 0x3f);
        } while (!(unicode >= 0x00010000 && unicode <= 0x0010ffff));

        characterSet[i] = std::string(1, tmp1) + std::string(1, tmp2) + std::string(1, tmp3) + std::string(1, tmp4);
        isBlunk[i][0] = false;
        isBlunk[i][1] = false;
        isBlunk[i][2] = false;
        isBlunk[i][3] = false;
    }

    // generate 4 bytes none utf8

    for (int i = 70; i < 80; ++i) {
        char tmp1, tmp2, tmp3, tmp4;
        tmp1 = rand() & 0xff;
        tmp2 = rand() & 0xff;
        tmp3 = rand() & 0xff;
        tmp4 = rand() & 0xff;
        tmp1 |= 0xf0;
        tmp1 &= 0xf7; // tmp1 should be 0x 1110x xxxx
        tmp2 |= 0x80;
        tmp2 &= 0xbf; // tmp2 should be 10xx xxxx
        tmp3 |= 0x80;
        tmp3 &= 0xbf; // tmp3 should be 10xx xxxx
        tmp4 |= 0x80;
        tmp4 &= 0xbf; // tmp3 should be 10xx xxxx

        randArr1[i - 70] = tmp1;
        randArr2[i - 70] = tmp2;
        randArr3[i - 70] = tmp3;
        randArr4[i - 70] = tmp4;
    }

    // the situation of 11110xxx 0xxxxxxx 10xxxxxxx 10xxxxxx
    for (int i = 70; i < 72; ++i) {
        do {
            randArr2[i - 70] = rand() & 0xff;
            randArr2[i - 70] &= 0x7f; // second bytes is 0xxx xxxx;
        } while (randArr2[i - 70] == 32);

        characterSet[i] = std::string(1, randArr1[i - 70]) + std::string(1, randArr2[i - 70])
            + std::string(1, randArr3[i - 70]) + std::string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = false;
        isBlunk[i][2] = true;
        isBlunk[i][3] = true;
    }
    // the situation of 1110xxxx 10xxxxxx 0xxxxxxx 10xxxxxx
    for (int i = 72; i < 74; ++i) {
        do {
            randArr3[i - 70] = rand() & 0xff;
            randArr3[i - 70] &= 0x7f; // second bytes is 0xxx xxxx;
        } while (randArr3[i - 70] == 32);
        characterSet[i] = std::string(1, randArr1[i - 70]) + std::string(1, randArr2[i - 70])
            + std::string(1, randArr3[i - 70]) + std::string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = false;
        isBlunk[i][3] = true;
    }
    // the situation of 1110xxxx 0xxxxxxx 0xxxxxxx 0xxxxxxxx

    for (int i = 74; i < 76; ++i) {
        do {
            randArr2[i - 70] = rand() & 0xff;
            randArr2[i - 70] &= 0x7f;
            randArr3[i - 70] = rand() & 0xff;
            randArr3[i - 70] &= 0x7f; // second bytes is 0xxx xxxx
            randArr4[i - 70] = rand() & 0xff;
            randArr4[i - 70] &= 0x7f; // second bytes is 0xxx xxxx
        } while (randArr4[i - 70] == 32 || randArr2[i - 70] == 32 || randArr3[i - 70] == 32);
        characterSet[i] = std::string(1, randArr1[i - 70]) + std::string(1, randArr2[i - 70])
            + std::string(1, randArr3[i - 70]) + std::string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = false;
        isBlunk[i][2] = false;
        isBlunk[i][3] = false;
    }

    // the situation of only format in utf8; and the real unicode is not in range

    // less than range
    for (int i = 76; i < 78; ++i) {
        randArr1[i - 70] &= 0xf0;
        randArr2[i - 70] &= 0x8f; // 1110 0000  100xxxxx 10xxxxxx
        characterSet[i] = std::string(1, randArr1[i - 70]) + std::string(1, randArr2[i - 70])
            + std::string(1, randArr3[i - 70]) + std::string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = true;
        isBlunk[i][3] = true;
    }

    // greater than range
    for (int i = 78; i < 80; ++i) {
        randArr1[i - 70] |= 0x04;
        randArr2[i - 70] |= 0x10; // 1110 0000  100xxxxx 10xxxxxx

        characterSet[i] = std::string(1, randArr1[i - 70]) + std::string(1, randArr2[i - 70])
            + std::string(1, randArr3[i - 70]) + std::string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = true;
        isBlunk[i][3] = true;
    }

    for (int i = 0; i < 10; ++i) {
        std::string testStr;
        const int CHARACTER_COUNT = 8192;
        bool flow[CHARACTER_COUNT * 4];
        int index = 0; // index of flow
        // generate test string with character randomly, and record whether a position should be replaced by blunck
        for (int j = 0; j < CHARACTER_COUNT; ++j) {
            int randIndex = rand() % 80;
            testStr += characterSet[randIndex];
            if (randIndex >= 0 && randIndex < 20) {
                flow[index] = isBlunk[randIndex][0];
                index++;
            } else if (randIndex >= 20 && randIndex < 40) {
                flow[index] = isBlunk[randIndex][0];
                flow[index + 1] = isBlunk[randIndex][1];
                index += 2;
            } else if (randIndex >= 40 && randIndex < 60) {
                flow[index] = isBlunk[randIndex][0];
                flow[index + 1] = isBlunk[randIndex][1];
                flow[index + 2] = isBlunk[randIndex][2];
                index += 3;
            } else if (randIndex >= 60 && randIndex < 80) {
                flow[index] = isBlunk[randIndex][0];
                flow[index + 1] = isBlunk[randIndex][1];
                flow[index + 2] = isBlunk[randIndex][2];
                flow[index + 3] = isBlunk[randIndex][3];
                index += 4;
            }

            if (j == (CHARACTER_COUNT - 1) && randIndex >= 20
                && randIndex % 20 < 10) // the last character of string ,and at least two bytes,ant is utf8
            {
                testStr = testStr.substr(0, testStr.size() - 1);
                if (randIndex >= 20 && randIndex < 30)
                    flow[index - 2] = true;
                else if (randIndex >= 40 && randIndex < 50)
                    flow[index - 3] = flow[index - 2] = true;
                else if (randIndex >= 60 && randIndex < 70)
                    flow[index - 4] = flow[index - 3] = flow[index - 2] = true;
            }
        }
        ProcessorFilterNative processor;
        processor.FilterNoneUtf8(testStr);
        for (uint32_t indexOfString = 0; indexOfString < testStr.size(); ++indexOfString) {
            if (flow[indexOfString] == true) {
                APSARA_TEST_EQUAL_FATAL(testStr[indexOfString], ' ');
            } else {
                APSARA_TEST_NOT_EQUAL_FATAL(testStr[indexOfString], ' ');
            }
        }
    }
} // end of case

} // namespace logtail

UNIT_TEST_MAIN
