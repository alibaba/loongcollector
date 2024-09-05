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

#include "file_server/reader/LogFileReader.h"

namespace logtail {

class JsonLogFileReader : public LogFileReader {
public:
    JsonLogFileReader(const std::string& hostLogPathDir,
                      const std::string& hostLogPathFile,
                      const DevInode& devInode,
                      const FileReaderConfig& readerConfig,
                      const MultilineConfig& multilineConfig)
        : LogFileReader(hostLogPathDir, hostLogPathFile, devInode, readerConfig, multilineConfig) {}

protected:
    int32_t RemoveLastIncompleteLog(char* buffer,
                                    int32_t size,
                                    int32_t& rollbackLineFeedCount,
                                    bool allowRollback = true) override;

private:
    bool FindJsonMatch(
        char* buffer, int32_t beginIdx, int32_t size, int32_t& endIdx, bool& startWithBlock, bool allowRollback = true);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class JsonLogFileReaderUnittest;
    friend class JsonParseLogLineUnittest;
    friend class RemoveLastIncompleteLogUnittest;
#endif
};

} // namespace logtail
