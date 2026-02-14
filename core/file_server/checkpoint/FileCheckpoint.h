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
    // 容器场景下与文件绑定的元信息
    struct ContainerMeta {
        std::string mContainerID;
        std::string mContainerName;
        std::string mPodName;
        std::string mNamespace;
        std::string mContainerPath; // 容器内路径
    };

    std::filesystem::path mFilePath;
    DevInode mDevInode;
    uint64_t mSignatureHash = 0;
    uint32_t mSignatureSize = 0;
    uint64_t mSize = 0; // 初始文件大小，用于限制 StaticFileServer reader 的读取范围，保持不变
    uint64_t mOffset = 0;
    FileStatus mStatus = FileStatus::WAITING;
    int32_t mStartTime = 0;
    int32_t mLastUpdateTime = 0;
    ContainerMeta mContainerMeta; // 容器场景下使用，非容器时各字段为空

    FileCheckpoint() = default;
    FileCheckpoint(const std::filesystem::path& filename,
                   const DevInode& devInode,
                   uint64_t signatureHash,
                   uint32_t signatureSize,
                   uint64_t initialSize = 0,
                   const ContainerMeta& containerMeta = {})
        : mFilePath(filename),
          mDevInode(devInode),
          mSignatureHash(signatureHash),
          mSignatureSize(signatureSize),
          mSize(initialSize),
          mContainerMeta(containerMeta) {}
};

struct FileFingerprint {
    std::filesystem::path mFilePath;
    DevInode mDevInode;
    uint32_t mSignatureSize;
    uint64_t mSignatureHash;
    uint64_t mSize = 0; // 初始文件大小，用于限制 StaticFileServer reader 的读取范围
    uint64_t mOffset = 0;
    std::string mContainerID; // 容器场景下当前文件所属容器 ID，非容器为空；用于读文件时校验容器是否变更/停止
};

} // namespace logtail
