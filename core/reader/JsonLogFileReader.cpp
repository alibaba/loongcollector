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

#include "JsonLogFileReader.h"
#include <ctime>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "common/LogtailCommonFlags.h"
#include "monitor/LogtailAlarm.h"
#include "parser/LogParser.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"

using namespace sls_logs;
using namespace logtail;
using namespace std;

JsonLogFileReader::JsonLogFileReader(const std::string& projectName,
                                     const std::string& category,
                                     const std::string& hostLogPathDir,
                                     const std::string& hostLogPathFile,
                                     int32_t tailLimit,
                                     const std::string& timeFormat,
                                     const std::string& topicFormat,
                                     const std::string& groupTopic,
                                     FileEncoding fileEncoding,
                                     bool discardUnmatch,
                                     bool dockerFileFlag)
    : LogFileReader(projectName,
                    category,
                    hostLogPathDir,
                    hostLogPathFile,
                    tailLimit,
                    topicFormat,
                    groupTopic,
                    fileEncoding,
                    discardUnmatch,
                    dockerFileFlag) {
    mLogType = JSON_LOG;
    mTimeFormat = timeFormat;
    mTimeKey.clear();
    mUseSystemTime = true;
}

void JsonLogFileReader::SetTimeKey(const std::string& timeKey) {
    mTimeKey = timeKey;
    if (timeKey.empty() || mTimeFormat.empty())
        mUseSystemTime = true;
    else
        mUseSystemTime = false;
}

bool JsonLogFileReader::ParseLogLine(StringView buffer,
                                     sls_logs::LogGroup& logGroup,
                                     ParseLogError& error,
                                     LogtailTime& lastLogLineTime,
                                     std::string& lastLogTimeStr,
                                     uint32_t& logGroupSize) {
    if (buffer.empty())
        return true;

    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }
    bool parseSuccess = true;
    uint64_t preciseTimestamp = 0;
    rapidjson::Document doc;
    doc.Parse(buffer.data(), buffer.size());
    if (doc.HasParseError()) {
        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("parse json log fail, log",
                         buffer)("rapidjson offset", doc.GetErrorOffset())("rapidjson error", doc.GetParseError())(
                            "project", mProjectName)("logstore", mCategory)("file", mHostLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                   string("parse json fail:") + buffer.to_string(),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
        }
        error = PARSE_LOG_FORMAT_ERROR;
        parseSuccess = false;
    } else if (!doc.IsObject()) {
        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("invalid json object, log", buffer)("project", mProjectName)("logstore",
                                                                                      mCategory)("file", mHostLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                   string("invalid json object:") + buffer.to_string(),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
        }
        error = PARSE_LOG_FORMAT_ERROR;
        parseSuccess = false;
    } else if (!mUseSystemTime) {
        rapidjson::Value::ConstMemberIterator itr = doc.FindMember(mTimeKey.c_str());
        if (itr != doc.MemberEnd() && (itr->value.IsString() || itr->value.IsInt64())) {
            if (!LogParser::ParseLogTime(buffer.data(),
                                         lastLogTimeStr,
                                         lastLogLineTime,
                                         preciseTimestamp,
                                         itr->value.IsString() ? itr->value.GetString()
                                                               : ToString(itr->value.GetInt64()),
                                         mTimeFormat.c_str(),
                                         mPreciseTimestampConfig,
                                         mSpecifiedYear,
                                         mProjectName,
                                         mCategory,
                                         mRegion,
                                         mHostLogPath,
                                         error,
                                         mTzOffsetSecond)) {
                parseSuccess = false;
                if (error == PARSE_LOG_HISTORY_ERROR)
                    return false;
            }
        } else {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse json log fail, log", buffer)("invalid time key", mTimeKey)("project", mProjectName)(
                                "logstore", mCategory)("file", mHostLogPath));
                LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                       string("found no time_key: ") + mTimeKey
                                                           + ", log:" + buffer.to_string(),
                                                       mProjectName,
                                                       mCategory,
                                                       mRegion);
            }
            error = PARSE_LOG_FORMAT_ERROR;
            parseSuccess = false;
        }
    }

    if (parseSuccess) {
        Log* logPtr = logGroup.add_logs();
        auto now = GetCurrentLogtailTime();
        if (mUseSystemTime) {
            SetLogTimeWithNano(logPtr, now.tv_sec, now.tv_nsec);
        } else {
            SetLogTimeWithNano(logPtr, lastLogLineTime.tv_sec, lastLogLineTime.tv_nsec);
        }
        for (rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
            const rapidjson::Value& contentKey = itr->name;
            const rapidjson::Value& contentValue = itr->value;
            LogParser::AddLog(
                logPtr, RapidjsonValueToString(contentKey), RapidjsonValueToString(contentValue), logGroupSize);
        }
        if (!mUseSystemTime && mPreciseTimestampConfig.enabled) {
            LogParser::AddLog(logPtr, mPreciseTimestampConfig.key, std::to_string(preciseTimestamp), logGroupSize);
        }
        return true;
    } else if (!mDiscardUnmatch) {
        LogParser::AddUnmatchLog(buffer, logGroup, logGroupSize);
    }
    return false;
}

