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
    RUNNING,
    FINISHED,
    ABORT,
};

class InputStaticFileCheckpoint {
public:
    InputStaticFileCheckpoint() = default;
    InputStaticFileCheckpoint(const std::string& configName, size_t idx, std::vector<FileCheckpoint>&& fileCpts);

    bool UpdateCurrentFileCheckpoint(uint64_t offset, uint64_t size, bool& needDump);
    bool InvalidateCurrentFileCheckpoint();
    bool GetCurrentFileFingerprint(FileFingerprint* cpt);
    void SetAbort();

    bool Serialize(std::string* res) const;
    bool Deserialize(const std::string& str, std::string* errMsg);

    const std::string& GetConfigName() const { return mConfigName; }
    size_t GetInputIndex() const { return mInputIdx; }

private:
    std::string mConfigName;
    size_t mInputIdx = 0;
    std::vector<FileCheckpoint> mFileCheckpoints;
    size_t mCurrentFileIndex = 0;
    StaticFileReadingStatus mStatus = StaticFileReadingStatus::RUNNING;
};

} // namespace logtail
