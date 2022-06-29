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

using namespace std;
using namespace sls_logs;

DECLARE_FLAG_BOOL(ilogtail_discard_old_data);
DECLARE_FLAG_INT32(gmt_align_deviation);

namespace logtail {

Logger::logger gLogger;

const char* logLine[] = {
    "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]", //1
    "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]\t", //2
    "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1754]\tsomestring", //3
    "[2013-03-13 "
    "18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1755]\tRealRecycle#Command:rm "
    "-rf /apsara/tubo/.fuxi_tubo_trash/*", //4
    "[2013-03-13 "
    "18:14:57.365716]\t[ERROR]\t[12835]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\tParseWhiteListOK:{\n\"sys/"
    "pangu/ChunkServerRole\": \"\",\n\"sys/pangu/PanguMasterRole\": \"\"}", //5
    "[2013-03-13 18:14:57.365716]\t[12835]\t[ERROR]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]", //6
    "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]", //7
    "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[ERROR]", //8
    "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]\t[5432187]", //9
    "[2013-03-13 "
    "18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]\t[5432187]\tcount:55", //10
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]", //11
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t", //12
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\n", //13
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tother\tcount:45", //14
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tother:\tcount:45", //15
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tcount:45", //16
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tcount:45\tnum:88\tjob:ss", //17
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corrupt\tcount:45\tnum:88\tjob:ss", //18
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corruptcount:45\tnum:88\tjob:ss", //19
    "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corrupt]count:45\tnum:88\tjob:ss", //20
    "[2013-03-13 18:14:57.365716]\t[build/debug64]\t[ERROR]\tcount:45\tnum:88\tjob:ss", //21
    "[2013-03-13 18:14:57.365716]\t[build/debug64:]\t[ERROR]\tcount:45\tnum:88\tjob:ss", //22
    "[2013-03-13 18:14:57.365716]\t[build/debug64:]\t[ERROR]\tcount:45\t:88\tjob:ss", //23
    "[2013-03-13 18:14:57.365716]", //24
    "[2013-03-13 18:14:57.365716]\t", //25
    "[2013-03-13 18:14:57.365716]\n", //26
    "[2013-03-13 18:14:57.365716]\t\t\t", //27
    "", //28
    "[2013-03-13 "
    "18:05:09.493309]\t[WARNING]\t[13000]\t[13003]\t[ERROR]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]", //29
    "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[13003]\t[ERROR]\t[tubo.cpp:1753]", //30
    "[2013-03-13 18:05:09.493309" //31
};

static const char* APSARA_FIELD_LEVEL = "__LEVEL__";
static const char* APSARA_FIELD_THREAD = "__THREAD__";
static const char* APSARA_FIELD_FILE = "__FILE__";
static const char* APSARA_FIELD_LINE = "__LINE__";

