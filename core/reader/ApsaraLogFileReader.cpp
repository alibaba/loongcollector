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

#include "reader/ApsaraLogFileReader.h"
#include "common/FileSystemUtil.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"

using namespace sls_logs;

namespace logtail {

ApsaraLogFileReader::ApsaraLogFileReader(const std::string& projectName,
                                         const std::string& category,
                                         const std::string& logPathDir,
                                         const std::string& logPathFile,
                                         int32_t tailLimit,
                                         const std::string topicFormat,
                                         const std::string& groupTopic,
                                         FileEncoding fileEncoding,
                                         bool discardUnmatch,
                                         bool dockerFileFlag)
    : LogFileReader(projectName, category, logPathDir, logPathFile, tailLimit, discardUnmatch, dockerFileFlag) {
    mLogType = APSARA_LOG;
    const std::string lowerConfig = ToLowerCaseString(topicFormat);
    if (lowerConfig == "none" || lowerConfig == "customized") {
        mTopicName = "";
    } else if (lowerConfig == "default") {
        size_t pos_dot = mHostLogPath.rfind("."); // the "." must be founded
        size_t pos = mHostLogPath.find("@");
        if (pos != std::string::npos) {
            size_t pos_slash = mHostLogPath.find(PATH_SEPARATOR, pos);
            if (pos_slash != std::string::npos) {
                mTopicName = mHostLogPath.substr(0, pos) + mHostLogPath.substr(pos_slash, pos_dot - pos_slash);
            }
        }
        if (mTopicName.empty()) {
            mTopicName = mHostLogPath.substr(0, pos_dot);
        }
        std::string lowTopic = ToLowerCaseString(mTopicName);
        std::string logSuffix = ".log";

        size_t suffixPos = lowTopic.rfind(logSuffix);
        if (suffixPos == lowTopic.size() - logSuffix.size()) {
            mTopicName = mTopicName.substr(0, suffixPos);
        }
    } else if (lowerConfig == "global_topic") {
        static LogtailGlobalPara* sGlobalPara = LogtailGlobalPara::Instance();
        mTopicName = sGlobalPara->GetTopic();
    } else if (lowerConfig == "group_topic")
        mTopicName = groupTopic;
    else
        mTopicName = GetTopicName(topicFormat, GetConvertedPath());
    mFileEncoding = fileEncoding;
}

bool ApsaraLogFileReader::ParseLogLine(StringView buffer,
                                       sls_logs::LogGroup& logGroup,
                                       ParseLogError& error,
                                       LogtailTime& lastLogLineTime,
                                       std::string& lastLogTimeStr,
                                       uint32_t& logGroupSize) {
    if (logGroup.logs_size() == 0) {
        logGroup.set_category(mCategory);
        logGroup.set_topic(mTopicName);
    }

    return LogParser::ApsaraEasyReadLogLineParser(buffer,
                                                  logGroup,
                                                  mDiscardUnmatch,
                                                  lastLogTimeStr,
                                                  lastLogLineTime.tv_sec,
                                                  mProjectName,
                                                  mCategory,
                                                  mRegion,
                                                  mHostLogPath,
                                                  error,
                                                  logGroupSize,
                                                  mTzOffsetSecond,
                                                  mAdjustApsaraMicroTimezone);
}

} // namespace logtail