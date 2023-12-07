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
#include "processor/ProcessorParseApsaraNative.h"
#include "processor/ProcessorSplitLogStringNative.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseApsaraNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
        BOOL_FLAG(ilogtail_discard_old_data) = false;
    }

    void TestInit();
    void TestProcessWholeLine();
    void TestProcessWholeLinePart();
    void TestProcessKeyOverwritten();
    void TestUploadRawLog();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestMultipleLines();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLine);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLinePart);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessKeyOverwritten);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestUploadRawLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestAddLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventDiscardUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestMultipleLines);

void ProcessorParseApsaraNativeUnittest::TestMultipleLines() {
    // 第一个contents 测试多行下的解析，第二个contents测试多行下time的解析
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:33.2]\t[INFO]\t[2]\t/ilogtail/AppConfigBase.cpp:2\t\tAppConfigBase AppConfigBase:2
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15
:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";


    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "1",
                    "AppConfigBase AppConfigBase": "1",
                    "__LEVEL__": "ERROR",
                    "__THREAD__": "1",
                    "log.file.offset": "0",
                    "microtime": "1693833350100000"
                },
                "timestamp": 1693833350,
                "timestampNanosecond": 100000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "2",
                    "AppConfigBase AppConfigBase": "2",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "2",
                    "log.file.offset": "0",
                    "microtime": "1693833333200000"
                },
                "timestamp": 1693833333,
                "timestampNanosecond": 200000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "log.file.offset": "0",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
                "type": 1
            },
            {
                "contents": {
                    "__raw_log__": "[2023-09-04 13:15",
                    "content": "[2023-09-04 13:15",
                    "log.file.offset": "0"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "__raw_log__": ":50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1",
                    "content": ":50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1",
                    "log.file.offset": "0"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "log.file.offset": "0",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
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
        Config config;
        config.mDiscardUnmatch = false;
        config.mUploadRawLog = false;
        config.mAdvancedConfig.mRawLogTag = "__raw__";
        config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();

        std::string pluginId = "testID";
        ComponentConfig componentConfig(pluginId, config);

        // run function ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processorSplitLogStringNative;
        processorSplitLogStringNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(componentConfig));
        processorSplitLogStringNative.Process(eventGroup);
        std::string outJson = eventGroup.ToJsonString();
        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        processor.Process(eventGroup);

        // judge result
        outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // ProcessorSplitRegexNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Config config;
        config.mDiscardUnmatch = false;
        config.mUploadRawLog = false;
        config.mAdvancedConfig.mRawLogTag = "__raw__";
        config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();
        config.mLogBeginReg = ".*";

        std::string pluginId = "testID";
        ComponentConfig componentConfig(pluginId, config);
        // run function processorSplitRegexNative
        ProcessorSplitRegexNative processorSplitRegexNative;
        processorSplitRegexNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitRegexNative.Init(componentConfig));
        processorSplitRegexNative.Process(eventGroup);
        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
        processor.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        std::string outJson2 = CompactJson(outJson);
        
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseApsaraNativeUnittest::TestInit() {
    Config config;
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mLogTimeZoneOffsetSecond = 0;

    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLine() {
    // make config
    Config config;
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1693833364862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "log.file.offset": "0",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "log.file.offset": "0",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "log.file.offset": "0",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLinePart() {
    // make config
    Config config;
    config.mDiscardUnmatch = true;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:0]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:0[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1234560\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 3;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessKeyOverwritten() {
    // make config
    Config config;
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = true;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\tcontent:100\t\t__raw__:success\t\t__raw_log__:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__raw__": "success",
                    "__raw_log__": "success",
                    "content": "100",
                    "log.file.offset": "0",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": "value1",
                    "__raw_log__": "value1",
                    "content": "value1",
                    "log.file.offset": "0"
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
}

void ProcessorParseApsaraNativeUnittest::TestUploadRawLog() {
    // make config
    Config config;
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = true;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    config.mLogTimeZoneOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__raw__": "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success",
                    "log.file.offset": "0",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": "value1",
                    "__raw_log__": "value1",
                    "content": "value1",
                    "log.file.offset": "0"
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
}

void ProcessorParseApsaraNativeUnittest::TestAddLog() {
    Config config;
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    APSARA_TEST_EQUAL_FATAL(int(strlen(key) + strlen(value) + 5), processor.GetContext().GetProcessProfile().logGroupSize);
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Config config;
    config.mSeparator = ",";
    config.mQuote = '\'';
    config.mColumnKeys = {"time", "method", "url", "request_time"};
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__raw_log__" : "value1",
                    "content": "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__" : "value1",
                    "content": "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__" : "value1",
                    "content": "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__" : "value1",
                    "content": "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__" : "value1",
                    "content": "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "__raw_log__value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Config config;
    config.mSeparator = ",";
    config.mQuote = '\'';
    config.mColumnKeys = {"time", "method", "url", "request_time"};
    config.mDiscardUnmatch = true;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processorInstance.Process(eventGroup);
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

} // namespace logtail

UNIT_TEST_MAIN