const char* logParseResult[][16] = {
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1753",
     NULL}, //1
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1753",
     NULL}, //2
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1754",
     NULL}, //3
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1755",
     "RealRecycle#Command",
     "rm -rf /apsara/tubo/.fuxi_tubo_trash/*",
     NULL}, //4
    {APSARA_FIELD_LEVEL,
     "ERROR",
     APSARA_FIELD_THREAD,
     "12835",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     "ParseWhiteListOK",
     "{\n\"sys/pangu/ChunkServerRole\": \"\",\n\"sys/pangu/PanguMasterRole\": \"\"}",
     NULL}, //5
    {APSARA_FIELD_THREAD,
     "12835",
     APSARA_FIELD_LEVEL,
     "ERROR",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     NULL}, //6
    {APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     APSARA_FIELD_THREAD,
     "12835",
     APSARA_FIELD_LEVEL,
     "ERROR",
     NULL}, //7
    {APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     APSARA_FIELD_LEVEL,
     "ERROR",
     NULL}, //8
    {APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     APSARA_FIELD_THREAD,
     "12835",
     APSARA_FIELD_LEVEL,
     "ERROR",
     NULL}, //9
    {APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1945",
     APSARA_FIELD_THREAD,
     "12835",
     APSARA_FIELD_LEVEL,
     "ERROR",
     "count",
     "55",
     NULL}, //10
    {APSARA_FIELD_LEVEL, "ERROR", NULL}, //11
    {APSARA_FIELD_LEVEL, "ERROR", NULL}, //12
    {APSARA_FIELD_LEVEL, "ERROR", NULL}, //13
    {APSARA_FIELD_LEVEL, "ERROR", "count", "45", NULL}, //14
    {APSARA_FIELD_LEVEL, "ERROR", "other", "", "count", "45", NULL}, //15
    {APSARA_FIELD_LEVEL, "ERROR", "count", "45", NULL}, //16
    {APSARA_FIELD_LEVEL, "ERROR", "count", "45", "num", "88", "job", "ss", NULL}, //17
    {APSARA_FIELD_LEVEL, "ERROR", "count", "45", "num", "88", "job", "ss", NULL}, //18
    {APSARA_FIELD_LEVEL, "ERROR", "[corruptcount", "45", "num", "88", "job", "ss", NULL}, //19
    {APSARA_FIELD_LEVEL, "ERROR", "[corrupt]count", "45", "num", "88", "job", "ss", NULL}, //20
    {APSARA_FIELD_FILE,
     "build/debug64",
     APSARA_FIELD_LEVEL,
     "ERROR",
     "count",
     "45",
     "num",
     "88",
     "job",
     "ss",
     NULL}, //21
    {APSARA_FIELD_FILE,
     "build/debug64",
     APSARA_FIELD_LINE,
     "",
     APSARA_FIELD_LEVEL,
     "ERROR",
     "count",
     "45",
     "num",
     "88",
     "job",
     "ss",
     NULL}, //22
    {APSARA_FIELD_FILE,
     "build/debug64",
     APSARA_FIELD_LINE,
     "",
     APSARA_FIELD_LEVEL,
     "ERROR",
     "count",
     "45",
     "",
     "88",
     "job",
     "ss",
     NULL}, //23
    {NULL}, //24
    {NULL}, //25
    {NULL}, //26
    {NULL}, //27
    {NULL}, //28
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "build/debug64/ilogtail/core/ilogtail.cpp",
     APSARA_FIELD_LINE,
     "1753",
     NULL}, //29
    {APSARA_FIELD_LEVEL,
     "WARNING",
     APSARA_FIELD_THREAD,
     "13000",
     APSARA_FIELD_FILE,
     "tubo.cpp",
     APSARA_FIELD_LINE,
     "1753",
     NULL}, //30
    {NULL} //31
};

class LogParserUnittest : public ::testing::Test {
public:
public:
    void TestApsaraEasyReadLogTimeParser();
    void TestApsaraEasyReadLogLineParser();
    void TestRegexLogLineParser();
    void TestLogTimeRegexParser();
    void TestParseTimestampNotInSeconds();
    void TestRegexLogLineParserWithTimeIndex();
    void TestLogParserParseLogTime();

    static void SetUpTestCase() //void Setup()
    {
        // mock data is all timeout, so shut up discard_old flag
        BOOL_FLAG(ilogtail_discard_old_data) = false;
        LOG_INFO(gLogger, ("LogParserUnittest", "setup"));
    }
    static void TearDownTestCase() //void CleanUp()
    {
        LOG_INFO(gLogger, ("LogParserUnittest", "cleanup"));
    }
};

APSARA_UNIT_TEST_CASE(LogParserUnittest, TestApsaraEasyReadLogTimeParser, 0);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestApsaraEasyReadLogLineParser, 1);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestRegexLogLineParser, 2);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestLogTimeRegexParser, 3);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestParseTimestampNotInSeconds, 4);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestRegexLogLineParserWithTimeIndex, 5);
APSARA_UNIT_TEST_CASE(LogParserUnittest, TestLogParserParseLogTime, 6);

