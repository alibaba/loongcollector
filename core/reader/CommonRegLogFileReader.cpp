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

#include "reader/CommonRegLogFileReader.h"
#include "common/Constants.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"

using namespace sls_logs;

namespace logtail {

CommonRegLogFileReader::CommonRegLogFileReader(const std::string& projectName,
                                               const std::string& category,
                                               const std::string& logPathDir,
                                               const std::string& logPathFile,
                                               int32_t tailLimit,
                                               const std::string& timeFormat,
                                               const std::string& topicFormat,
                                               const std::string& groupTopic /* = "" */,
                                               FileEncoding fileEncoding /* = ENCODING_UTF8 */,
                                               bool discardUnmatch /* = true */,
                                               bool dockerFileFlag /* = true */)
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
    mLogType = REGEX_LOG;
    mTimeFormat = timeFormat;
    mTimeKey = "time";
}

void CommonRegLogFileReader::SetTimeKey(const std::string& timeKey) {
    if (!timeKey.empty()) {
        mTimeKey = timeKey;
    }
}

bool CommonRegLogFileReader::AddUserDefinedFormat(const std::string& regStr, const std::string& keys) {
    std::vector<std::string> keyParts = StringSpliter(keys, ",");
    boost::regex reg(regStr);
    bool isWholeLineMode = regStr == "(.*)";
    mUserDefinedFormat.push_back(UserDefinedFormat(reg, keyParts, isWholeLineMode));
    int32_t index = -1;
    for (size_t i = 0; i < keyParts.size(); i++) {
        if (ToLowerCaseString(keyParts[i]) == mTimeKey) {
            index = i;
            break;
        }
    }
    mTimeIndex.push_back(index);
    return true;
}

bool CommonRegLogFileReader::ParseLogLine(StringView buffer,
                                          LogGroup& logGroup,
                                          ParseLogError& error,
                                          LogtailTime& lastLogLineTime,
                                          std::string& lastLogTimeStr,
                                          uint32_t& logGroupSize) {
    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }

    for (uint32_t i = 0; i < mUserDefinedFormat.size(); ++i) {
        const UserDefinedFormat& format = mUserDefinedFormat[i];
        bool res = true;
        if (mTimeIndex[i] >= 0 && !mTimeFormat.empty()) {
            res = LogParser::RegexLogLineParser(buffer,
                                                format.mReg,
                                                logGroup,
                                                mDiscardUnmatch,
                                                format.mKeys,
                                                mCategory,
                                                mTimeFormat.c_str(),
                                                mPreciseTimestampConfig,
                                                mTimeIndex[i],
                                                lastLogTimeStr,
                                                lastLogLineTime,
                                                mSpecifiedYear,
                                                mProjectName,
                                                mRegion,
                                                mHostLogPath,
                                                error,
                                                logGroupSize,
                                                mTzOffsetSecond);
        } else {
            // if "time" field not exist in user config or timeformat empty, set current system time for logs
            LogtailTime ts = GetCurrentLogtailTime();
            if (format.mIsWholeLineMode) {
                res = LogParser::WholeLineModeParser(
                    buffer, logGroup, format.mKeys.empty() ? DEFAULT_CONTENT_KEY : format.mKeys[0], ts, logGroupSize);
            } else {
                res = LogParser::RegexLogLineParser(buffer,
                                                    format.mReg,
                                                    logGroup,
                                                    mDiscardUnmatch,
                                                    format.mKeys,
                                                    mCategory,
                                                    ts,
                                                    mProjectName,
                                                    mRegion,
                                                    mHostLogPath,
                                                    error,
                                                    logGroupSize);
            }
        }
        if (res) {
            return true;
        }
    }

    return false;
}

} // namespace logtail