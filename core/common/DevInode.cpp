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

#include "DevInode.h"
#include <errno.h>
#if defined(__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "logger/Logger.h"

namespace logtail {

// TODO: Remove it, it seems that nobody call it now.
uint64_t GetFileInode(const std::string& filePath) {
#if defined(__linux__)
    struct stat buf;
    if (stat(filePath.c_str(), &buf) != 0) // if symbolic link, return the referred inode
    {
        if (errno != EEXIST)
            LOG_WARNING(sLogger, ("call stat() on file fail", filePath)("error", strerror(errno)));
        else
            LOG_DEBUG(sLogger, ("call stat() on file fail", filePath)("error", strerror(errno)));
        return NO_BLOCK_INODE;
    }
    return buf.st_ino;
#elif defined(_MSC_VER)
    return 0;
#endif
}

DevInode GetFileDevInode(const std::string& filePath) {
#if defined(__linux__)
    struct stat buf;
    if (stat(filePath.c_str(), &buf) != 0) // if symbolic link, return the referred inode
    {
        if (errno != EEXIST)
            LOG_WARNING(sLogger, ("call stat() on file fail", filePath)("error", strerror(errno)));
        else
            LOG_DEBUG(sLogger, ("call stat() on file fail", filePath)("error", strerror(errno)));
        return DevInode();
    }
    return DevInode(buf.st_dev, buf.st_ino);
#elif defined(_MSC_VER)
    HANDLE hFile = CreateFile(filePath.c_str(),
                              FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS,
                              NULL);
    if (INVALID_HANDLE_VALUE == hFile) {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_FILE_NOT_FOUND)
            LOG_ERROR(sLogger, ("File not exist", filePath));
        else
            LOG_ERROR(sLogger, ("Open file failed", filePath)("errno", lastError));
        return DevInode();
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi)) {
        CloseHandle(hFile);
        LOG_ERROR(sLogger, ("GetFileInformationByHandle failed", filePath)("errno", GetLastError()));
        return DevInode();
    }
    CloseHandle(hFile);
    return DevInode(fi.dwVolumeSerialNumber, (static_cast<uint64_t>(fi.nFileIndexHigh) << 32) | fi.nFileIndexLow);
#endif
}

} // namespace logtail