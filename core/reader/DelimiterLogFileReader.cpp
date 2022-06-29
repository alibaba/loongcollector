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

#include "DelimiterLogFileReader.h"
#include <string.h>
#include <ctime>
#include "profiler/LogtailAlarm.h"
#include "parser/LogParser.h"
#include "parser/DelimiterModeFsmParser.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"

using namespace sls_logs;
using namespace logtail;
using namespace std;

const std::string DelimiterLogFileReader::s_mDiscardedFieldKey = "_";

DelimiterLogFileReader::DelimiterLogFileReader(const std::string& projectName,
                                               const std::string& category,
                                               const std::string& logPathDir,
                                               const std::string& logPathFile,
                                               int32_t tailLimit,
                                               const std::string& timeFormat,
                                               const std::string& topicFormat,
                                               const std::string& groupTopic,
                                               FileEncoding fileEncoding,
                                               const std::string& separator,
                                               char quote,
                                               bool autoExtend,
                                               bool discardUnmatch,
                                               bool dockerFileFlag,
                                               bool acceptNoEnoughKeys,
                                               bool extractPartialFields)
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
    mLogType = DELIMITER_LOG;
    mTimeFormat = timeFormat;
    mSeparator = separator;
    if (!separator.empty())
        mSeparatorChar = mSeparator.data()[0];
    else {
        // This should never happened.
        mSeparatorChar = '\t';
    }
    mQuote = quote;
    mTimeIndex = 0;
    mAutoExtend = autoExtend;
    mUseSystemTime = true;
    mAcceptNoEnoughKeys = acceptNoEnoughKeys;
    mExtractPartialFields = extractPartialFields;
    mDelimiterModeFsmParserPtr = new DelimiterModeFsmParser(mQuote, mSeparatorChar);
}

DelimiterLogFileReader::~DelimiterLogFileReader() {
    delete mDelimiterModeFsmParserPtr;
}

void DelimiterLogFileReader::SetColumnKeys(const std::vector<std::string>& columnKeys, const std::string& timeKey) {
    mUseSystemTime = true;
    mColumnKeys = columnKeys;
    mTimeIndex = 0;
    if (timeKey.empty() || mTimeFormat.empty())
        return;

    for (uint32_t idx = 0; idx != mColumnKeys.size(); idx++) {
        if (mColumnKeys[idx] == timeKey) {
            mTimeIndex = idx;
            mUseSystemTime = false;
            return;
        }
    }
}