std::string JsonLogFileReader::RapidjsonValueToString(const rapidjson::Value& value) {
    if (value.IsString())
        return value.GetString();
    else if (value.IsBool())
        return ToString(value.GetBool());
    else if (value.IsInt())
        return ToString(value.GetInt());
    else if (value.IsUint())
        return ToString(value.GetUint());
    else if (value.IsInt64())
        return ToString(value.GetInt64());
    else if (value.IsUint64())
        return ToString(value.GetUint64());
    else if (value.IsDouble())
        return ToString(value.GetDouble());
    else if (value.IsNull())
        return "";
    else // if (value.IsObject() || value.IsArray())
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return buffer.GetString();
    }
}

bool JsonLogFileReader::LogSplit(const char* buffer,
                                 int32_t size,
                                 int32_t& lineFeed,
                                 std::vector<StringView>& logIndex,
                                 std::vector<StringView>& discardIndex) {
    int32_t line_beg = 0, i = 0;
    while (++i < size) {
        if (buffer[i] == '\0') {
            logIndex.emplace_back(buffer + line_beg, i - line_beg);
            line_beg = i + 1;
        }
    }
    logIndex.emplace_back(buffer + line_beg, size - line_beg);
    return true;
}

// split log & find last match in this function
int32_t
JsonLogFileReader::LastMatchedLine(char* buffer, int32_t size, int32_t& rollbackLineFeedCount, bool allowRollback) {
    int32_t readBytes = 0;
    int32_t endIdx = 0;
    int32_t beginIdx = 0;
    rollbackLineFeedCount = 0;
    bool startWithBlock = false;
    // check if has json block in this buffer
    do {
        if (!FindJsonMatch(buffer, beginIdx, size, endIdx, startWithBlock, allowRollback)) {
            if (startWithBlock && allowRollback) { // may be the ending } has not been read, rollback
                break;
            }
            // else impossible to be a valid json line, advance and skip
            char* pos = strchr(buffer + beginIdx, '\n');
            if (pos == NULL) {
                break;
            }
            endIdx = pos - buffer;
        }
        // advance if json is valid or impossible to be valid
        beginIdx = endIdx + 1;
        buffer[endIdx] = '\0';
    } while (beginIdx < size);
    readBytes = beginIdx;

    if (allowRollback) {
        rollbackLineFeedCount = std::count(buffer + beginIdx, buffer + size, '\n');
        if (beginIdx < size && buffer[size - 1] != '\n') {
            ++rollbackLineFeedCount;
        }
    } else {
        return size;
    }
    return readBytes;
}

bool JsonLogFileReader::FindJsonMatch(
    char* buffer, int32_t beginIdx, int32_t size, int32_t& endIdx, bool& startWithBlock, bool allowRollback) {
    int32_t idx = beginIdx;
    while (idx < size) {
        if (buffer[idx] == ' ' || buffer[idx] == '\n' || buffer[idx] == '\t' || buffer[idx] == '\0')
            idx++;
        else
            break;
    }
    // return true when total buffer is empty
    if (beginIdx == 0 && idx == size && size > 0) {
        endIdx = size - 1;
        LOG_DEBUG(sLogger, ("empty json content, skip from ", beginIdx)("to", size));
        startWithBlock = false;
        return true;
    }
    if (idx == size || buffer[idx] != '{') {
        LOG_DEBUG(sLogger,
                  ("invalid json begining",
                   buffer + beginIdx)("project", mProjectName)("logstore", mCategory)("file", mHostLogPath));
        startWithBlock = false;
        return false;
    }
    startWithBlock = true;

    int32_t braceCount = 0;
    bool inQuote = false;
    for (; idx < size; ++idx) {
        switch (buffer[idx]) {
            case '{':
                if (!inQuote)
                    braceCount++;
                break;
            case '}':
                if (!inQuote)
                    braceCount--;
                if (braceCount < 0) {
                    LOG_WARNING(
                        sLogger,
                        ("brace count", braceCount)("brace not match, invalid json begining", buffer + beginIdx)(
                            "project", mProjectName)("logstore", mCategory)("file", mHostLogPath));
                    return false;
                }
                break;
            case '\n':
                if (braceCount == 0) {
                    endIdx = idx;
                    return true;
                }
                break;
            case '"':
                inQuote = !inQuote;
                break;
            case '\\':
                ++idx; // skip next char after escape char
                break;
            default:
                break;
        }
    }
    if (!allowRollback && braceCount == 0) {
        // when !allowRollback, we can return true because the tailing \n will be ignored in the next read
        endIdx = idx; // assert idx == size
        return true;
    }
    LOG_DEBUG(sLogger,
              ("find no match, beginIdx",
               beginIdx)("idx", idx)("project", mProjectName)("logstore", mCategory)("file", mHostLogPath));
    return false;
}
