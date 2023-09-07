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
#include "processor/ProcessorParseTimestampNative.h"

namespace logtail {

class ProcessorParseTimestampNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
    }

    void TestInit();
    void TestProcessNoFormat();
    void TestProcessEventRegularFormat();
    void TestProcessEventRegularFormatFailed();
    void TestProcessEventHistoryDiscard();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessNoFormat);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessEventRegularFormat);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessEventRegularFormatFailed);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessEventHistoryDiscard);

void ProcessorParseTimestampNativeUnittest::TestInit() {
    Config config;
    config.mTimeFormat = "%Y-%m-%d %H:%M:%S";
    config.mTimeKey = "time";
    config.mAdvancedConfig.mSpecifiedYear = 0;
    config.mAdvancedConfig.mEnablePreciseTimestamp = false;
    config.mAdvancedConfig.mPreciseTimestampKey = "precise_timestamp";
    config.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::MILLISECOND;
    config.mLogTimeZoneOffsetSecond = 28800;

    ProcessorParseTimestampNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processor.Init(componentConfig));
}

void ProcessorParseTimestampNativeUnittest::TestProcessNoFormat() {
    // make config
    Config config;
    config.mTimeFormat = "";
    config.mTimeKey = "time";
    config.mLogTimeZoneOffsetSecond = 28800;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "time" : "2023-07-30 23:00:00"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseTimestampNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processor.Init(componentConfig));
    processor.Process(eventGroup);
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseTimestampNativeUnittest::TestProcessEventRegularFormat() {
    // make config
    Config config;
    config.mTimeFormat = "%Y-%m-%d %H:%M:%S";
    config.mTimeKey = "time";
    config.mLogTimeZoneOffsetSecond = 28800;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto logEvent = PipelineEventPtr(LogEvent::CreateEvent(sourceBuffer));
    time_t now = time(nullptr); // will not be discarded by history timeout
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), config.mTimeFormat.c_str(), now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "contents" :
        {
            "time" : ")"
             << timebuff << R"("
        },
        "timestamp" : 12345678901,
        "type" : 1
    })";
    logEvent->FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processor.Init(componentConfig));
    logtail::StringView timeStrCache;
    APSARA_TEST_TRUE_FATAL(processor.ProcessEvent("/var/log/message", logEvent, timeStrCache));
    // judge result
    std::string outJson = logEvent->ToJsonString();
    std::stringstream expectJsonSs;
    expectJsonSs << R"({
        "contents" :
        {
            "time" : ")"
                 << timebuff << R"("
        },
        "timestamp" : )"
                 << now - config.mLogTimeZoneOffsetSecond << R"(,
        "type" : 1
    })"; // TODO: adjust time zone
    APSARA_TEST_EQUAL_FATAL(CompactJson(expectJsonSs.str()), CompactJson(outJson));
}

void ProcessorParseTimestampNativeUnittest::TestProcessEventRegularFormatFailed() {
    // make config
    Config config;
    config.mTimeFormat = "%Y-%m-%d %H:%M:%S";
    config.mTimeKey = "time";
    config.mLogTimeZoneOffsetSecond = 28800;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto logEvent = PipelineEventPtr(LogEvent::CreateEvent(sourceBuffer));
    time_t now
        = time(nullptr) - INT32_FLAG(ilogtail_discard_interval) - 1; // will parse fail so timeout should not matter
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), "%Y-%m-%d", now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "contents" :
        {
            "time" : ")"
             << timebuff << R"("
        },
        "timestamp" : 12345678901,
        "type" : 1
    })";
    std::string inJson = inJsonSs.str();
    logEvent->FromJsonString(inJson);
    // run function
    ProcessorParseTimestampNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processor.Init(componentConfig));
    logtail::StringView timeStrCache;
    APSARA_TEST_TRUE_FATAL(processor.ProcessEvent("/var/log/message", logEvent, timeStrCache));
    // judge result
    std::string outJson = logEvent->ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseTimestampNativeUnittest::TestProcessEventHistoryDiscard() {
    // make config
    Config config;
    config.mTimeFormat = "%Y-%m-%d %H:%M:%S";
    config.mTimeKey = "time";
    config.mLogTimeZoneOffsetSecond = 28800;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto logEvent = PipelineEventPtr(LogEvent::CreateEvent(sourceBuffer));
    time_t now = time(nullptr) - INT32_FLAG(ilogtail_discard_interval) - 1; // will be discarded by history timeout
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), config.mTimeFormat.c_str(), now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "contents" :
        {
            "time" : ")"
             << timebuff << R"("
        },
        "timestamp" : 12345678901,
        "type" : 1
    })";
    logEvent->FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ComponentConfig componentConfig(pluginId, config);
    APSARA_TEST_TRUE_FATAL(processor.Init(componentConfig));
    logtail::StringView timeStrCache;
    APSARA_TEST_FALSE_FATAL(processor.ProcessEvent("/var/log/message", logEvent, timeStrCache));
    // check observablity
    APSARA_TEST_EQUAL_FATAL(1, processor.GetContext().GetProcessProfile().historyFailures);
}


} // namespace logtail

UNIT_TEST_MAIN