void LogParserUnittest::TestApsaraEasyReadLogTimeParser() {
    LOG_INFO(sLogger, ("TestApsaraEasyReadLogTimeParser() begin", time(NULL)));
    string buffer = "[1378972170425093]\tA:B";
    int64_t microTime = 0;
    uint32_t dateTime = 0;
    time_t lastTime = 0;
    string lastStr;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972170);
    APSARA_TEST_EQUAL(microTime, 1378972170425093);
    APSARA_TEST_EQUAL(lastTime, 0);

    buffer = "[1378972171093]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972171);
    APSARA_TEST_EQUAL(microTime, 1378972171093000);
    APSARA_TEST_EQUAL(lastTime, 0);

    buffer = "[1378972172]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972172);
    APSARA_TEST_EQUAL(microTime, 1378972172000000);
    APSARA_TEST_EQUAL(lastTime, 0);

    buffer = "[2013-09-12 22:18:28.819129]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995508);
    APSARA_TEST_EQUAL(microTime, 1378995508819129);
    APSARA_TEST_EQUAL(dateTime, lastTime);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:28");

    buffer = "[2013-09-12 22:18:28.819139]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995508);
    APSARA_TEST_EQUAL(microTime, 1378995508819139);
    APSARA_TEST_EQUAL(dateTime, lastTime);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:28");

    buffer = "[2013-09-12 22:18:29.819139]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = LogParser::ApsaraEasyReadLogTimeParser(buffer.c_str(), lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995509);
    APSARA_TEST_EQUAL(microTime, 1378995509819139);
    APSARA_TEST_EQUAL(dateTime, lastTime);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:29");
    LOG_INFO(sLogger, ("TestApsaraEasyReadLogTimeParser() end", time(NULL)));
}

void LogParserUnittest::TestApsaraEasyReadLogLineParser() {
    LOG_INFO(sLogger, ("TestApsaraEasyReadLogLineParser() begin", time(NULL)));
    bool ret;
    for (uint32_t i = 0; i < sizeof(logLine) / sizeof(logLine[0]); i++) {
        LogGroup logGroup;
        string timeStr = "";
        time_t lastLogTime = 0;
        ParseLogError error;
        uint32_t logGroupSize = 0;
        ret = LogParser::ApsaraEasyReadLogLineParser(
            logLine[i], logGroup, true, timeStr, lastLogTime, "", "", "", "", error, logGroupSize);
        if (i == 27) //empty string
        {
            APSARA_TEST_TRUE_DESC(!ret, "Empty string should parse fail.");
            continue;
        }
        APSARA_TEST_TRUE_DESC(ret, string("ParseFail:") + logLine[i]);
        APSARA_TEST_EQUAL(logGroup.logs_size(), 1);
        if (logGroup.logs_size() != 1) {
            continue;
        }
        const Log& log = logGroup.logs(0);
        int32_t contentSize = log.contents_size();
        for (int j = 0; j < 10 && logParseResult[i][j] != NULL; j++) {
            if (contentSize <= j / 2) {
                APSARA_TEST_TRUE_DESC(false, string("miss content") + ToString(i + 1));
                break;
            }
            const Log_Content& content = log.contents(j / 2);
            if (j % 2 == 0) {
                APSARA_TEST_EQUAL_DESC(content.key(), logParseResult[i][j], ToString(i + 1));
            } else {
                APSARA_TEST_EQUAL_DESC(content.value(), logParseResult[i][j], ToString(i + 1));
            }
        }
    }
    LOG_INFO(sLogger, ("TestApsaraEasyReadLogLineParser() end", time(NULL)));
}

