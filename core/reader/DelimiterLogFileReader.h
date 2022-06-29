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
#include "LogFileReader.h"
#include <string>
#include <vector>

namespace logtail {

class DelimiterModeFsmParser;

class DelimiterLogFileReader : public LogFileReader {
public:
    DelimiterLogFileReader(const std::string& projectName,
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
                           bool dockerFileFlag = false,
                           bool acceptNoEnoughKeys = false,
                           bool extractPartialFields = false);
    ~DelimiterLogFileReader();

    void SetColumnKeys(const std::vector<std::string>& columnKeys, const std::string& timeKey);

protected:
    bool ParseLogLine(const char* buffer,
                      sls_logs::LogGroup& logGroup,
                      ParseLogError& error,
                      time_t& lastLogLineTime,
                      std::string& lastLogTimeStr,
                      uint32_t& logGroupSize);

    bool SplitString(const char* buffer,
                     int32_t begIdx,
                     int32_t endIdx,
                     std::vector<size_t>& colBegIdxs,
                     std::vector<size_t>& colLens);

    bool mUseSystemTime;
    char mQuote;
    std::string mSeparator;
    // We have to add an independent member to store the first char of mSeparator,
    // because it might be accessed by multiple processing threads, non-const version
    // of string operator[] might lead to data corruption.
    char mSeparatorChar;
    bool mAutoExtend;
    bool mAcceptNoEnoughKeys;
    std::string mTimeFormat;
    uint32_t mTimeIndex;
    std::vector<std::string> mColumnKeys;
    bool mExtractPartialFields;
    DelimiterModeFsmParser* mDelimiterModeFsmParserPtr;

    static const std::string s_mDiscardedFieldKey;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogFileReaderUnittest;
#endif
};

} // namespace logtail
