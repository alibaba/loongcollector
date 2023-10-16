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
#include "unittest/Unittest.h"
#include "common/JsonUtil.h"
#include "processor/ProcessorFilterNative.h"
#include "plugin/instance/ProcessorInstance.h"
#include "common/ExceptionBase.h"
#include "config/UserLogConfigParser.h"

using namespace std;
using boost::regex;

DECLARE_FLAG_STRING(user_log_config);

namespace logtail {

class ProcessorFilterNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
    }
    // GetFilterFule constructs LogFilterRule according to @filterKeys and @filterRegs.
    // **Will throw exception if @filterKeys.size() != @filterRegs.size() or failed**.
    LogFilterRule* GetFilterFule(const Json::Value& filterKeys, const Json::Value& filterRegs) {
        if (filterKeys.size() != filterRegs.size()) {
            throw ExceptionBase(std::string("The filterKey size is ") + ToString(filterKeys.size())
                                + std::string(", while the filterRegs size is ") + ToString(filterRegs.size()));
        }

        LogFilterRule* rulePtr = new LogFilterRule();
        for (uint32_t i = 0; i < filterKeys.size(); i++) {
            try {
                rulePtr->FilterKeys.push_back(filterKeys[i].asString());
                rulePtr->FilterRegs.push_back(boost::regex(filterRegs[i].asString()));
            } catch (const exception& e) {
                LOG_WARNING(sLogger, ("The filter is invalid", e.what()));
                delete rulePtr;
                throw;
            } catch (...) {
                LOG_WARNING(sLogger, ("The filter is invalid reason unknow", ""));
                delete rulePtr;
                throw;
            }
        }
        return rulePtr;
    }

    void TestInit();
    void TestInitFilterBypass();
    void TestFilter();
    void TestLogFilterRule();
    void TestBaseFilter();
    void TestFilterNoneUtf8();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestInitFilterBypass);
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestFilter);
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestLogFilterRule);
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestBaseFilter);
UNIT_TEST_CASE(ProcessorFilterNativeUnittest, TestFilterNoneUtf8);

void ProcessorFilterNativeUnittest::TestInit() {
    string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : [\"key1\", "
                    "\"key2\"], \"regs\" : [\"value1\",\"value2\"]}, {\"project_name\" : \"456_proj\", \"category\" : "
                    "\"test_1\", \"keys\" : [\"key1\", \"key2\"], \"regs\" : [\"value1\",\"value2\"]}]}";
    string path = GetProcessExecutionDir() + "user_log_config.json";
    ofstream file(path.c_str());
    file << config;
    file.close();

    Config mConfig;
    mConfig.mAdvancedConfig.mFilterExpressionRoot;
    mConfig.mFilterRule = NULL;
    mConfig.mLogType = JSON_LOG;
    mConfig.mDiscardNoneUtf8 = true;

    // run function
    ProcessorFilterNative& processor = *(new ProcessorFilterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, mConfig);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

    APSARA_TEST_EQUAL_DESC(processor.mFilters.size(), 2, "The filter size should be 2");
    APSARA_TEST_TRUE_DESC(processor.mFilters.find("123_proj_test") != processor.mFilters.end(),
                          "The 123_proj_test should be a key of the filter");
    APSARA_TEST_TRUE_DESC(processor.mFilters.find("456_proj_test_1") != processor.mFilters.end(),
                          "The 456_test_1 should be a key of the filter");
    LogFilterRule* filterRulePtr = processor.mFilters["123_proj_test"];
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys.size(), 2, "The filter keys size should be 2");
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs.size(), 2, "The filter regs size should be 2");
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys[0], "key1", "The filter key should be key1");
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterKeys[1], "key2", "The filter key should be key2");
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs[0], regex("value1"), "The filter reg should be value1");
    APSARA_TEST_EQUAL_DESC(filterRulePtr->FilterRegs[1], regex("value2"), "The filter reg should be value1");
    LOG_INFO(sLogger, ("TestInitFilter() end", time(NULL)));
}

void ProcessorFilterNativeUnittest::TestInitFilterBypass() {
    string config = "{\"filters\" : [{\"project_name\" : \"123_proj\", \"category\" : \"test\", \"keys\" : "
                    "[\"key1\", \"key2\"], \"regs\" : [\"value1\",\"value2\", \"value3\"]}]}";
    string path = GetProcessExecutionDir() + "user_log_config.json";
    ofstream file(path.c_str());
    file << config;
    file.close();

    Config mConfig;
    mConfig.mAdvancedConfig.mFilterExpressionRoot;
    mConfig.mFilterRule = NULL;
    mConfig.mLogType = JSON_LOG;
    mConfig.mDiscardNoneUtf8 = true;

    // run function
    ProcessorFilterNative& processor = *(new ProcessorFilterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, mConfig);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    APSARA_TEST_EQUAL_FATAL(ProcessorFilterNative::BYPASS_MODE, processor.mFilterMode);
}

