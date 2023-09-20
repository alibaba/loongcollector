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
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

namespace logtail {

class JsonLogFileReader : public LogFileReader {
public:
    JsonLogFileReader(const std::string& projectName,
                      const std::string& category,
                      const std::string& hostLogPathDir,
                      const std::string& hostLogPathFile,
                      int32_t tailLimit,
                      const std::string& timeFormat,
                      const std::string& topicFormat,
                      const std::string& groupTopic,
                      FileEncoding fileEncoding,
                      bool discardUnmatch,
                      bool dockerFileFlag = false);

    void SetTimeKey(const std::string& timeKey);
    bool LogSplit(const char* buffer,
                  int32_t size,
                  int32_t& lineFeed,
                  std::vector<StringView>& logIndex,
                  std::vector<StringView>& discardIndex) override;

protected:
    bool ParseLogLine(StringView buffer,
                      sls_logs::LogGroup& logGroup,
                      ParseLogError& error,
                      LogtailTime& lastLogLineTime,
                      std::string& lastLogTimeStr,
                      uint32_t& logGroupSize) override;

    int32_t
    LastMatchedLine(char* buffer, int32_t size, int32_t& rollbackLineFeedCount, bool allowRollback = true) override;

private:
    bool FindJsonMatch(
        char* buffer, int32_t beginIdx, int32_t size, int32_t& endIdx, bool& startWithBlock, bool allowRollback = true);
    std::string RapidjsonValueToString(const rapidjson::Value& value);

    std::string mTimeKey;
    std::string mTimeFormat;
    bool mUseSystemTime;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class JsonLogFileReaderUnittest;
    friend class JsonParseLogLineUnittest;
    friend class LastMatchedLineUnittest;
#endif
};

} // namespace logtail
