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

#include "file_server/reader/JsonLogFileReader.h"

#include "logger/Logger.h"

using namespace std;

namespace logtail {
int32_t JsonLogFileReader::RemoveLastIncompleteLog(char* buffer,
                                                   int32_t size,
                                                   int32_t& rollbackLineFeedCount,
                                                   bool allowRollback) {
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
                  ("invalid json begining", buffer + beginIdx)("project", mReaderConfig.second->GetProjectName())(
                      "logstore", mReaderConfig.second->GetLogstoreName())("file", mHostLogPath));
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
                    LOG_WARNING(sLogger,
                                ("brace count", braceCount)("brace not match, invalid json begining",
                                                            buffer + beginIdx)("project",
                                                                               mReaderConfig.second->GetProjectName())(
                                    "logstore", mReaderConfig.second->GetLogstoreName())("file", mHostLogPath));
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
              ("find no match, beginIdx", beginIdx)("idx", idx)("project", mReaderConfig.second->GetProjectName())(
                  "logstore", mReaderConfig.second->GetLogstoreName())("file", mHostLogPath));
    return false;
}

} // namespace logtail