// To test bool ProcessorFilterNative::Filter(LogEvent& sourceEvent)
void ProcessorFilterNativeUnittest::TestFilter() {
    string config = "{\"filters\" : [{\"project_name\" : \"project\", \"category\" : \"logstore\", \"keys\" : "
                    "[\"key1\", \"key2\"], \"regs\" : [\".*value1\",\"value2.*\"]}]}";
    string path = GetProcessExecutionDir() + "user_log_config.json";
    ofstream file(path.c_str());
    file << config;
    file.close();

    Config mConfig;
    mConfig.mAdvancedConfig.mFilterExpressionRoot = NULL;
    mConfig.mFilterRule = NULL;
    mConfig.mLogType = JSON_LOG;
    mConfig.mDiscardNoneUtf8 = true;

    // run function
    ProcessorFilterNative& processor = *(new ProcessorFilterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, mConfig);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

    // case 1 : the field are all provided,  only one matched
    auto sourceBuffer1 = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup1(sourceBuffer1);
    char illegalUtf8[] = {char(0xFF), char(0x80), '\0'};
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1xxxxx",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "key3)" + std::string(illegalUtf8) + R"(": "abcdea)" + std::string(illegalUtf8) + R"(value1",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup1.FromJsonString(inJson);
    std::cout<<inJson<<std::endl;
    // run function
    processorInstance.Process(eventGroup1);
    std::string outJson = eventGroup1.ToJsonString();
    // judge result
    std::string expectJson = R"({
        "events" : 
        [
            {
                "contents" : 
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "key3  " : "abcdea  value1",
                    "log.file.offset" : "0"
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
                    "key1" : "abcvalue1",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup2.FromJsonString(inJson);
    // run function
    processorInstance.Process(eventGroup2);
    outJson = eventGroup2.ToJsonString();
    // judge result
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());

    // case 3 : if the filter don't exist, it should return all the fields;
    auto sourceBuffer3 = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup3(sourceBuffer3);
    inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1xxxxx",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup3.FromJsonString(inJson);
    // run function
    expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1xxxxx",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    mContext.SetProjectName("project123123");
    processorInstance.Process(eventGroup3);
    outJson = eventGroup3.ToJsonString();
    // judge result
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());

    mContext.SetProjectName("project");
}

// To test bool ProcessorFilterNative::Filter(LogEvent& sourceEvent, const LogFilterRule* filterRule)
void ProcessorFilterNativeUnittest::TestLogFilterRule() {
    Config mConfig;
    Json::Value filterKeys;
    filterKeys.append(Json::Value("key1"));
    filterKeys.append(Json::Value("key2"));

    Json::Value filterRegs;
    filterRegs.append(Json::Value(".*value1"));
    filterRegs.append(Json::Value("value2.*"));

    mConfig.mFilterRule.reset(GetFilterFule(filterKeys, filterRegs));

    mConfig.mLogType = JSON_LOG;
    mConfig.mDiscardNoneUtf8 = true;


    // run function
    ProcessorFilterNative& processor = *(new ProcessorFilterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, mConfig);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

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
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup1.FromJsonString(inJson);
    // run function
    processorInstance.Process(eventGroup1);
    std::string outJson = eventGroup1.ToJsonString();
    // judge result
    std::string expectJson = R"({
        "events" : 
        [
            {
                "contents" : 
                {
                    "key1" : "abcdeavalue1",
                    "key2" : "value2xxxxx",
                    "log.file.offset" : "0"
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
                    "key1" : "abcvalue1",
                    "log.file.offset": "0"
                },
                "timestampNanosecond" : 0,
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup2.FromJsonString(inJson);
    // run function
    processorInstance.Process(eventGroup2);
    outJson = eventGroup2.ToJsonString();
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
        operands2["operator"] = "and";

        Json::Value operands3;
        operands3["key"] = "key2";
        operands3["exp"] = "value2.*";
        operands3["type"] = "regex";

        operands2["operands"].append(operands3);

        root["operands"].append(operands1);
        root["operands"].append(operands2);

        Config mConfig;
        mConfig.mLogType = JSON_LOG;
        mConfig.mDiscardNoneUtf8 = true;

        mConfig.mAdvancedConfig.mFilterExpressionRoot = UserLogConfigParser::ParseExpressionFromJSON(root);

        // run function
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(&processor, pluginId);
        ComponentConfig componentConfig(pluginId, mConfig);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

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
                        "key2" : "value2xxxxx",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "key1" : "abcdeavalue1",
                        "key2" : "value2xxxxx",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup1.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup1);
        std::string outJson = eventGroup1.ToJsonString();
        // judge result
        std::string expectJson = R"({
            "events" : 
            [
                {
                    "contents" : 
                    {
                        "key1" : "abcdeavalue1",
                        "key2" : "value2xxxxx",
                        "log.file.offset" : "0"
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
                        "key1" : "abcvalue1",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup2.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup2);
        outJson = eventGroup2.ToJsonString();
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
        Config mConfig;
        mConfig.mLogType = JSON_LOG;
        mConfig.mDiscardNoneUtf8 = true;
        mConfig.mAdvancedConfig.mFilterExpressionRoot = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
        std::string pluginId = "testID";
        ProcessorInstance processorInstance(&processor, pluginId);
        ComponentConfig componentConfig(pluginId, mConfig);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

        // (a and not d) and (b or c)
        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() != NULL);

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
                        "d" : "2008-08-08",
                        "log.file.offset": "0"
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
                        "d" : "1999-1-1",
                        "log.file.offset": "0"
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
                        "d" : "2222-22-22",
                        "log.file.offset": "0"
                    },
                    "timestampNanosecond" : 0,
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup1.FromJsonString(inJson);
        // run function
        processorInstance.Process(eventGroup1);
        std::string outJson = eventGroup1.ToJsonString();
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
                        "d" : "1999-1-1",
                        "log.file.offset" : "0"
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() != NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() != NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() != NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() == NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() == NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() == NULL);
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

        BaseFilterNodePtr root = UserLogConfigParser::ParseExpressionFromJSON(rootNode);
        APSARA_TEST_TRUE_FATAL(root.get() != NULL);
        APSARA_TEST_TRUE(root->GetNodeType() == OPERATOR_NODE);
    }
}