void LogParserUnittest::TestRegexLogLineParser() {
    LOG_INFO(sLogger, ("TestRegexLogLineParser() begin", time(NULL)));
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.key = "precise_timestamp_key";

    const char* timeFormat = "%Y-%m-%d %H:%M:%S";
    boost::regex rightReg = boost::regex("\\[([^\\]]+)\\]\\s*(.*)");
    boost::regex wrongReg = boost::regex("\\[(<^\\]]+)\\]\\s*(.*)");
    vector<string> keys;
    keys.push_back("time");
    keys.push_back("message");

    // case 0: reg parse failed.
    string timeStrSecond = "2013-10-31 21:03:49";
    string timeStr = timeStrSecond + ".123";
    string msgStr = "sample ace log\n";
    string bufferStr = "[" + timeStr + "] " + msgStr;
    string metricName = "ace";
    struct tm sTm;
    memset(&sTm, 0, sizeof(tm));
    strptime(timeStr.c_str(), timeFormat, &sTm);
    uint32_t logTime = mktime(&sTm);
    LogGroup logGroup;
    ParseLogError error;
    uint32_t logGroupSize;
    bool flag = LogParser::RegexLogLineParser(
        bufferStr.c_str(), wrongReg, logGroup, true, keys, metricName, logTime, "", "", "", error, logGroupSize);
    APSARA_TEST_EQUAL(flag, false);
    APSARA_TEST_EQUAL(logGroup.logs_size(), 0);

    // case 1: specified time
    logGroupSize = 0;
    flag = LogParser::RegexLogLineParser(
        bufferStr.c_str(), rightReg, logGroup, true, keys, metricName, logTime, "", "", "", error, logGroupSize);
    APSARA_TEST_EQUAL(flag, true);
    const Log& firstLog = logGroup.logs(0);
    APSARA_TEST_EQUAL(firstLog.contents_size(), 2);
    APSARA_TEST_EQUAL(firstLog.time(), logTime);
    APSARA_TEST_EQUAL(firstLog.contents(0).key(), "time");
    APSARA_TEST_EQUAL(firstLog.contents(0).value(), timeStr);
    APSARA_TEST_EQUAL(firstLog.contents(1).key(), "message");
    APSARA_TEST_EQUAL(firstLog.contents(1).value(), msgStr);

    // case 2: has last time
    string lastTimeStr = timeStrSecond;
    uint32_t timeIndex = 0;
    memset(&sTm, 0, sizeof(tm));
    strptime(lastTimeStr.c_str(), timeFormat, &sTm);
    time_t secondLogTime = mktime(&sTm);
    logGroupSize = 0;
    flag = LogParser::RegexLogLineParser(bufferStr.c_str(),
                                         rightReg,
                                         logGroup,
                                         true,
                                         keys,
                                         metricName,
                                         timeFormat,
                                         preciseTimestampConfig,
                                         timeIndex,
                                         lastTimeStr,
                                         secondLogTime,
                                         -1,
                                         "",
                                         "",
                                         "",
                                         error,
                                         logGroupSize);
    APSARA_TEST_EQUAL(flag, true);
    const Log& secondLog = logGroup.logs(1);
    APSARA_TEST_EQUAL(secondLog.contents_size(), 3);
    APSARA_TEST_EQUAL(secondLog.time(), secondLogTime);
    APSARA_TEST_EQUAL(secondLog.contents(0).key(), "time");
    APSARA_TEST_EQUAL(secondLog.contents(0).value(), timeStr);
    APSARA_TEST_EQUAL(secondLog.contents(1).key(), "message");
    APSARA_TEST_EQUAL(secondLog.contents(1).value(), msgStr);
    APSARA_TEST_EQUAL(secondLog.contents(2).key(), "precise_timestamp_key");
    APSARA_TEST_EQUAL(secondLog.contents(2).value(), "1383224629123");

    // case 3: has no last time
    lastTimeStr = "";
    logGroupSize = 0;
    timeStr = "2013-10-31 22:04:49.1";
    bufferStr = "[" + timeStr + "] " + msgStr;
    memset(&sTm, 0, sizeof(tm));
    strptime(timeStr.c_str(), timeFormat, &sTm);
    time_t thirdLogTime = mktime(&sTm);
    flag = LogParser::RegexLogLineParser(bufferStr.c_str(),
                                         rightReg,
                                         logGroup,
                                         true,
                                         keys,
                                         metricName,
                                         timeFormat,
                                         preciseTimestampConfig,
                                         timeIndex,
                                         lastTimeStr,
                                         secondLogTime,
                                         -1,
                                         "",
                                         "",
                                         "",
                                         error,
                                         logGroupSize);
    APSARA_TEST_EQUAL(flag, true);
    const Log& thirdLog = logGroup.logs(2);
    APSARA_TEST_EQUAL(thirdLog.contents_size(), 3);
    APSARA_TEST_EQUAL(thirdLog.time(), thirdLogTime);
    APSARA_TEST_EQUAL(thirdLog.contents(0).key(), "time");
    APSARA_TEST_EQUAL(thirdLog.contents(0).value(), timeStr);
    APSARA_TEST_EQUAL(thirdLog.contents(1).key(), "message");
    APSARA_TEST_EQUAL(thirdLog.contents(1).value(), msgStr);
    APSARA_TEST_EQUAL(thirdLog.contents(2).key(), "precise_timestamp_key");
    APSARA_TEST_EQUAL(thirdLog.contents(2).value(), "1383228289100");

    // test time adjust
    int localOffset = GetLocalTimeZoneOffsetSecond();
    int hawaiiTimeZoneOffsetSecond = -10 * 3600;

    int logTZSecond;
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("GMT-10:00", logTZSecond), true);
    APSARA_TEST_EQUAL(logTZSecond, -10 * 3600);

    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("GMT-04:30", logTZSecond), true);
    APSARA_TEST_EQUAL(logTZSecond, -4 * 3600 - 30 * 60);


    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("GMT+04:30", logTZSecond), true);
    APSARA_TEST_EQUAL(logTZSecond, 4 * 3600 + 30 * 60);

    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("gmt-04:30", logTZSecond), false);
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("GMTx04:30", logTZSecond), false);
    APSARA_TEST_EQUAL(ConfigManager::GetInstance()->ParseTimeZoneOffsetSecond("GMT-04:3", logTZSecond), false);


    LogParser::AdjustLogTime(logGroup.mutable_logs(2), hawaiiTimeZoneOffsetSecond, localOffset);
    APSARA_TEST_EQUAL(thirdLog.time(), thirdLogTime + (localOffset - hawaiiTimeZoneOffsetSecond));

    LOG_INFO(sLogger,
             ("hawaii time", timeStr)("local time", GetTimeStamp(thirdLog.time(), timeFormat))(
                 "UTC time", thirdLog.time())("local timezone offset", localOffset));

    // case 4
    flag = LogParser::RegexLogLineParser(bufferStr.c_str(),
                                         wrongReg,
                                         logGroup,
                                         true,
                                         keys,
                                         metricName,
                                         timeFormat,
                                         preciseTimestampConfig,
                                         timeIndex,
                                         lastTimeStr,
                                         thirdLogTime,
                                         -1,
                                         "",
                                         "",
                                         "",
                                         error,
                                         logGroupSize);
    APSARA_TEST_EQUAL(flag, false);
    LOG_INFO(sLogger, ("TestRegexLogLineParser() end", time(NULL)));
}

