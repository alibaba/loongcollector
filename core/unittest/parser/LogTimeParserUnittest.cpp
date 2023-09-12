// Copyright 2022 iLogtail Authors
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
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "common/StringTools.h"
#include "parser/LogParser.h"
#include "reader/LogFileReader.h"
#include <iostream>
#include "common/util.h"
#include "common/TimeUtil.h"
#include <ctime>
#include <cstdlib>
#include "config_manager/ConfigManager.h"
#include <chrono>
#include <cmath>

using namespace std;
using namespace sls_logs;

namespace logtail {

Logger::logger gLogger;

class LogParserUnittest : public ::testing::Test {
public:
public:
    void TestParseLogTime();
    void TestParseLogTimeSecondCache();
    void TestParseLogTimeTimeZone();

    static void SetUpTestCase() // void Setup()
    {
        // mock data is all timeout, so shut up discard_old flag
        BOOL_FLAG(ilogtail_discard_old_data) = false;
        LOG_INFO(gLogger, ("LogParserUnittest", "setup"));
    }
    static void TearDownTestCase() // void CleanUp()
    {
        LOG_INFO(gLogger, ("LogParserUnittest", "cleanup"));
    }
};

APSARA_UNIT_TEST_CASE(LogParserUnittest, TestParseLogTime, 0);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestParseLogTimeSecondCache, 1);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestParseLogTimeTimeZone, 2);

void LogParserUnittest::TestParseLogTime() {
    struct Case {
        std::string inputTimeStr;
        std::string fmtStr;
        time_t exceptedLogTime;
        long exceptedLogTimeNanosecond;
        uint64_t exceptedPreciseTimestamp;
    };

    vector<Case> inputTimes = {
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S.%f", 1484147107, 12000000, 1484147107012},
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S.%f", 1484147107, 12000000,1484147107012},
        {"[2017-1-11 15:05:07.0123]", "[%Y-%m-%d %H:%M:%S.%f", 1484147107, 12300000,1484147107012},
        {"11 Jan 17 15:05 MST", "%d %b %y %H:%M", 1484147100, 0, 1484147100000},
        {"11 Jan 17 15:05 -0700", "%d %b %y %H:%M", 1484147100, 0, 1484147100000},
        {"Tuesday, 11-Jan-17 15:05:07.0123 MST", "%A, %d-%b-%y %H:%M:%S.%f", 1484147107, 12300000, 1484147107012},
        {"Tuesday, 11 Jan 2017 15:05:07 MST", "%A, %d %b %Y %H:%M:%S", 1484147107, 0, 1484147107000},
        {"2017-01-11T15:05:07Z08:00", "%Y-%m-%dT%H:%M:%S", 1484147107, 0, 1484147107000},
        {"2017-01-11T15:05:07.012999999Z07:00", "%Y-%m-%dT%H:%M:%S.%f", 1484147107, 12999999, 1484147107012},
        {"1484147107", "%s", 1484147107, 0, 1484147107000},
        {"1484147107123", "%s", 1484147107, 123000000, 1484147107123},
        {"15:05:07.012 2017-1-11", "%H:%M:%S.%f %Y-%m-%d", 1484147107, 12000000, 1484147107012},
        {"2017-1-11 15:05:07.012 +0700 (UTC)", "%Y-%m-%d %H:%M:%S.%f %z (%Z)", 1484147107, 12000000, 1484147107012},
        // Compatibility Test
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S", 1484147107, 0, 1484147107012},
    };

    int32_t tzOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    for (size_t i = 0; i < inputTimes.size(); ++i) {
        auto& c = inputTimes[i];
        std::string outTimeStr;
        LogtailTime outTime;
        ParseLogError error;
        uint64_t preciseTimestamp = 0;
        PreciseTimestampConfig preciseTimestampConfig;
        preciseTimestampConfig.enabled = true;
        preciseTimestampConfig.key = "key";
        preciseTimestampConfig.unit = TimeStampUnit::MILLISECOND;
        bool ret = LogParser::ParseLogTime("TestData",
                                           outTimeStr,
                                           outTime,
                                           preciseTimestamp,
                                           c.inputTimeStr,
                                           c.fmtStr.c_str(),
                                           preciseTimestampConfig,
                                           -1,
                                           "",
                                           "",
                                           "",
                                           "",
                                           error,
                                           c.fmtStr != "%s" ? tzOffsetSecond : 0);
        LOG_INFO(sLogger,
                 ("Case", i)("InputTimeStr", c.inputTimeStr)("FormatStr", c.fmtStr)("ret", ret)(outTimeStr, outTime.tv_sec)(
                     "preciseTimestamp", preciseTimestamp)("error", error));
        EXPECT_EQ(ret, true) << "failed: " + c.inputTimeStr;
        EXPECT_EQ(outTime.tv_sec, c.exceptedLogTime) << "failed: " + c.inputTimeStr;
        EXPECT_EQ(outTime.tv_nsec, c.exceptedLogTimeNanosecond) << "failed: " + c.inputTimeStr;
        EXPECT_EQ(preciseTimestamp, c.exceptedPreciseTimestamp) << "failed: " + c.inputTimeStr;
    }
}