static const char UTF8_BYTE_PREFIX = 0x80;
static const char UTF8_BYTE_MASK = 0xc0;

void ProcessorFilterNativeUnittest::TestFilterNoneUtf8() {
    const int caseCharacterNum = 80; // ten charactor for every situation
    string characterSet[caseCharacterNum];
    bool isBlunk[caseCharacterNum][4]; // every charactor has 4 byte atmost

    // generate one byte utf8;

    for (int i = 0; i < 10; ++i) {
        char tmp;
        do {
            tmp = rand() & 0xff;
            tmp &= 0x7f;
        } while (tmp == 32 || tmp == 9); // tmp should not be space or \t

        characterSet[i] = string(1, tmp);
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
        characterSet[i] = string(1, tmp);
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

        characterSet[i] = string(1, tmp1) + string(1, tmp2);
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
        characterSet[index] = string(1, randArr1[index - 30]) + string(1, randArr2[index - 30]);
        isBlunk[index][0] = true;
        isBlunk[index][1] = false;
    }
    // five case of the situation thar only the format is utf8, but not unicode;

    for (int index = 35; index < 40; ++index) {
        randArr1[index - 30] &= 0xe1; // unicode must in rand [0x80,0x7fff]; ant two byte has 11 bits ,so the
                                      // situation can only be < 0x80
        characterSet[index] = string(1, randArr1[index - 30]) + string(1, randArr2[index - 30]);
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

        characterSet[i] = string(1, tmp1) + string(1, tmp2) + string(1, tmp3);
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
        characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
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
        characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
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
        characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = false;
        isBlunk[i][2] = false;
    }

    // the situation of only format in utf8;
    for (int i = 56; i < 60; ++i) {
        randArr1[i - 50] &= 0xf0;
        randArr2[i - 50] &= 0xdf; // 1110 0000  100xxxxx 10xxxxxx

        characterSet[i] = string(1, randArr1[i - 50]) + string(1, randArr2[i - 50]) + string(1, randArr3[i - 50]);
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

        characterSet[i] = string(1, tmp1) + string(1, tmp2) + string(1, tmp3) + string(1, tmp4);
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

        characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
            + string(1, randArr4[i - 70]);
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
        characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
            + string(1, randArr4[i - 70]);
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
        characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
            + string(1, randArr4[i - 70]);
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
        characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
            + string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = true;
        isBlunk[i][3] = true;
    }


    // greater than range
    for (int i = 78; i < 80; ++i) {
        randArr1[i - 70] |= 0x04;
        randArr2[i - 70] |= 0x10; // 1110 0000  100xxxxx 10xxxxxx

        characterSet[i] = string(1, randArr1[i - 70]) + string(1, randArr2[i - 70]) + string(1, randArr3[i - 70])
            + string(1, randArr4[i - 70]);
        isBlunk[i][0] = true;
        isBlunk[i][1] = true;
        isBlunk[i][2] = true;
        isBlunk[i][3] = true;
    }

    for (int i = 0; i < 10; ++i) {
        LOG_INFO(sLogger, ("################################round", i));
        string testStr;
        const int CHARACTER_COUNT = 8192;
        bool flow[CHARACTER_COUNT * 4];
        int index = 0; // index of flow
        // generate test string with character randomly, and record whether a position should be replaced by blunck
        for (int j = 0; j < CHARACTER_COUNT; ++j) {
            int randIndex = rand() % 80;
            LOG_INFO(sLogger, ("j", j)("randIndex", randIndex)("index", index));
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
        ProcessorFilterNative& processor = *(new ProcessorFilterNative);
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
