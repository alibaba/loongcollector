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
#include <string>
#include <memory>
#include "common/DevInode.h"
#include "common/StringTools.h"

namespace logtail {

struct FileInfo {
    FileInfo(int fd = -1,
             DevInode devInode = DevInode(),
             const std::string& filename = "",
             int64_t offset = 0,
             int64_t len = 0,
             int64_t filePos = 0,
             int64_t readPos = 0,
             int64_t size = 0)
        : filename(filename),
          fd(fd),
          devInode(devInode),
          offset(offset),
          len(len),
          filePos(filePos),
          readPos(readPos),
          fileSize(size) {}

    FileInfo(const FileInfo& rhs)
        : filename(rhs.filename),
          fd(rhs.fd),
          devInode(rhs.devInode),
          offset(rhs.offset),
          len(rhs.len),
          filePos(rhs.filePos),
          readPos(rhs.readPos),
          fileSize(rhs.fileSize) {}

    FileInfo& operator=(const FileInfo& rhs) {
        if (this != &rhs) {
            filename = rhs.filename;
            fd = rhs.fd;
            devInode = rhs.devInode;
            offset = rhs.offset;
            len = rhs.len;
            filePos = rhs.filePos;
            readPos = rhs.readPos;
            fileSize = rhs.fileSize;
        }

        return *this;
    }

    // key: fd + "_" + dev + "_" + inode
    std::string GetKey() const {
        std::string str(ToString(fd));
        return str.append("_").append(ToString(devInode.dev)).append("_").append(ToString(devInode.inode));
    }

    // filename is normally absolute patth
    // if under fuse mode, filename is not absolute path
    // e.g. logPath is /home/admin/logs, filePattern is *.log
    // if a file's absolute path is /home/admin/logs/a/b/xxx.log
    // then file name here is a/b/xxx.log
    std::string filename;

    // use fd to sparsify file instead of filename, which may cause error while log file rotates
    int fd;

    // use dev inode to mark file when log file rotates
    DevInode devInode;

    int64_t offset;
    int64_t len;

    // log file reader travers back from read pos until it meets '\n'
    // normally, file pos <= read pos
    int64_t filePos;
    // log file reader ever reaches may contain lines without '\n'
    int64_t readPos;
    int64_t fileSize;
};

struct TruncateInfo {
    TruncateInfo(int64_t oriOffset = 0, int64_t curOffset = 0) : oriOffset(oriOffset), curOffset(curOffset) {}

    void clear() {
        oriOffset = 0;
        curOffset = 0;
    }

    bool empty() const { return (oriOffset == 0 && curOffset == 0); }

    std::string toString() const {
        std::string info;
        info.append("[");
        info.append(ToString(oriOffset));
        info.append("-");
        info.append(ToString(curOffset));
        info.append("]");
        return info;
    }

    int64_t oriOffset;
    int64_t curOffset;
};

typedef std::shared_ptr<FileInfo> FileInfoPtr;
typedef std::shared_ptr<TruncateInfo> TruncateInfoPtr;

} // namespace logtail
