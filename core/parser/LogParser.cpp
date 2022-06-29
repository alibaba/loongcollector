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

#include "LogParser.h"
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <regex>
#include "common/StringTools.h"
#include "common/util.h"
#include "common/LogtailCommonFlags.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogtailAlarm.h"
#include "app_config/AppConfig.h"

using namespace std;
using namespace sls_logs;

namespace logtail {

static const char* SLS_KEY_LEVEL = "__LEVEL__";
static const char* SLS_KEY_THREAD = "__THREAD__";
static const char* SLS_KEY_FILE = "__FILE__";
static const char* SLS_KEY_LINE = "__LINE__";
static const int32_t MAX_BASE_FIELD_NUM = 10;
const char* LogParser::UNMATCH_LOG_KEY = "__raw_log__";

bool LogParser::IsPrefixString(const string& all, const string& prefix) {
    if (all.size() < prefix.size())
        return false;
    if (prefix.size() == 0)
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (all[i] != prefix[i])
            return false;
    }
    return true;
}

bool LogParser::IsPrefixString(const char* all, const string& prefix) {
    if (prefix.size() == 0)
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (all[i] == '\0')
            return false;
        if (all[i] != prefix[i])
            return false;
    }
    return true;
}

time_t
LogParser::ApsaraEasyReadLogTimeParser(const char* buffer, string& timeStr, time_t& lastLogTime, int64_t& microTime) {
    int beg_index = 0;
    if (buffer[beg_index] != '[') {
        return 0;
    }
    int curTime = 0;
    if (buffer[1] == '1') // for normal time, e.g 1378882630, starts with '1'
    {
        int i = 0;
        for (; i < 16; i++) // 16 = 10(second width) + 6(micro part)
        {
            const char& c = buffer[1 + i];
            if (c >= '0' && c <= '9') {
                if (i < 10) {
                    curTime = curTime * 10 + c - '0';
                }
                microTime = microTime * 10 + c - '0';
            } else {
                break;
            }
        }
        if (i >= 10) {
            while (i < 16) {
                microTime *= 10;
                i++;
            }
            return curTime;
        }
    }
    // test other date format case
    {
        if (IsPrefixString(buffer + beg_index + 1, timeStr) == true) {
            microTime = (int64_t)lastLogTime * 1000000 + GetApsaraLogMicroTime(buffer);
            return lastLogTime;
        }
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        if (NULL == strptime(buffer + beg_index + 1, "%Y-%m-%d %H:%M:%S", &tm)) {
            LOG_WARNING(sLogger,
                        ("parse apsara log time", "fail")("string", buffer)("timeformat", "%Y-%m-%d %H:%M:%S"));
            return 0;
        }
        tm.tm_isdst = -1;
        lastLogTime = mktime(&tm);
        // if the time is valid (strptime not return NULL), the date value size must be 19 ,like '2013-09-11 03:11:05'
        timeStr = string(buffer + beg_index + 1, 19);

        microTime = (int64_t)lastLogTime * 1000000 + GetApsaraLogMicroTime(buffer);
        return lastLogTime;
    }
}

void LogParser::AddUnmatchLog(const char* buffer, sls_logs::LogGroup& logGroup, uint32_t& logGroupSize) {
    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(time(NULL));
    AddLog(logPtr, UNMATCH_LOG_KEY, buffer, logGroupSize);
}

#if defined(_MSC_VER)
static bool StdRegexMatch(const char* buffer, const std::regex& re, std::string& exception, std::cmatch& match) {
    try {
        if (std::regex_search(buffer, match, re) && match.size() > 1)
            return true;
        return false;
    } catch (std::regex_error& e) {
        exception = "std::regex_error: ";
        exception += e.what();
    } catch (std::exception& e) {
        exception = "std::exception: ";
        exception += e.what();
    } catch (...) {
        exception = "unknown error";
    }

    return false;
}