void LogParserUnittest::TestLogTimeRegexParser() {
    LOG_INFO(sLogger, ("TestLogTimeRegexParser() begin", time(NULL)));

    const char* logBufferRight
        = "2018-07-05 04:01:02 [DEBUG] removed /home/admin/logs/zclean.log.2018-06-20 size 193 in 1 seconds";
    const char* logBufferWrong
        = "2018-7-5   4:01:02 [DEBUG] removed /home/admin/logs/zclean.log.2018-06-20 size 193 in 1 seconds";
    const char* logBufferRight2
        = "[2018-08-07 20:30:38.652271]    [WARNING]       [7280]  [build/debug64/sls/ilogtail/LogFileReader.cpp:652]  "
          "    "
          "parse regex log fail:2018-07-05 04:01:02 [DEBUG] removed /home/admin/logs/zclean.log.2018-06-20 size 193 in "
          "1 seconds   region: project:        logstore:       file:";
    const char* timeRegRight = "([0-9]{4})-(0[0-9]{1}|1[0-2])-(0[0-9]{1}|[12][0-9]{1}|3[01]) "
                               "(0[0-9]{1}|1[0-9]{1}|2[0-3]):[0-5][0-9]{1}:([0-5][0-9]{1})";
    // NOTICE: \d is not supported in C++ regex extension, supported in boost::regex
    const char* timeRegWrong
        = "(\\d{4})-(0\\d{1}|1[0-2])-(0\\d{1}|[12]\\d{1}|3[01])\\s(0\\d{1}|1\\d{1}|2[0-3]):[0-5]\\d{1}:([0-5]\\d{1})";

    boost::regex reg1(timeRegRight);
    boost::regex reg2(timeRegWrong);

    std::string timeFormat("%Y-%m-%d %H:%M:%S");
    time_t logTime = -1;
    bool flag = LogFileReader::ParseLogTime(logBufferRight, &reg2, logTime, timeFormat);
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime, 1530734462);

    logTime = -1;
    flag = LogFileReader::ParseLogTime(logBufferWrong, &reg1, logTime, timeFormat);
    APSARA_TEST_EQUAL(flag, false);
    APSARA_TEST_EQUAL(logTime, -1);

    flag = LogFileReader::ParseLogTime(logBufferRight, &reg2, logTime, timeFormat);
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime, 1530734462); // 2018-07-05 04:01:02 unix time: 1530734462

    logTime = -1;
    flag = LogFileReader::ParseLogTime(logBufferRight, &reg1, logTime, timeFormat);
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime, 1530734462); // 2018-07-05 04:01:02 unix time: 1530734462

    logTime = -1;
    flag = LogFileReader::ParseLogTime(logBufferRight2, &reg1, logTime, timeFormat);
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime, 1533645038); // 2018-08-07 20:30:38 unix time: 1533645038

    // test get log time by offset
    time_t logTime2 = -1;
    flag = LogFileReader::GetLogTimeByOffset(logBufferRight, 0, logTime2, timeFormat);
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime2, 1530734462);

    logTime2 = -1;
    flag = LogFileReader::GetLogTimeByOffset(logBufferRight2, 1, logTime2, timeFormat); // offset is 1
    APSARA_TEST_EQUAL(flag, true);
    APSARA_TEST_EQUAL(logTime2, 1533645038);

    LOG_INFO(sLogger, ("TestLogTimeRegexParser() end", time(NULL)));
}

