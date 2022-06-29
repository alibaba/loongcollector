/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <stdint.h>
#include <boost/regex.hpp>
#include <vector>
#include "common/TimeUtil.h"

namespace sls_logs {
class LogGroup;
class Log;
} // namespace sls_logs

namespace logtail {

struct UserDefinedFormat {
    boost::regex mReg;
    std::vector<std::string> mKeys;
    bool mIsWholeLineMode;
    UserDefinedFormat(const boost::regex& reg, const std::vector<std::string>& keys, bool isWholeLineMode)
        : mReg(reg), mKeys(keys), mIsWholeLineMode(isWholeLineMode) {}
};

enum ParseLogError {
    PARSE_LOG_REGEX_ERROR,
    PARSE_LOG_FORMAT_ERROR,
    PARSE_LOG_TIMEFORMAT_ERROR,
    PARSE_LOG_HISTORY_ERROR
};

class LogParser {
public:
    static const char* UNMATCH_LOG_KEY;

    static time_t
    ApsaraEasyReadLogTimeParser(const char* buffer, std::string& timeStr, time_t& lastLogTime, int64_t& microTime);
    static bool ApsaraEasyReadLogLineParser(const char* buffer,
                                            sls_logs::LogGroup& logGroup,
                                            bool discardUnmatch,
                                            std::string& timeStr,
                                            time_t& lastLogTime,
                                            const std::string& projectName,
                                            const std::string& category,
                                            const std::string& region,
                                            const std::string& logPath,
                                            ParseLogError& error,
                                            uint32_t& logGroupSize);
    static time_t ApsaraEasyParseLogTimeParser(const char* buffer);

    static bool WholeLineModeParser(const char* buffer,
                                    sls_logs::LogGroup& logGroup,
                                    const std::string& key,
                                    time_t logTime,
                                    uint32_t& logGroupSize);

    // RegexLogLineParser parses @buffer according to @reg.
    // Log time parsing: use @timeIndex to decide which field should be considered
    // as log time, and @timeFormat is used to parse it (strptime). @timeStr and
    // @logTime is the parsed result in string and time_t format.
    static bool RegexLogLineParser(const char* buffer,
                                   const boost::regex& reg,
                                   sls_logs::LogGroup& logGroup,
                                   bool discardUnmatch,
                                   const std::vector<std::string>& keys,
                                   const std::string& category,
                                   const char* timeFormat,
                                   const PreciseTimestampConfig& preciseTimestampConfig,
                                   const uint32_t timeIndex,
                                   std::string& timeStr,
                                   time_t& logTime,
                                   int32_t specifiedYear,
                                   const std::string& projectName,
                                   const std::string& region,
                                   const std::string& logPath,
                                   ParseLogError& error,
                                   uint32_t& logGroupSize);
    // RegexLogLineParser with specified log time.
    static bool RegexLogLineParser(const char* buffer,
                                   const boost::regex& reg,
                                   sls_logs::LogGroup& logGroup,
                                   bool discardUnmatch,
                                   const std::vector<std::string>& keys,
                                   const std::string& category,
                                   time_t logTime,
                                   const std::string& projectName,
                                   const std::string& region,
                                   const std::string& logPath,
                                   ParseLogError& error,
                                   uint32_t& logGroupSize);

    static void AddLog(sls_logs::Log* logPtr, const std::string& key, const std::string& value, uint32_t& logGroupSize);

    static int32_t GetApsaraLogMicroTime(const char* buffer);

    static bool IsPrefixString(const std::string& all, const std::string& prefix);

    static bool IsPrefixString(const char* all, const std::string& prefix);

    static void AddUnmatchLog(const char* buffer, sls_logs::LogGroup& logGroup, uint32_t& logGroupSize);

    static bool ParseLogTime(const char* buffer,
                             std::string& timeStr,
                             time_t& logTime,
                             uint64_t& preciseTimestamp,
                             const std::string& curTimeStr,
                             const char* timeFormat,
                             const PreciseTimestampConfig& preciseTimestampConfig,
                             int32_t specifiedYear,
                             const std::string& projectName,
                             const std::string& category,
                             const std::string& region,
                             const std::string& logPath,
                             ParseLogError& error);

    static void AdjustLogTime(sls_logs::Log* logPtr, int logTimeZoneOffsetSecond, int localTimeZoneOffsetSecond);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogParserUnittest;
#endif
};

} // namespace logtail