bool DelimiterLogFileReader::ParseLogLine(const char* buffer,
                                          sls_logs::LogGroup& logGroup,
                                          ParseLogError& error,
                                          time_t& lastLogLineTime,
                                          std::string& lastLogTimeStr,
                                          uint32_t& logGroupSize) {
    int32_t endIdx = strlen(buffer);
    if (endIdx == 0)
        return true;

    for (int32_t i = endIdx - 1; i >= 0; --i) {
        if (buffer[i] == ' ' || '\r' == buffer[i])
            endIdx = i;
        else
            break;
    }
    int32_t begIdx = 0;
    for (int32_t i = 0; i < endIdx; ++i) {
        if (buffer[i] == ' ')
            begIdx = i + 1;
        else
            break;
    }

    if (begIdx >= endIdx)
        return true;

    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }
    size_t reserveSize = mAutoExtend ? (mColumnKeys.size() + 10) : (mColumnKeys.size() + 1);
    vector<string> columnValues;
    std::vector<size_t> colBegIdxs;
    std::vector<size_t> colLens;
    bool parseSuccess = false;
    uint64_t preciseTimestamp = 0;
    size_t parsedColCount = 0;
    bool useQuote = (mSeparator.size() == 1) && (mQuote != mSeparatorChar);
    if (mColumnKeys.size() > 0) {
        if (useQuote) {
            columnValues.reserve(reserveSize);
            parseSuccess = mDelimiterModeFsmParserPtr->ParseDelimiterLine(buffer, begIdx, endIdx, columnValues);
            // handle auto extend
            if (!mAutoExtend && columnValues.size() > mColumnKeys.size()) {
                std::string extraFields;
                for (size_t i = mColumnKeys.size(); i < columnValues.size(); ++i) {
                    extraFields.append(1, mSeparatorChar).append(columnValues[i]);
                }
                // remove extra fields
                columnValues.resize(mColumnKeys.size());
                columnValues.push_back(extraFields);
            }
            parsedColCount = columnValues.size();
        } else {
            colBegIdxs.reserve(reserveSize);
            colLens.reserve(reserveSize);
            parseSuccess = SplitString(buffer, begIdx, endIdx, colBegIdxs, colLens);
            parsedColCount = colBegIdxs.size();
        }

        if (parseSuccess) {
            if (parsedColCount <= 0 || (!mAcceptNoEnoughKeys && parsedColCount < mColumnKeys.size())) {
                LOG_WARNING(sLogger,
                            ("parse delimiter log fail, keys count unmatch "
                             "columns count, parsed",
                             parsedColCount)("required", mColumnKeys.size())("log", buffer)("project", mProjectName)(
                                "logstore", mCategory)("file", mLogPath));
                LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                       string("keys count unmatch columns count :")
                                                           + ToString(parsedColCount) + ", required:"
                                                           + ToString(mColumnKeys.size()) + ", logs:" + string(buffer),
                                                       mProjectName,
                                                       mCategory,
                                                       mRegion);
                error = PARSE_LOG_FORMAT_ERROR;
                parseSuccess = false;
            } else if (!mUseSystemTime && parsedColCount > mTimeIndex) {
                if (!LogParser::ParseLogTime(buffer,
                                             lastLogTimeStr,
                                             lastLogLineTime,
                                             preciseTimestamp,
                                             useQuote ? columnValues[mTimeIndex]
                                                      : string(buffer + colBegIdxs[mTimeIndex], colLens[mTimeIndex]),
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
            }
        } else {
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                   string("parse delimiter log fail") + ", logs:" + string(buffer),
                                                   mProjectName,
                                                   mCategory,
                                                   mRegion);
            error = PARSE_LOG_FORMAT_ERROR;
            parseSuccess = false;
        }
    } else {
        LogtailAlarm::GetInstance()->SendAlarm(
            PARSE_LOG_FAIL_ALARM, "no column keys defined", mProjectName, mCategory, mRegion);
        LOG_WARNING(sLogger,
                    ("parse delimiter log fail",
                     "no column keys defined")("project", mProjectName)("logstore", mCategory)("file", mLogPath));
        error = PARSE_LOG_FORMAT_ERROR;
        parseSuccess = false;
    }

    if (parseSuccess) {
        Log* logPtr = logGroup.add_logs();
        logPtr->set_time(mUseSystemTime || lastLogLineTime <= 0 ? time(NULL) : lastLogLineTime);

        for (uint32_t idx = 0; idx < parsedColCount; idx++) {
            if (mColumnKeys.size() > idx) {
                if (mExtractPartialFields && mColumnKeys[idx] == s_mDiscardedFieldKey) {
                    continue;
                }

                LogParser::AddLog(logPtr,
                                  mColumnKeys[idx],
                                  useQuote ? columnValues[idx] : string(buffer + colBegIdxs[idx], colLens[idx]),
                                  logGroupSize);
            } else {
                if (mExtractPartialFields) {
                    continue;
                }

                LogParser::AddLog(logPtr,
                                  string("__column") + ToString(idx) + "__",
                                  useQuote ? columnValues[idx] : string(buffer + colBegIdxs[idx], colLens[idx]),
                                  logGroupSize);
            }
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

bool DelimiterLogFileReader::SplitString(
    const char* buffer, int32_t begIdx, int32_t endIdx, std::vector<size_t>& colBegIdxs, std::vector<size_t>& colLens) {
    if (endIdx <= begIdx || mSeparator.size() == 0 || mColumnKeys.size() == 0)
        return false;
    size_t size = endIdx - begIdx;
    size_t d_size = mSeparator.size();
    if (d_size == 0 || d_size > size) {
        colBegIdxs.push_back(begIdx);
        colLens.push_back(size);
        return true;
    }
    size_t pos = begIdx;
    size_t top = endIdx - d_size;
    while (pos <= top) {
        const char* pch = strstr(buffer + pos, mSeparator.c_str());
        size_t pos2 = pch == NULL ? endIdx : (pch - buffer);
        if (pos2 != pos) {
            colBegIdxs.push_back(pos);
            colLens.push_back(pos2 - pos);
        } else {
            colBegIdxs.push_back(pos);
            colLens.push_back(0);
        }
        if (pos2 == (size_t)endIdx)
            return true;
        pos = pos2 + d_size;
        if (colLens.size() >= mColumnKeys.size() && !mAutoExtend) {
            colBegIdxs.push_back(pos2);
            colLens.push_back(endIdx - pos2);
            return true;
        }
    }
    if (pos <= (size_t)endIdx) {
        colBegIdxs.push_back(pos);
        colLens.push_back(endIdx - pos);
    }
    return true;
}
