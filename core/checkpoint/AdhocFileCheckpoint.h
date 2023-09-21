/*
 * Copyright 2023 iLogtail Authors
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
#include <memory>
#include <string>
#include <mutex>
#include "common/DevInode.h"
#include "common/StringTools.h"

namespace logtail {

enum FileReadStatus {
    STATUS_WAITING,
    STATUS_LOADING,
    STATUS_FINISHED,
    STATUS_LOST,
};

std::string TransStatusToString(FileReadStatus status);
FileReadStatus GetStatusFromString(std::string statusStr);

class AdhocFileCheckpoint {
private:
    /* data */
public:
    AdhocFileCheckpoint() {}

    AdhocFileCheckpoint(const std::string& filename,
                        int64_t size,
                        int64_t offset,
                        uint32_t signatureSize,
                        uint64_t signatureHash,
                        const DevInode& devInode,
                        FileReadStatus status = STATUS_WAITING,
                        const std::string& jobName = std::string(),
                        const std::string& realFileName = std::string())
        : mFileName(filename),
          mSize(size),
          mOffset(offset),
          mSignatureSize(signatureSize),
          mSignatureHash(signatureHash),
          mDevInode(devInode),
          mStatus(status),
          mJobName(jobName),
          mRealFileName(realFileName) {}

    std::string mFileName;
    int64_t mSize;
    int64_t mOffset;
    uint32_t mSignatureSize;
    uint64_t mSignatureHash;
    DevInode mDevInode;
    FileReadStatus mStatus;
    std::string mJobName;
    std::string mRealFileName;
    int32_t mStartTime;
    int32_t mLastUpdateTime;
};

struct AdhocFileKey {
    AdhocFileKey() {}
    AdhocFileKey(const DevInode& devInode, uint32_t signatureSize, uint64_t signatureHash)
        : mDevInode(devInode), mSignatureSize(signatureSize), mSignatureHash(signatureHash) {}
    DevInode mDevInode;
    uint32_t mSignatureSize;
    uint64_t mSignatureHash;
};

typedef std::shared_ptr<AdhocFileCheckpoint> AdhocFileCheckpointPtr;

} // namespace logtail