static bool StdRegexLogLineParser(const char* buffer,
                                  const std::string& regStr,
                                  LogGroup& logGroup,
                                  bool discardUnmatch,
                                  const vector<string>& keys,
                                  const string& category,
                                  const char* timeFormat,
                                  const PreciseTimestampConfig& preciseTimestampConfig,
                                  const uint32_t timeIndex,
                                  string& timeStr,
                                  time_t& logTime,
                                  int32_t specifiedYear,
                                  const string& projectName,
                                  const string& region,
                                  const string& logPath,
                                  ParseLogError& error,
                                  uint32_t& logGroupSize) {
    std::regex stdReg;
    std::string exception;
    try {
        stdReg = std::regex(regStr);
    } catch (std::regex_error& e) {
        exception = "during construct regex" + regStr + ", std::regex_error: ";
        exception += e.what();
    } catch (std::exception& e) {
        exception = "during construct regex" + regStr + ", std::exception: ";
        exception += e.what();
    } catch (...) {
        exception = "during construct regex" + regStr + ", unknown error";
    }

    std::cmatch match;
    bool parseSuccess = true;
    uint64_t preciseTimestamp = 0;
    if (!exception.empty() || !StdRegexMatch(buffer, stdReg, exception, match)) {
        if (!exception.empty()) {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_ERROR(sLogger,
                              ("parse regex log fail", buffer)("exception", exception)("project", projectName)(
                                  "logstore", category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                       "errorlog:" + string(buffer)
                                                           + " | exception:" + string(exception),
                                                       projectName,
                                                       category,
                                                       region);
            }
        } else {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_WARNING(sLogger,
                                ("parse regex log fail", buffer)("project", projectName)("logstore",
                                                                                         category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(
                    REGEX_MATCH_ALARM, "errorlog:" + string(buffer), projectName, category, region);
            }
        }
        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;
    } else if (match.size() <= keys.size()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse key count not match", match.size())("parse regex log fail", buffer)(
                                "project", projectName)("logstore", category)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                   "parse key count not match" + ToString(match.size() - 1)
                                                       + "errorlog:" + string(buffer),
                                                   projectName,
                                                   category,
                                                   region);
        }

        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;
    } else if (!LogParser::ParseLogTime(buffer,
                                        timeStr,
                                        logTime,
                                        preciseTimestamp,
                                        match.str(timeIndex + 1),
                                        timeFormat,
                                        preciseTimestampConfig,
                                        specifiedYear,
                                        projectName,
                                        category,
                                        region,
                                        logPath,
                                        error)) {
        parseSuccess = false;
        if (error == PARSE_LOG_HISTORY_ERROR)
            return false;
    }

    if (parseSuccess) {
        Log* logPtr = logGroup.add_logs();
        logPtr->set_time(logTime);
        if (preciseTimestampConfig.enabled) {
            LogParser::AddLog(logPtr, preciseTimestampConfig.key, std::to_string(preciseTimestamp), logGroupSize);
        }

        for (uint32_t i = 0; i < keys.size(); i++) {
            LogParser::AddLog(logPtr, keys[i], match[i + 1].str(), logGroupSize);
        }
        return true;
    } else if (!discardUnmatch) {
        LogParser::AddUnmatchLog(buffer, logGroup, logGroupSize);
        return true;
    }
    return false;
}
#endif

bool LogParser::RegexLogLineParser(const char* buffer,
                                   const boost::regex& reg,
                                   LogGroup& logGroup,
                                   bool discardUnmatch,
                                   const vector<string>& keys,
                                   const string& category,
                                   const char* timeFormat,
                                   const PreciseTimestampConfig& preciseTimestampConfig,
                                   const uint32_t timeIndex,
                                   string& timeStr,
                                   time_t& logTime,
                                   int32_t specifiedYear,
                                   const string& projectName,
                                   const string& region,
                                   const string& logPath,
                                   ParseLogError& error,
                                   uint32_t& logGroupSize) {
    boost::match_results<const char*> what;
    string exception;
    uint64_t preciseTimestamp = 0;
    bool parseSuccess = true;
    if (!BoostRegexMatch(buffer, reg, exception, what, boost::match_default)) {
#if defined(_MSC_VER) // Try std::regex on Windows.
        return StdRegexLogLineParser(buffer,
                                     reg.str(),
                                     logGroup,
                                     discardUnmatch,
                                     keys,
                                     category,
                                     timeFormat,
                                     preciseTimestampConfig,
                                     timeIndex,
                                     timeStr,
                                     logTime,
                                     specifiedYear,
                                     projectName,
                                     region,
                                     logPath,
                                     error,
                                     logGroupSize);
#endif

        if (!exception.empty()) {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_ERROR(sLogger,
                              ("parse regex log fail", buffer)("exception", exception)("project", projectName)(
                                  "logstore", category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                       "errorlog:" + string(buffer)
                                                           + " | exception:" + string(exception),
                                                       projectName,
                                                       category,
                                                       region);
            }
        } else {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_WARNING(sLogger,
                                ("parse regex log fail", buffer)("project", projectName)("logstore",
                                                                                         category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(
                    REGEX_MATCH_ALARM, "errorlog:" + string(buffer), projectName, category, region);
            }
        }
        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;
    } else if (what.size() <= keys.size()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse key count not match", what.size())("parse regex log fail", buffer)(
                                "project", projectName)("logstore", category)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                   "parse key count not match" + ToString(what.size())
                                                       + "errorlog:" + string(buffer),
                                                   projectName,
                                                   category,
                                                   region);
        }

        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;
    } else if (!ParseLogTime(buffer,
                             timeStr,
                             logTime,
                             preciseTimestamp,
                             what[timeIndex + 1].str().c_str(),
                             timeFormat,
                             preciseTimestampConfig,
                             specifiedYear,
                             projectName,
                             category,
                             region,
                             logPath,
                             error)) {
        parseSuccess = false;
        if (error == PARSE_LOG_HISTORY_ERROR)
            return false;
    }

    if (parseSuccess) {
        Log* logPtr = logGroup.add_logs();
        logPtr->set_time(logTime);
        for (uint32_t i = 0; i < keys.size(); i++) {
            AddLog(logPtr, keys[i], what[i + 1].str(), logGroupSize);
        }
        if (preciseTimestampConfig.enabled) {
            AddLog(logPtr, preciseTimestampConfig.key, std::to_string(preciseTimestamp), logGroupSize);
        }
        return true;
    } else if (!discardUnmatch) {
        LogParser::AddUnmatchLog(buffer, logGroup, logGroupSize);
        return true;
    }
    return false;
}

