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
#include "profiler/LogtailAlarm.h"
#include "parser/LogParser.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"

using namespace sls_logs;
using namespace logtail;
using namespace std;

JsonLogFileReader::JsonLogFileReader(const std::string& projectName,
                                     const std::string& category,
                                     const std::string& logPathDir,
                                     const std::string& logPathFile,
                                     int32_t tailLimit,
                                     const std::string& timeFormat,
                                     const std::string& topicFormat,
                                     const std::string& groupTopic,
                                     FileEncoding fileEncoding,
                                     bool discardUnmatch,
                                     bool dockerFileFlag)
    : LogFileReader(projectName,
                    category,
                    logPathDir,
                    logPathFile,
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

bool JsonLogFileReader::ParseLogLine(const char* buffer,
                                     sls_logs::LogGroup& logGroup,
                                     ParseLogError& error,
                                     time_t& lastLogLineTime,
                                     std::string& lastLogTimeStr,
                                     uint32_t& logGroupSize) {
    if (strlen(buffer) == 0)
        return true;

    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }
    bool parseSuccess = true;
    uint64_t preciseTimestamp = 0;
    rapidjson::Document doc;
    doc.Parse(buffer);
    if (doc.HasParseError()) {
        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(sLogger,
                        ("parse json log fail, log",
                         buffer)("rapidjson offset", doc.GetErrorOffset())("rapidjson error", doc.GetParseError())(
                            "project", mProjectName)("logstore", mCategory)("file", mLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(
                PARSE_LOG_FAIL_ALARM, string("parse json fail:") + string(buffer), mProjectName, mCategory, mRegion);
        }
        error = PARSE_LOG_FORMAT_ERROR;
        parseSuccess = false;
    } else if (!doc.IsObject()) {
        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
            LOG_WARNING(
                sLogger,
                ("invalid json object, log", buffer)("project", mProjectName)("logstore", mCategory)("file", mLogPath));
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                   string("invalid json object:") + string(buffer),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
        }
        error = PARSE_LOG_FORMAT_ERROR;
        parseSuccess = false;
    } else if (!mUseSystemTime) {
        rapidjson::Value::ConstMemberIterator itr = doc.FindMember(mTimeKey.c_str());
        if (itr != doc.MemberEnd() && (itr->value.IsString() || itr->value.IsInt64())) {
            if (!LogParser::ParseLogTime(buffer,
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
                                         mLogPath,
                                         error)) {
                parseSuccess = false;
                if (error == PARSE_LOG_HISTORY_ERROR)
                    return false;
            }
        } else {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse json log fail, log", buffer)("invalid time key", mTimeKey)("project", mProjectName)(
                                "logstore", mCategory)("file", mLogPath));
                LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                       string("found no time_key: ") + mTimeKey
                                                           + ", log:" + string(buffer),
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
        logPtr->set_time(mUseSystemTime ? time(NULL) : lastLogLineTime);
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
        return true;
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

vector<int32_t> JsonLogFileReader::LogSplit(char* buffer, int32_t size, int32_t& lineFeed) {
    vector<int32_t> index;
    int32_t i = 0;
    index.push_back(i);
    while (++i < size) {
        if (buffer[i] == '\0' && i < size - 1)
            index.push_back(i + 1);
    }
    return index;
}

// rollbackLineFeedCount is useful in encoding conversion, ingnore it in json reader(should be utf8)
// split log & find last match in this function
int32_t JsonLogFileReader::LastMatchedLine(char* buffer, int32_t size, int32_t& rollbackLineFeedCount) {
    int32_t readBytes = 0;
    int32_t endIdx = 0;
    int32_t beginIdx = 0;
    rollbackLineFeedCount = 0;
    // check if has json block in this buffer
    bool noJsonBlockFlag = true;
    do {
        bool startWithBlock = false;
        if (FindJsonMatch(buffer, beginIdx, size, endIdx, startWithBlock)) {
            noJsonBlockFlag = false;
            buffer[endIdx] = '\0';
            readBytes = endIdx + 1;
            beginIdx = endIdx + 1;
            rollbackLineFeedCount = 0;
        } else {
            char* pos = strchr(buffer + beginIdx, '\n');
            if (pos == NULL)
                break;
            ++rollbackLineFeedCount;
            endIdx = pos - buffer;
            buffer[endIdx] = '\0'; //invalid json line
            beginIdx = endIdx + 1;
            if (noJsonBlockFlag) {
                // if no json block in this buffer and now line has no block, forse read this line
                // if no json block in this buffer and now line has block, set noJsonBlockFlag false
                if (!startWithBlock) {
                    readBytes = endIdx + 1;
                } else {
                    noJsonBlockFlag = false;
                }
            }
        }
    } while (beginIdx < size);
    return readBytes;
}

bool JsonLogFileReader::FindJsonMatch(
    char* buffer, int32_t beginIdx, int32_t size, int32_t& endIdx, bool& startWithBlock) {
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
                  ("invalid json begining", buffer + beginIdx)("project", mProjectName)("logstore",
                                                                                        mCategory)("file", mLogPath));
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
                            "project", mProjectName)("logstore", mCategory)("file", mLogPath));
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
                ++idx; //skip next char after escape char
                break;
            default:
                break;
        }
    }
    LOG_DEBUG(sLogger,
              ("find no match, beginIdx", beginIdx)("idx", idx)("project", mProjectName)("logstore",
                                                                                         mCategory)("file", mLogPath));
    return false;
}
