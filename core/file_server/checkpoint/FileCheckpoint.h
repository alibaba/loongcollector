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

#include <filesystem>
#include <string>

#include "common/DevInode.h"

namespace logtail {

enum class FileStatus {
    UNKNOWN,
    WAITING,
    READING,
    FINISHED,
    ABORT,
};

const std::string& FileStatusToString(FileStatus status);
FileStatus GetFileStatusFromString(const std::string& status);

struct FileCheckpoint {
    std::filesystem::path mFilePath;
    // std::string mRealFileName;
    DevInode mDevInode;
    uint64_t mSignatureHash = 0;
    uint32_t mSignatureSize = 0;
    uint64_t mSize = 0;
    uint64_t mOffset = 0;
    FileStatus mStatus = FileStatus::WAITING;
    int32_t mStartTime = 0;
    int32_t mLastUpdateTime = 0;

    FileCheckpoint() = default;
    FileCheckpoint(const std::filesystem::path& filename,
                   const DevInode& devInode,
                   uint64_t signatureHash,
                   uint32_t signatureSize)
        : mFilePath(filename), mDevInode(devInode), mSignatureHash(signatureHash), mSignatureSize(signatureSize) {}
};

struct FileFingerprint {
    std::filesystem::path mFilePath;
    DevInode mDevInode;
    uint32_t mSignatureSize;
    uint64_t mSignatureHash;
};

} // namespace logtail
