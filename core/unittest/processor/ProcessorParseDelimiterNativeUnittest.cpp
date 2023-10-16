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

#include "common/JsonUtil.h"
#include "config/Config.h"
#include "processor/ProcessorParseDelimiterNative.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"

namespace logtail {

class ProcessorParseDelimiterNativeUnittest : public ::testing::Test {
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
    void TestProcessQuote();
    void TestProcessKeyOverwritten();
    void TestUploadRawLog();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessWholeLine);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessQuote);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessKeyOverwritten);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestUploadRawLog);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestAddLog);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorParseDelimiterNativeUnittest, TestProcessEventDiscardUnmatch);

void ProcessorParseDelimiterNativeUnittest::TestInit() {
    Config config;
    config.mSeparator = ",";
    config.mQuote = '\'';
    config.mColumnKeys = {"time", "method", "url", "request_time"};
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = false;
    config.mAdvancedConfig.mRawLogTag = "__raw__";

    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
}

void ProcessorParseDelimiterNativeUnittest::TestProcessWholeLine() {
    // make config
    Config config;
    config.mSeparator = ",";
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
                    "content" : "2013-10-31 21:03:49,POST,PutData?Category=YunOsAccountOpLog,0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:04:49,POST,PutData?Category=YunOsAccountOpLog,0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "log.file.offset": "0",
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
                    "log.file.offset": "0",
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
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog,0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOs'AccountOpLog',0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "log.file.offset": "0",
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
                    "__raw_log__": "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog,0.024",
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog,0.024",
                    "log.file.offset": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw_log__": "2013-10-31 21:03:49,POST,'PutData?Category=YunOs'AccountOpLog',0.024",
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOs'AccountOpLog',0.024",
                    "log.file.offset": "0"
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

void ProcessorParseDelimiterNativeUnittest::TestProcessKeyOverwritten() {
    // make config
    Config config;
    config.mSeparator = ",";
    config.mQuote = '\'';
    config.mColumnKeys = {"time", "__raw__", "content", "__raw_log__"};
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = true;
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
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
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
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__raw__": "POST",
                    "__raw_log__": "0.024",
                    "content": "PutData?Category=YunOsAccountOpLog",
                    "log.file.offset": "0",
                    "time": "2013-10-31 21:03:49"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__raw__": "value1",
                    "__raw_log__": "value1",
                    "content" : "value1",
                    "log.file.offset": "0"
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
    Config config;
    config.mSeparator = ",";
    config.mQuote = '\'';
    config.mColumnKeys = {"time", "method", "url", "request_time"};
    config.mDiscardUnmatch = false;
    config.mUploadRawLog = true;
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
                    "content" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
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
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(componentConfig, mContext));
    processor.Process(eventGroup);
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__raw__" : "2013-10-31 21:03:49,POST,'PutData?Category=YunOsAccountOpLog',0.024",
                    "log.file.offset": "0",
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
                    "__raw__": "value1",
                    "__raw_log__": "value1",
                    "content" : "value1",
                    "log.file.offset": "0"
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
    Config config;
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
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
    APSARA_TEST_EQUAL_FATAL(strlen(key) + strlen(value) + 5, processor.GetContext().GetProcessProfile().logGroupSize);
}


void ProcessorParseDelimiterNativeUnittest::TestProcessEventKeepUnmatch() {
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
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
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
                    "content" : "value1",
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
                    "content" : "value1",
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
                    "content" : "value1",
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
                    "content" : "value1",
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
                    "content" : "value1",
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
    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length())*count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "__raw_log__value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length())*count, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(0, processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(count, processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseDelimiterNativeUnittest::TestProcessEventDiscardUnmatch() {
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
    ProcessorParseDelimiterNative& processor = *(new ProcessorParseDelimiterNative);
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
    APSARA_TEST_EQUAL_FATAL(count, processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length())*count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(0, processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0, processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(count, processor.mProcParseErrorTotal->GetValue());
}

} // namespace logtail

UNIT_TEST_MAIN