void LogParserUnittest::TestParseTimestampNotInSeconds() {
    LOG_INFO(sLogger, ("TestParseTimestampNotInSeconds() begin", time(NULL)));

    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    preciseTimestampConfig.key = "key";

    struct Case {
        std::string inputTimeStr;
        std::string outputTimeStr;
        time_t outputTime;
    };
    std::vector<Case> cases;
    auto milliseconds = GetCurrentTimeInMilliSeconds();
    cases.push_back(
        {std::to_string(milliseconds), std::to_string(milliseconds / 1000), static_cast<time_t>(milliseconds / 1000)});
    auto microseconds = GetCurrentTimeInMicroSeconds();
    cases.push_back({std::to_string(microseconds),
                     std::to_string(microseconds / 1000000),
                     static_cast<time_t>(microseconds / 1000000)});

    for (size_t i = 0; i < cases.size(); ++i) {
        auto& c = cases[i];
        std::string outTimeStr;
        uint64_t preciseTimestamp;
        time_t outTime = 0;
        ParseLogError error;
        bool ret = LogParser::ParseLogTime("TestData",
                                           outTimeStr,
                                           outTime,
                                           preciseTimestamp,
                                           c.inputTimeStr,
                                           "%s",
                                           preciseTimestampConfig,
                                           -1,
                                           "",
                                           "",
                                           "",
                                           "",
                                           error);
        LOG_INFO(sLogger,
                 ("Case", i)("ret", ret)(outTimeStr, outTime)("preciseTimestamp", preciseTimestamp)("error", error));
        EXPECT_TRUE(ret);
        EXPECT_EQ(outTimeStr, c.outputTimeStr);
        EXPECT_EQ(outTime, c.outputTime);
    }

    std::string outTimeStr;
    time_t outTime = 0;
    uint64_t preciseTimestamp;
    ParseLogError error;
    bool ret = LogParser::ParseLogTime("BadTestData",
                                       outTimeStr,
                                       outTime,
                                       preciseTimestamp,
                                       "abcd",
                                       "%s",
                                       preciseTimestampConfig,
                                       -1,
                                       "",
                                       "",
                                       "",
                                       "",
                                       error);
    LOG_INFO(sLogger, ("Bad case", ret)("error", error));
    EXPECT_FALSE(ret);
#if defined(_MSC_VER)
    EXPECT_EQ(PARSE_LOG_HISTORY_ERROR, error);
#else
    EXPECT_EQ(PARSE_LOG_TIMEFORMAT_ERROR, error);
#endif

    LOG_INFO(sLogger, ("TestParseTimestampNotInSeconds() end", time(NULL)));
}

