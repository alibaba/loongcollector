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
#include <string>
#include <vector>

#include "common/JsonUtil.h"
#include "common/TimeUtil.h"
#include "config/PipelineConfig.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "plugin/processor/ProcessorParseTimestampNative.h"
#include "unittest/Unittest.h"

using namespace logtail;

namespace logtail {

class ProcessorParseTimestampNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcessNoFormat();
    void TestProcessRegularFormat();
    void TestProcessNoYearFormat();
    void TestProcessRegularFormatFailed();
    void TestProcessHistoryDiscard();
    void TestProcessEventPreciseTimestampLegacy();
    void TestCheckTime();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessNoFormat);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessRegularFormat);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessNoYearFormat);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessRegularFormatFailed);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessHistoryDiscard);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestProcessEventPreciseTimestampLegacy);
UNIT_TEST_CASE(ProcessorParseTimestampNativeUnittest, TestCheckTime);

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"1"};
    return pluginMeta;
}

bool CheckTimeFormatV2(const std::string& timeValue, const std::string& timeFormat) {
    LogtailTime logTime = {0, 0};

    int nanosecondLength = -1;
    const char* strptimeResult = NULL;
    strptimeResult = Strptime(timeValue.c_str(), timeFormat.c_str(), &logTime, nanosecondLength, -1);
    if (NULL == strptimeResult) {
        return false;
    }
    return true;
}

bool CheckTimeFormatV1(const std::string& timeValue, const std::string& timeFormat) {
    struct tm tm;

    if (NULL == strptime(timeValue.c_str(), timeFormat.c_str(), &tm)) {
        return false;
    } else {
        return true;
    }
}

void ProcessorParseTimestampNativeUnittest::TestCheckTime() {
    std::string timeValue;
    std::string timeFormat;
    timeValue = "Fri";
    timeFormat = "%a";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "Friday";
    timeFormat = "%A";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "Jan";
    timeFormat = "%b";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "January";
    timeFormat = "%B";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "19";
    timeFormat = "%d";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "Jan";
    timeFormat = "%h";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "22";
    timeFormat = "%H";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "01";
    timeFormat = "%I";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "08";
    timeFormat = "%m";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "01";
    timeFormat = "%M";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "\n";
    timeFormat = "%n";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "AM";
    timeFormat = "%p";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "11:59:59 AM";
    timeFormat = "%r";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "23:59";
    timeFormat = "%R";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "59";
    timeFormat = "%S";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = " ";
    timeFormat = "%t";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "98";
    timeFormat = "%y";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "2004";
    timeFormat = "%Y";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "20";
    timeFormat = "%C";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "31";
    timeFormat = "%e";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "365";
    timeFormat = "%j";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "2";
    timeFormat = "%u";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "53";
    timeFormat = "%U";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "24";
    timeFormat = "%V";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "5";
    timeFormat = "%w";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "23";
    timeFormat = "%W";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeValue = "Tue Nov 20 14:12:58 2020";
    timeFormat = "%c";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeFormat = "%x";
    timeValue = "10/26/23";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeFormat = "%X";
    timeValue = "14:12:58";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeFormat = "%s";
    timeValue = "1605853978";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%f";
    timeValue = "123456789";
    APSARA_TEST_TRUE_FATAL(!CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%Y-%m-%d %H:%M:%S.%f";
    timeValue = "2021-11-25 14:16:46.123456789";
    APSARA_TEST_TRUE_FATAL(!CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%Y-%m-%d %H:%M:%S";
    timeValue = "2020-11-20 14:12:58";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "[%Y-%m-%d %H:%M:%S";
    timeValue = "[2017-12-11 15:05:07.012]";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%d %b %y %H:%M";
    timeValue = "02 Jan 06 15:04 MST";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%d %b %y %H:%M";
    timeValue = "02 Jan 06 15:04 -0700";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%A, %d-%b-%y %H:%M:%S";
    timeValue = "Monday, 02-Jan-06 15:04:05 MST";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%A, %d %b %Y %H:%M:%S";
    timeValue = "Mon, 02 Jan 2006 15:04:05 MST";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%Y-%m-%dT%H:%M:%S";
    timeValue = "2006-01-02T15:04:05Z07:00";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%Y-%m-%dT%H:%M:%S";
    timeValue = "2006-01-02T15:04:05.999999999Z07:00";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%s";
    timeValue = "1637843406";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%s";
    timeValue = "1637843406123";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%D";
    timeValue = "11/20/20";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeFormat = "%F";
    timeValue = "2020-11-20";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
    timeFormat = "%T";
    timeValue = "14:12:58";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%z";
    timeValue = "+0800";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%Z";
    timeValue = "CST";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));

    timeFormat = "%%";
    timeValue = "%";
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV1(timeValue, timeFormat));
    APSARA_TEST_TRUE_FATAL(CheckTimeFormatV2(timeValue, timeFormat));
}

void ProcessorParseTimestampNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
    config["SourceTimezone"] = "GMT+08:00";
    config["SourceYear"] = 0;

    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
}

void ProcessorParseTimestampNativeUnittest::TestProcessNoFormat() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "";
    config["SourceTimezone"] = "GMT+08:00";
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(!processorInstance.Init(config, mContext));
}

void ProcessorParseTimestampNativeUnittest::TestProcessRegularFormat() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
    config["SourceTimezone"] = "GMT+08:00";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t now = time(nullptr); // will not be discarded by history timeout
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), config["SourceFormat"].asString().c_str(), now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);
    
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    std::stringstream expectJsonSs;
    expectJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
                 << timebuff << R"("
                },
                "timestamp" : )"
                 << now - processor.mLogTimeZoneOffsetSecond << R"(,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
                 << timebuff << R"("
                },
                "timestamp" : )"
                 << now - processor.mLogTimeZoneOffsetSecond << R"(,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    APSARA_TEST_EQUAL_FATAL(CompactJson(expectJsonSs.str()), CompactJson(outJson));
    // check observablity
    APSARA_TEST_EQUAL_FATAL(0, processor.GetContext().GetProcessProfile().historyFailures);
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mInEventsTotal->GetValue());
    // discard history, so output is 0
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mOutEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseTimestampNativeUnittest::TestProcessNoYearFormat() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%m-%d %H:%M:%S.%f";
    config["SourceTimezone"] = "GMT+08:00";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t now = time(nullptr); // will not be discarded by history timeout
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    config["SourceYear"] = now_tm->tm_year + 1900;
    strftime(timebuff, sizeof(timebuff), "%m-%d %H:%M:%S.999999999", now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    std::stringstream expectJsonSs;
    expectJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
                 << timebuff << R"("
                },
                "timestamp" : )"
                 << now - processor.mLogTimeZoneOffsetSecond << R"(,
                "timestampNanosecond" : 999999999,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
                 << timebuff << R"("
                },
                "timestamp" : )"
                 << now - processor.mLogTimeZoneOffsetSecond << R"(,
                "timestampNanosecond" : 999999999,
                "type" : 1
            }
        ]
    })";
    APSARA_TEST_EQUAL_FATAL(CompactJson(expectJsonSs.str()), CompactJson(outJson));
    // check observablity
    APSARA_TEST_EQUAL_FATAL(0, processor.GetContext().GetProcessProfile().historyFailures);
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mInEventsTotal->GetValue());
    // discard history, so output is 0
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mOutEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseTimestampNativeUnittest::TestProcessRegularFormatFailed() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
    config["SourceTimezone"] = "GMT+08:00";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t now
        = time(nullptr) - INT32_FLAG(ilogtail_discard_interval) - 1; // will parse fail so timeout should not matter
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), "%Y-%m-%d", now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string inJson = inJsonSs.str();
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);
    
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    APSARA_TEST_EQUAL_FATAL(0, processor.GetContext().GetProcessProfile().historyFailures);
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mInEventsTotal->GetValue());
    // discard history, so output is 0
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mOutEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(2UL, processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseTimestampNativeUnittest::TestProcessHistoryDiscard() {
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
    config["SourceTimezone"] = "GMT+08:00";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t now = time(nullptr) - INT32_FLAG(ilogtail_discard_interval) - 1; // will be discarded by history timeout
    char timebuff[32] = "";
    std::tm* now_tm = std::localtime(&now);
    strftime(timebuff, sizeof(timebuff), config["SourceFormat"].asString().c_str(), now_tm);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "time" : ")"
             << timebuff << R"("
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);
    
    // check observablity
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_EQUAL_FATAL(2, processor.GetContext().GetProcessProfile().historyFailures);
    APSARA_TEST_EQUAL_FATAL(2UL, processorInstance.mInEventsTotal->GetValue());
    // discard history, so output is 0
    APSARA_TEST_EQUAL_FATAL(0UL, processorInstance.mOutEventsTotal->GetValue());
    // event group size is not 0
    APSARA_TEST_NOT_EQUAL_FATAL(0UL, processorInstance.mOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(2UL, processor.mDiscardedEventsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(0UL, processor.mOutFailedEventsTotal->GetValue());
}

void ProcessorParseTimestampNativeUnittest::TestProcessEventPreciseTimestampLegacy() {
    // make config
    BOOL_FLAG(ilogtail_discard_old_data) = false;
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceFormat"] = "%Y-%m-%d %H:%M:%S.%f";
    config["SourceTimezone"] = "GMT+00:00";
    // make events
    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto logEvent = PipelineEventPtr(eventGroup.CreateLogEvent(), false, nullptr);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "contents" :
        {
            "time" : "2017-1-11 15:05:07.012"
        },
        "timestamp" : 12345678901,
        "type" : 1
    })";
    logEvent->FromJsonString(inJsonSs.str());
    // run function
    ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    logtail::StringView timeStrCache;
    LogtailTime logTime = {0, 0};
    APSARA_TEST_TRUE_FATAL(processor.ProcessEvent("/var/log/message", logEvent, logTime, timeStrCache));
    // judge result
    std::string outJson = logEvent->ToJsonString();
    std::stringstream expectJsonSs;
    expectJsonSs << R"({
        "contents" :
        {
            "time" : "2017-1-11 15:05:07.012"
        },
        "timestamp" : )"
                 << 1484147107 - processor.mLogTimeZoneOffsetSecond << R"(,
        "timestampNanosecond": 12000000,
        "type" : 1
    })";
    APSARA_TEST_EQUAL_FATAL(CompactJson(expectJsonSs.str()), CompactJson(outJson));
}

class ProcessorParseLogTimeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestParseLogTime();
    void TestParseLogTimeSecondCache();
    void TestAdjustTimeZone();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseLogTimeUnittest, TestParseLogTime);
UNIT_TEST_CASE(ProcessorParseLogTimeUnittest, TestParseLogTimeSecondCache);
// UNIT_TEST_CASE(ProcessorParseLogTimeUnittest, TestAdjustTimeZone);

void ProcessorParseLogTimeUnittest::TestParseLogTime() {
    struct Case {
        std::string inputTimeStr;
        std::string fmtStr;
        time_t exceptedLogTime;
        long exceptedLogTimeNanosecond;
    };

    std::vector<Case> inputTimes = {
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S.%f", 1484147107, 12000000},
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S.%f", 1484147107, 12000000},
        {"[2017-1-11 15:05:07.0123]", "[%Y-%m-%d %H:%M:%S.%f", 1484147107, 12300000},
        {"11 Jan 17 15:05 MST", "%d %b %y %H:%M", 1484147100, 0},
        {"11 Jan 17 15:05 -0700", "%d %b %y %H:%M", 1484147100, 0},
        {"Tuesday, 11-Jan-17 15:05:07.0123 MST", "%A, %d-%b-%y %H:%M:%S.%f", 1484147107, 12300000},
        {"Tuesday, 11 Jan 2017 15:05:07 MST", "%A, %d %b %Y %H:%M:%S", 1484147107, 0},
        {"2017-01-11T15:05:07Z08:00", "%Y-%m-%dT%H:%M:%S", 1484147107, 0},
        {"2017-01-11T15:05:07.012999999Z07:00", "%Y-%m-%dT%H:%M:%S.%f", 1484147107, 12999999},
        {"1484147107", "%s", 1484147107, 0},
        {"1484147107123", "%s", 1484147107, 123000000},
        {"15:05:07.012 2017-1-11", "%H:%M:%S.%f %Y-%m-%d", 1484147107, 12000000},
        {"2017-1-11 15:05:07.012 +0700 (UTC)", "%Y-%m-%d %H:%M:%S.%f %z (%Z)", 1484147107, 12000000},
        // Compatibility Test
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S", 1484147107, 0},
    };
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceTimezone"] = "GMT+00:00";
    for (size_t i = 0; i < inputTimes.size(); ++i) {
        auto& c = inputTimes[i];
        config["SourceFormat"] = c.fmtStr;
        config["SourceFormat"] = c.fmtStr;
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        StringView timeStrCache;
        bool ret = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
        EXPECT_EQ(ret, true) << "failed: " + c.inputTimeStr;
        EXPECT_EQ(outTime.tv_sec, c.exceptedLogTime) << "failed: " + c.inputTimeStr;
        EXPECT_EQ(outTime.tv_nsec, c.exceptedLogTimeNanosecond) << "failed: " + c.inputTimeStr;
    }
}