bool LogParser::RegexLogLineParser(const char* buffer,
                                   const boost::regex& reg,
                                   LogGroup& logGroup,
                                   bool discardUnmatch,
                                   const vector<string>& keys,
                                   const string& category,
                                   time_t logTime,
                                   const string& projectName,
                                   const string& region,
                                   const string& logPath,
                                   ParseLogError& error,
                                   uint32_t& logGroupSize) {
    boost::match_results<const char*> what;
    string exception;
    bool parseSuccess = true;
    if (!BoostRegexMatch(buffer, reg, exception, what, boost::match_default)) {
        if (!exception.empty()) {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_ERROR(sLogger,
                              ("parse regex log fail", buffer)("exception", exception)("project", projectName)(
                                  "logstore", category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                       "errorlog:" + string(buffer)
                                                           + " | exception:" + string(exception),
                                                       projectName,
                                                       category,
                                                       region);
            }
        } else {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_WARNING(sLogger,
                                ("parse regex log fail", buffer)("project", projectName)("logstore",
                                                                                         category)("file", logPath));
                }
                LogtailAlarm::GetInstance()->SendAlarm(
                    REGEX_MATCH_ALARM, string("errorlog:") + string(buffer), projectName, category, region);
            }
        }

        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;

    } else if (what.size() <= keys.size()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse key count not match", what.size())("parse regex log fail", buffer)(
                                "project", projectName)("logstore", category)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(REGEX_MATCH_ALARM,
                                                   "parse key count not match" + ToString(what.size())
                                                       + "errorlog:" + string(buffer),
                                                   projectName,
                                                   category,
                                                   region);
        }

        error = PARSE_LOG_REGEX_ERROR;
        parseSuccess = false;
    }
    if (!parseSuccess) {
        if (discardUnmatch)
            return false;
        else {
            AddUnmatchLog(buffer, logGroup, logGroupSize);
            return true;
        }
    }

    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(logTime); // current system time, no need history check
    for (uint32_t i = 0; i < keys.size(); i++) {
        AddLog(logPtr, keys[i], what[i + 1].str(), logGroupSize);
    }
    return true;
}

