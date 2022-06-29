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

#include "LogFileOperator.h"
#if defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#endif
#include "FileSystemUtil.h"
#include "fuse/ulogfslib_file.h"

namespace logtail {

int LogFileOperator::Open(const char* path, bool fuseMode) {
    if (!path || IsOpen()) {
        return -1;
    }
    mFuseMode = fuseMode;

    if (mFuseMode) {
        mFd = ulogfs_open(path);
    } else {
#if defined(_MSC_VER)
        auto hFile = CreateFile(path,
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
        if (INVALID_HANDLE_VALUE == hFile) {
            return -1;
        }
        mFile = hFile;
        // Might conflict, but can make sure that mFd >= 0.
        mFd = (reinterpret_cast<std::intptr_t>(hFile)) & std::numeric_limits<int>::max();
#else
        mFd = open(path, O_RDONLY);
#endif
    }
    return mFd;
}

int64_t LogFileOperator::Seek(int64_t offset, int whence) {
    if (!IsOpen()) {
        return -1;
    }
    if (mFuseMode) {
        return ulogfs_seek(mFd, offset, whence);
    } else {
#if defined(_MSC_VER)
        switch (whence) {
            case SEEK_CUR:
                whence = FILE_CURRENT;
                break;
            case SEEK_SET:
                whence = FILE_BEGIN;
                break;
            case SEEK_END:
                whence = FILE_END;
                break;
            default:
                return -1;
        }
        LARGE_INTEGER liPos;
        liPos.QuadPart = offset;
        LARGE_INTEGER liNewPos{0};
        if (FALSE == SetFilePointerEx(mFile, liPos, &liNewPos, whence)) {
            return -1;
        }
        return liNewPos.QuadPart;
#else
        return lseek(mFd, offset, whence);
#endif
    }
}

int LogFileOperator::Stat(fsutil::PathStat& ps) const {
    if (!IsOpen()) {
        return -1;
    }

    if (mFuseMode) {
        return ulogfs_stat(mFd, ps.GetRawStat());
    } else {
#if defined(_MSC_VER)
        return fsutil::PathStat::fstat(mFile, ps) ? 0 : -1;
#else
        return fsutil::PathStat::fstat(mFd, ps) ? 0 : -1;
#endif
    }
}

int LogFileOperator::Pread(void* ptr, size_t size, size_t count, int64_t offset) {
    if (!ptr || !size || !count || !IsOpen()) {
        return 0;
    }

    if (mFuseMode) {
        // datadir is NULL, ulogfs will get real datadir from env
        return ulogfs_pread2(mFd, NULL, ptr, size * count, (off_t*)&offset);
    } else {
#if defined(_MSC_VER)
        LARGE_INTEGER liPos;
        liPos.QuadPart = offset;
        if (FALSE == SetFilePointerEx(mFile, liPos, NULL, FILE_BEGIN)) {
            return 0;
        }
        DWORD dwRead = 0;
        if (FALSE == ::ReadFile(mFile, ptr, size * count, &dwRead, NULL)) {
            return 0;
        }
        return static_cast<int>(dwRead);
#else
        return pread(mFd, ptr, size * count, offset);
#endif
    }
}

size_t LogFileOperator::SkipHoleRead(void* ptr, size_t size, size_t count, int64_t* offset) {
    if (!mFuseMode || !ptr || !size || !count || !IsOpen()) {
        return 0;
    }

    int64_t off = *offset;
    int nBytes = ulogfs_pread2(mFd, NULL, ptr, (int)(size * count), (off_t*)&off);
    if (nBytes <= 0) {
        return nBytes;
    }

    auto readBytes = (size_t)nBytes;

    //if off == *offset, no hole no extra handle
    //if off > *offset, there is a hole
    if (off > *offset) {
        if (off > *offset + nBytes) {
            readBytes = 0;
        } else {
            readBytes = *offset + nBytes - off;
            memmove(ptr, ((char*)ptr + (off - *offset)), readBytes);
        }

        *offset = off;
    }

    return readBytes;
}

int64_t LogFileOperator::GetFileSize() const {
    if (!IsOpen()) {
        return -1;
    }

    if (mFuseMode) {
        return static_cast<int64_t>(ulogfs_tell(mFd));
    } else {
#if defined(_MSC_VER)
        LARGE_INTEGER liSize{0};
        if (FALSE == GetFileSizeEx(mFile, &liSize)) {
            return -1;
        }
        return static_cast<int64_t>(liSize.QuadPart);
#else
        return static_cast<int64_t>(lseek(mFd, 0, SEEK_END));
#endif
    }
}

bool LogFileOperator::IsOpen() const {
    return mFd >= 0;
}

int LogFileOperator::Close() {
    if (!IsOpen()) {
        return -1;
    }

    int ret = 0;
    if (mFuseMode) {
        ret = ulogfs_close(mFd);
    } else {
#if defined(_MSC_VER)
        ret = (TRUE == CloseHandle(mFile)) ? 0 : -1;
        mFile = INVALID_HANDLE_VALUE;
#else
        ret = close(mFd);
#endif
    }
    mFd = -1;
    return ret;
}

int LogFileOperator::GetFd() const {
    return mFd;
}

std::string LogFileOperator::GetFilePath() const {
    if (!IsOpen()) {
        return "";
    }

    if (mFuseMode) {
        return GetFdPath(mFd);
    } else {
#if defined(_MSC_VER)
        char filePath[MAX_PATH + 1];
        auto ret = GetFinalPathNameByHandle(mFile, filePath, MAX_PATH + 1, VOLUME_NAME_DOS);
        if (ret > MAX_PATH || ret <= 0) {
            return "";
        }
        if (0 == memcmp(filePath, "\\\\?\\", 4)) {
            return std::string(filePath + 4);
        }
        return std::string(filePath);
#else
        return GetFdPath(mFd);
#endif
    }
}

} // namespace logtail
