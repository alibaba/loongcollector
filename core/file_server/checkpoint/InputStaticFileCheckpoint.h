/*
 * Copyright 2025 iLogtail Authors
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

#include <cstdint>

#include <string>
#include <vector>

#include "json/json.h"

#include "file_server/checkpoint/FileCheckpoint.h"

namespace logtail {

enum class StaticFileReadingStatus {
    UNKNOWN,
    RUNNING,
    FINISHED,
    ABORT,
};

class InputStaticFileCheckpoint {
public:
    InputStaticFileCheckpoint() = default;
    InputStaticFileCheckpoint(const std::string& configName,
                              size_t idx,
                              std::vector<FileCheckpoint>&& fileCpts,
                              uint32_t startTime = 0,
                              uint32_t expireTime = 0);

    bool UpdateCurrentFileCheckpoint(uint64_t offset, uint64_t size, bool& needDump);
    bool InvalidateCurrentFileCheckpoint();
    bool GetCurrentFileFingerprint(FileFingerprint* cpt);
    void SetAbort();

    bool Serialize(std::string* res) const;
    bool Deserialize(const std::string& str, std::string* errMsg);
    bool SerializeToLogEvents() const;

    const std::string& GetConfigName() const { return mConfigName; }
    size_t GetInputIndex() const { return mInputIdx; }

private:
    std::string mConfigName;
    size_t mInputIdx = 0;
    std::vector<FileCheckpoint> mFileCheckpoints;
    size_t mCurrentFileIndex = 0;
    StaticFileReadingStatus mStatus = StaticFileReadingStatus::RUNNING;
    uint32_t mStartTime = 0;
    uint32_t mExpireTime = 0;
    uint32_t mFinishTime = 0; // 记录状态变为 FINISHED 或 ABORT 的时间
    mutable size_t mLastSentIndex = 0; // 跟踪上次发送到的位置

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputStaticFileCheckpointManagerUnittest;
    friend class StaticFileServerUnittest;
    friend class InputStaticFileUnittest;
#endif
};

} // namespace logtail