bool LogParser::ParseLogTime(const char* buffer,
                             std::string& timeStr,
                             time_t& logTime,
                             uint64_t& preciseTimestamp,
                             const std::string& curTimeStr,
                             const char* timeFormat,
                             const PreciseTimestampConfig& preciseTimestampConfig,
                             int32_t specifiedYear,
                             const string& projectName,
                             const string& category,
                             const string& region,
                             const string& logPath,
                             ParseLogError& error) {
    if (IsPrefixString(curTimeStr, timeStr) == false) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        // In order to handle timestamp not in seconds, curTimeStr will be truncated,
        // only the front 10 charaters will be used.
        // NOTE: This method can only work until 2286/11/21 1:46:39 (9999999999).
        bool keepTimeStr = (strcmp("%s", timeFormat) != 0);
        const char* strptimeResult = NULL;
        if (keepTimeStr) {
            strptimeResult = Strptime(curTimeStr.c_str(), timeFormat, &tm, specifiedYear);
        } else {
            strptimeResult = Strptime(curTimeStr.substr(0, 10).c_str(), timeFormat, &tm);
        }
        if (NULL == strptimeResult) {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                    LOG_WARNING(sLogger,
                                ("parse time fail", curTimeStr)("project", projectName)("logstore", category)(
                                    "file", logPath)("keep time str", keepTimeStr));
                }
                LogtailAlarm::GetInstance()->SendAlarm(PARSE_TIME_FAIL_ALARM,
                                                       curTimeStr + " " + timeFormat
                                                           + " flag: " + std::to_string(keepTimeStr),
                                                       projectName,
                                                       category,
                                                       region);
            }

            error = PARSE_LOG_TIMEFORMAT_ERROR;
            return false;
        }
        tm.tm_isdst = -1;
        logTime = mktime(&tm);
        timeStr = ConvertToTimeStamp(logTime, timeFormat);

        if (preciseTimestampConfig.enabled) {
            preciseTimestamp = GetPreciseTimestamp(logTime, strptimeResult, preciseTimestampConfig);
        }
    } else {
        if (preciseTimestampConfig.enabled) {
            preciseTimestamp
                = GetPreciseTimestamp(logTime, curTimeStr.substr(timeStr.length()).c_str(), preciseTimestampConfig);
        }
    }

    if (logTime <= 0
        || (BOOL_FLAG(ilogtail_discard_old_data) && (time(NULL) - logTime) > INT32_FLAG(ilogtail_discard_interval))) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard history data", buffer)("timestamp", logTime)("project", projectName)(
                                "logstore", category)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(
                OUTDATED_LOG_ALARM, string("logTime: ") + ToString(logTime), projectName, category, region);
        }
        error = PARSE_LOG_HISTORY_ERROR;
        return false;
    }
    return true;
}

bool LogParser::WholeLineModeParser(
    const char* buffer, LogGroup& logGroup, const string& key, time_t logTime, uint32_t& logGroupSize) {
    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(logTime); // current system time, no need history check
    AddLog(logPtr, key, buffer, logGroupSize);
    return true;
}

int32_t LogParser::GetApsaraLogMicroTime(const char* buffer) {
    int begIndex = 0;
    while (buffer[begIndex]) {
        if (buffer[begIndex] == '.') {
            begIndex++;
            break;
        }
        begIndex++;
    }
    char* endPtr;
    return strtol(buffer + begIndex, &endPtr, 10);
}

static int32_t FindBaseFields(const char* buffer, int32_t beginIndexArray[], int32_t endIndexArray[]) {
    int32_t baseFieldNum = 0;
    for (int32_t i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '[') {
            beginIndexArray[baseFieldNum] = i + 1;
        } else if (buffer[i] == ']') {
            if (buffer[i + 1] == '\t' || buffer[i + 1] == '\0' || buffer[i + 1] == '\n') {
                endIndexArray[baseFieldNum] = i;
                baseFieldNum++;
            }
            if (baseFieldNum >= MAX_BASE_FIELD_NUM) {
                break;
            }
            if (buffer[i + 1] == '\t' && buffer[i + 2] != '[') {
                break;
            }
        }
    }
    return baseFieldNum;
}
static bool IsFieldLevel(const char* buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > 'Z' || buffer[i] < 'A') {
            return false;
        }
    }
    return true;
}
static bool IsFieldThread(const char* buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > '9' || buffer[i] < '0') {
            return false;
        }
    }
    return true;
}
static bool IsFieldFileLine(const char* buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == '/' || buffer[i] == '.') {
            return true;
        }
    }
    return false;
}
static int32_t FindColonIndex(const char* buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == ':') {
            return i;
        }
    }
    return endIndex;
}