void LogParserUnittest::TestRegexLogLineParserWithTimeIndex() {
    LOG_INFO(sLogger, ("TestRegexLogLineParserWithTimeIndex() begin", time(NULL)));

    const char* timeFormat = "%Y-%m-%d %H:%M:%S";
    boost::regex reg = boost::regex("\\[([^\\]]+)\\]\\s(\\w+)\\s(.*)");
    vector<string> keys;
    keys.push_back("time");
    keys.push_back("message");
    keys.push_back("mytime");

    string timeStr = "2013-10-31 21:03:49";
    string msgStr = "sample";
    string mytimeStr = "2013-10-31 21:03:50:1234"; // timeStr + 1
    const uint32_t timeIndex = 2;
    string metricName = "ace";

    string bufferStr = "[" + timeStr + "] " + msgStr + " " + mytimeStr;

    string lastTimeStr = timeStr;
    struct tm sTm;
    memset(&sTm, 0, sizeof(tm));
    strptime(lastTimeStr.c_str(), timeFormat, &sTm);
    time_t logTime = mktime(&sTm);

    lastTimeStr.clear(); // Force RegexLogLineParser parse the time string.
    time_t myLogTime = logTime;
    LogGroup logGroup;
    uint32_t logGroupSize = 0;
    ParseLogError error;
    PreciseTimestampConfig preciseTimestampConfig;
    preciseTimestampConfig.enabled = true;
    bool flag = LogParser::RegexLogLineParser(bufferStr.c_str(),
                                              reg,
                                              logGroup,
                                              true,
                                              keys,
                                              metricName,
                                              timeFormat,
                                              preciseTimestampConfig,
                                              timeIndex,
                                              lastTimeStr,
                                              myLogTime,
                                              -1,
                                              "",
                                              "",
                                              "",
                                              error,
                                              logGroupSize);
    APSARA_TEST_EQUAL(flag, true);
    const Log& log = logGroup.logs(0);
    APSARA_TEST_EQUAL(log.time(), myLogTime);
    APSARA_TEST_EQUAL(log.time(), logTime + 1);
    APSARA_TEST_EQUAL(log.contents(0).key(), "time");
    APSARA_TEST_EQUAL(log.contents(0).value(), timeStr);
    APSARA_TEST_EQUAL(log.contents(1).key(), "message");
    APSARA_TEST_EQUAL(log.contents(1).value(), msgStr);
    APSARA_TEST_EQUAL(log.contents(2).key(), "mytime");
    APSARA_TEST_EQUAL(log.contents(2).value(), mytimeStr);
    APSARA_TEST_EQUAL(log.contents(3).key(), "precise_timestamp");
    APSARA_TEST_EQUAL(log.contents(3).value(), "1383224630123");

    LOG_INFO(sLogger, ("TestRegexLogLineParserWithTimeIndex() end", time(NULL)));
}

void LogParserUnittest::TestLogParserParseLogTime() {
    LOG_INFO(sLogger, ("TestLogParserParseLogTime() begin", time(NULL)));
    struct Case {
        std::string inputTimeStr;
        std::string fmtStr;
        time_t exceptedLogTime;
        uint64_t exceptedPreciseTimestamp;
    };

    vector<Case> inputTimes = {
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S", 1484118307, 1484118307012},
        {"2017-1-11 15:05:07.012", "%Y-%m-%d %H:%M:%S", 1484118307, 1484118307012},
        {"[2017-1-11 15:05:07.0123]", "[%Y-%m-%d %H:%M:%S", 1484118307, 1484118307012},
        {"11 Jan 17 15:05 MST", "%d %b %y %H:%M", 1484118300, 1484118300000},
        {"11 Jan 17 15:05 -0700", "%d %b %y %H:%M", 1484118300, 1484118300000},
        {"Tuesday, 11-Jan-17 15:05:07.0123 MST", "%A, %d-%b-%y %H:%M:%S", 1484118307, 1484118307012},
        {"Tuesday, 11 Jan 2017 15:05:07 MST", "%A, %d %b %Y %H:%M:%S", 1484118307, 1484118307000},
        {"2017-01-11T15:05:07Z08:00", "%Y-%m-%dT%H:%M:%S", 1484118307, 1484118307000},
        {"2017-01-11T15:05:07.012999999Z07:00", "%Y-%m-%dT%H:%M:%S", 1484118307, 1484118307012},
        {"1484118307", "%s", 1484118307, 1484118307000},
        {"1484118307123", "%s", 1484118307, 1484118307000},
    };

    for (size_t i = 0; i < inputTimes.size(); ++i) {
        auto& c = inputTimes[i];
        std::string outTimeStr;
        time_t outTime = 0;
        ParseLogError error;
        uint64_t preciseTimestamp = 0;
        PreciseTimestampConfig preciseTimestampConfig;
        preciseTimestampConfig.enabled = true;
        preciseTimestampConfig.key = "key";
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
                                           error);
        LOG_INFO(sLogger,
                 ("Case", i)("InputTimeStr", c.inputTimeStr)("FormatStr", c.fmtStr)("ret", ret)(outTimeStr, outTime)(
                     "preciseTimestamp", preciseTimestamp)("error", error));
        APSARA_TEST_EQUAL(ret, true);
        APSARA_TEST_EQUAL(outTime, c.exceptedLogTime);
        APSARA_TEST_EQUAL(preciseTimestamp, c.exceptedPreciseTimestamp);
    }
    LOG_INFO(sLogger, ("TestLogParserParseLogTime() end", time(NULL)));
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    logtail::gLogger = logtail::Logger::Instance().GetLogger("/logger/global/test");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
