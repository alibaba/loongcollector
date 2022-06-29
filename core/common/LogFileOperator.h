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
#include <cstdio>
#include <string>
#include <cstdint>
#if defined(_MSC_VER)
#include <Windows.h>
#elif defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace logtail {

namespace fsutil {
    class PathStat;
}

class LogFileOperator {
public:
    LogFileOperator(bool fuseMode = false) : mFuseMode(fuseMode) {}
    ~LogFileOperator() { Close(); }

    // @return file descriptor when fuseMode is enabled or on Linux.
    //   An positve identifier is returned on Windows.
    int Open(const char* path, bool fuseMode = false);

    int64_t Seek(int64_t offset, int whence);

    int Stat(fsutil::PathStat& ps) const;

    int Pread(void* ptr, size_t size, size_t count, int64_t offset);

    // For FUSE only.
    size_t SkipHoleRead(void* ptr, size_t size, size_t count, int64_t* offset);

    // GetFileSize gets the size of current file.
    int64_t GetFileSize() const;

    bool IsOpen() const;

    // Close always let IsOpen false
    int Close();

    // For non-fuse mode on Windows, it returns an identifier rather
    // than real file descriptor.
    int GetFd() const;

    // GetFilePath gets the current path of file.
    std::string GetFilePath() const;

private:
    LogFileOperator(const LogFileOperator&) = delete;
    LogFileOperator& operator=(const LogFileOperator&) = delete;

private:
    // We have to use HANDLE on Windows to support more simultaneous opened files,
    // which is limit by C runtime (8192 file descriptors at most).
#if defined(_MSC_VER)
    HANDLE mFile = INVALID_HANDLE_VALUE;
#endif
    int mFd = -1;
    bool mFuseMode;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogFileOperatorUnittest;
#endif
};

} // namespace logtail