static int32_t ParseApsaraBaseFields(const char* buffer, Log* logPtr, uint32_t& logGroupSize) {
    int32_t beginIndexArray[MAX_BASE_FIELD_NUM] = {0};
    int32_t endIndexArray[MAX_BASE_FIELD_NUM] = {0};
    int32_t baseFieldNum = FindBaseFields(buffer, beginIndexArray, endIndexArray);
    if (baseFieldNum == 0) {
        return 0;
    }
    int32_t beginIndex, endIndex;
    int32_t findFieldBitMap = 0x0;
    //i=0 field is the time field.
    for (int32_t i = 1; findFieldBitMap != 0x111 && i < baseFieldNum; i++) {
        beginIndex = beginIndexArray[i];
        endIndex = endIndexArray[i];
        if ((findFieldBitMap & 0x1) == 0 && IsFieldLevel(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x1;
            LogParser::AddLog(logPtr, SLS_KEY_LEVEL, string(buffer + beginIndex, endIndex - beginIndex), logGroupSize);
        } else if ((findFieldBitMap & 0x10) == 0 && IsFieldThread(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x10;
            LogParser::AddLog(logPtr, SLS_KEY_THREAD, string(buffer + beginIndex, endIndex - beginIndex), logGroupSize);
        } else if ((findFieldBitMap & 0x100) == 0 && IsFieldFileLine(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x100;
            int32_t colonIndex = FindColonIndex(buffer, beginIndex, endIndex);
            LogParser::AddLog(logPtr, SLS_KEY_FILE, string(buffer + beginIndex, colonIndex - beginIndex), logGroupSize);
            if (colonIndex < endIndex) {
                LogParser::AddLog(
                    logPtr, SLS_KEY_LINE, string(buffer + colonIndex + 1, endIndex - colonIndex - 1), logGroupSize);
            }
        }
    }
    return endIndexArray[baseFieldNum - 1]; //return ']' position
}

bool LogParser::ApsaraEasyReadLogLineParser(const char* buffer,
                                            LogGroup& logGroup,
                                            bool discardUnmatch,
                                            string& timeStr,
                                            time_t& lastLogTime,
                                            const string& projectName,
                                            const string& category,
                                            const string& region,
                                            const string& logPath,
                                            ParseLogError& error,
                                            uint32_t& logGroupSize) {
    int64_t logTime_in_micro = 0;
    time_t logTime = LogParser::ApsaraEasyReadLogTimeParser(buffer, timeStr, lastLogTime, logTime_in_micro);
    if (logTime <= 0) // this case will handle empty apsara log line
    {
        string bufOut(buffer);
        if (bufOut.size() > (size_t)(1024)) {
            bufOut.resize(1024);
        }
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard error timeformat log", bufOut)("parsed time", logTime)("project", projectName)(
                                "logstore", category)("file", logPath));
            }
        }

        LogtailAlarm::GetInstance()->SendAlarm(
            PARSE_TIME_FAIL_ALARM, bufOut + " $ " + ToString(logTime), projectName, category, region);
        error = PARSE_LOG_TIMEFORMAT_ERROR;

        if (discardUnmatch)
            return false;
        else {
            AddUnmatchLog(buffer, logGroup, logGroupSize);
            return true;
        }
    }
    if (BOOL_FLAG(ilogtail_discard_old_data) && (time(NULL) - logTime) > INT32_FLAG(ilogtail_discard_interval)) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            string bufOut(buffer);
            if (bufOut.size() > (size_t)(1024)) {
                bufOut.resize(1024);
            }
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard history data, first 1k", bufOut)("parsed time", logTime)("project", projectName)(
                                "logstore", category)("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(OUTDATED_LOG_ALARM,
                                                   string("logTime: ") + ToString(logTime) + ", log:" + bufOut,
                                                   projectName,
                                                   category,
                                                   region);
        }

        error = PARSE_LOG_HISTORY_ERROR;
        return false;
    }

    Log* logPtr = logGroup.add_logs();
    logPtr->set_time(logTime);
    int32_t beg_index = 0;
    int32_t colon_index = -1;
    int32_t index = -1;
    index = ParseApsaraBaseFields(buffer, logPtr, logGroupSize);
    if (buffer[index] != 0) {
        do {
            ++index;
            if (buffer[index] == '\t' || buffer[index] == '\0') {
                if (colon_index >= 0) {
                    AddLog(logPtr,
                           string(buffer + beg_index, colon_index - beg_index),
                           string(buffer + colon_index + 1, index - colon_index - 1),
                           logGroupSize);
                    colon_index = -1;
                }
                beg_index = index + 1;
            } else if (buffer[index] == ':' && colon_index == -1) {
                colon_index = index;
            }
        } while (buffer[index]);
    }
    char s_micro[20] = {0};
#if defined(__linux__)
    sprintf(s_micro, "%ld", logTime_in_micro);
#elif defined(_MSC_VER)
    sprintf(s_micro, "%lld", logTime_in_micro);
#endif
    AddLog(logPtr, "microtime", string(s_micro), logGroupSize);
    return true;
}

void LogParser::AddLog(Log* logPtr, const string& key, const string& value, uint32_t& logGroupSize) {
    Log_Content* logContentPtr = logPtr->add_contents();
    logContentPtr->set_key(key);
    logContentPtr->set_value(value);
    logGroupSize += key.size() + value.size() + 5;
}

void LogParser::AdjustLogTime(sls_logs::Log* logPtr, int mLogTimeZoneOffsetSecond, int timeZoneOffsetSecond) {
    logPtr->set_time(logPtr->time() - mLogTimeZoneOffsetSecond + timeZoneOffsetSecond);
}

} // namespace logtail