void LogParserUnittest::TestParseLogTimeSecondCache() {
    int32_t tzOffsetSecond = -GetLocalTimeZoneOffsetSecond();
    LogtailTime outTime;
    ParseLogError error;
    uint64_t preciseTimestamp = 0;
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.key = "key";
    preciseTimestampConfig.unit = TimeStampUnit::MICROSECOND;
    { // case: second
        std::string preTimeStr = "2012-01-01 15:04:59";
        std::string timeFormat = "%Y-%m-%d %H:%M:%S";
        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + to_string(i) : to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = std::string(second.data());
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                tzOffsetSecond);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000);
            }
        }
    }
    { // case: nanosecond
        std::string preTimeStr = "2012-01-01 15:04:59";
        std::string timeFormat = "%Y-%m-%d %H:%M:%S.%f";
        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + to_string(i) : to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = second + "." + to_string(j);
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                tzOffsetSecond);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
    }
    { // case: timestamp second
        std::string preTimeStr = "1484147106";
        std::string timeFormat = "%s";
        time_t expectLogTimeBase = 1484147107;
        long expectLogTimeNanosecondBase = 1484147107000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = to_string(expectLogTimeBase + i);
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = second;
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                0);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000);
            }
        }
    }
    { // case: timestamp nanosecond
        std::string preTimeStr = "1484147107123";
        std::string timeFormat = "%s";
        time_t expectLogTimeBase = 1484147107;
        long expectLogTimeNanosecondBase = 1484147107000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = to_string(expectLogTimeBase + i);
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = second + to_string(j);
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                0);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
    }
    { // case: nanosecond in the middle
        std::string preTimeStr = "15:04:59.012 2012-01-01";
        std::string timeFormat = "%H:%M:%S.%f %Y-%m-%d";
        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "15:05:" + (i < 10 ? "0" + to_string(i) : to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = second + "." + to_string(j) + " 2012-01-01";
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                tzOffsetSecond);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000 + j * 100000);
            }
        }
    }
}

void LogParserUnittest::TestParseLogTimeTimeZone() {
    LogtailTime outTime;
    ParseLogError error;
    uint64_t preciseTimestamp = 0;
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.key = "key";
    preciseTimestampConfig.unit = TimeStampUnit::MICROSECOND;
    { // case: UTC
        int32_t tzOffsetSecond = -GetLocalTimeZoneOffsetSecond();
        std::string preTimeStr = "2012-01-01 15:04:59";
        std::string timeFormat = "%Y-%m-%d %H:%M:%S";
        time_t expectLogTimeBase = 1325430300;
        long expectLogTimeNanosecondBase = 1325430300000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + to_string(i) : to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = std::string(second.data());
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                tzOffsetSecond);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000);
            }
        }
    }
    { // case: +7
        int32_t tzOffsetSecond = 7 * 3600 - GetLocalTimeZoneOffsetSecond();
        std::string preTimeStr = "2012-01-01 15:04:59";
        std::string timeFormat = "%Y-%m-%d %H:%M:%S";
        time_t expectLogTimeBase = 1325405100;
        long expectLogTimeNanosecondBase = 1325405100000000;
        for (size_t i = 0; i < 5; ++i) {
            std::string second = "2012-01-01 15:05:" + (i < 10 ? "0" + to_string(i) : to_string(i));
            for (size_t j = 0; j < 5; ++j) {
                std::string inputTimeStr = std::string(second.data());
                bool ret = LogParser::ParseLogTime("TestData",
                                                preTimeStr,
                                                outTime,
                                                preciseTimestamp,
                                                inputTimeStr,
                                                timeFormat.c_str(),
                                                preciseTimestampConfig,
                                                -1,
                                                "",
                                                "",
                                                "",
                                                "",
                                                error,
                                                tzOffsetSecond);
                APSARA_TEST_EQUAL(ret, true);
                APSARA_TEST_EQUAL(outTime.tv_sec, expectLogTimeBase + i);
                APSARA_TEST_EQUAL(preciseTimestamp, expectLogTimeNanosecondBase + i * 1000000);
            }
        }
    }
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    logtail::gLogger = logtail::Logger::Instance().GetLogger("/logger/global/test");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}