void ProcessorParseLogTimeUnittest::TestParseLogTimeSecondCache() {
    struct Case {
        std::string inputTimeStr;
        time_t exceptedLogTime;
        long exceptedLogTimeNanosecond;

        Case(std::string _inputTimeStr,
             time_t _exceptedLogTime,
             long _exceptedLogTimeNanosecond,
             uint64_t _exceptedPreciseTimestamp)
            : inputTimeStr(_inputTimeStr),
              exceptedLogTime(_exceptedLogTime),
              exceptedLogTimeNanosecond(_exceptedLogTimeNanosecond) {}
    };
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    config["SourceTimezone"] = "GMT+00:00";
    { // case: second
        config["SourceFormat"] = "%Y-%m-%d %H:%M:%S";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(
                    std::string(second.data()), expectLogTimeBase + i, 0, expectLogTimeNanosecondBase + i * 1000000);
            }
        }

        StringView timeStrCache = "2012-01-01 15:04:59";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
    { // case: nanosecond
        config["SourceFormat"] = "%Y-%m-%d %H:%M:%S.%f";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(second + "." + std::to_string(j),
                                        expectLogTimeBase + i,
                                        j * 100000000,
                                        expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
        StringView timeStrCache = "2012-01-01 15:04:59";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
    { // case: timestamp second
        config["SourceFormat"] = "%s";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1484147107;
        long expectLogTimeNanosecondBase = 1484147107000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = std::to_string(expectLogTimeBase + i);
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(
                    std::string(second.data()), expectLogTimeBase + i, 0, expectLogTimeNanosecondBase + i * 1000000);
            }
        }
        StringView timeStrCache = "1484147106";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
    { // case: timestamp nanosecond
        config["SourceFormat"] = "%s";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1484147107;
        long expectLogTimeNanosecondBase = 1484147107000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = std::to_string(expectLogTimeBase + i);
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(second + std::to_string(j),
                                        expectLogTimeBase + i,
                                        j * 100000000,
                                        expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
        StringView timeStrCache = "1484147106";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
    { // case: nanosecond in the middle
        config["SourceFormat"] = "%H:%M:%S.%f %Y-%m-%d";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "15:05:" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(second + "." + std::to_string(j) + " 2012-01-01",
                                        expectLogTimeBase + i,
                                        j * 100000000,
                                        expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
        StringView timeStrCache = "15:04:59.0 2012-01-01";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
}

void ProcessorParseLogTimeUnittest::TestAdjustTimeZone() {
    struct Case {
        std::string inputTimeStr;
        time_t exceptedLogTime;
        long exceptedLogTimeNanosecond;

        Case(std::string _inputTimeStr,
             time_t _exceptedLogTime,
             long _exceptedLogTimeNanosecond,
             uint64_t _exceptedPreciseTimestamp)
            : inputTimeStr(_inputTimeStr),
              exceptedLogTime(_exceptedLogTime),
              exceptedLogTimeNanosecond(_exceptedLogTimeNanosecond) {}
    };
    // make config
    Json::Value config;
    config["SourceKey"] = "time";
    { // case: UTC
        config["SourceTimezone"] = "GMT+00:00";
        config["SourceFormat"] = "%Y-%m-%d %H:%M:%S.%f";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(second + "." + std::to_string(j),
                                        expectLogTimeBase + i,
                                        j * 100000000,
                                        expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
        StringView timeStrCache = "2012-01-01 15:04:59";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
    { // case: +7
        config["SourceTimezone"] = "GMT+07:00";
        config["SourceFormat"] = "%Y-%m-%d %H:%M:%S.%f";
        // run function
        ProcessorParseTimestampNative& processor = *(new ProcessorParseTimestampNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

        time_t expectLogTimeBase = 1325405100;
        long expectLogTimeNanosecondBase = 1325405100000000;
        LogtailTime outTime = {0, 0};
        uint64_t preciseTimestamp = 0;
        std::vector<Case> inputTimes;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                inputTimes.emplace_back(second + "." + std::to_string(j),
                                        expectLogTimeBase + i,
                                        j * 100000000,
                                        expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
        StringView timeStrCache = "2012-01-01 15:04:59";
        for (size_t i = 0; i < inputTimes.size(); ++i) {
            auto c = inputTimes[i];
            bool ret
                = processor.ParseLogTime(c.inputTimeStr, "/var/log/message", outTime, preciseTimestamp, timeStrCache);
            APSARA_TEST_EQUAL(ret, true);
            APSARA_TEST_EQUAL(outTime.tv_sec, c.exceptedLogTime);
            APSARA_TEST_EQUAL(outTime.tv_nsec, c.exceptedLogTimeNanosecond);
        }
    }
}

} // namespace logtail

UNIT_TEST_MAIN