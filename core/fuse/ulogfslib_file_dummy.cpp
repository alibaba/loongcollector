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

#include "ulogfslib_file.h"

// Empty implementations of ulogfs.
// TODO: Replace it with libulogfs.a

#ifdef __cplusplus
extern "C" {
#endif

int ulogfs_open_ro(const char* name) {
    return -1;
}

int ulogfs_open(const char* name) {
    return -1;
}

int ulogfs_close(int fd) {
    return -1;
}

ssize_t ulogfs_pread(int fd, void* buf, size_t count, off_t* offset) {
    return -1;
}

ssize_t ulogfs_pread2(int fd, const char* datadir, void* buf, size_t count, off_t* offset) {
    return -1;
}

off_t ulogfs_seek(int fd, off_t offset, int whence) {
    return -1;
}

#if defined(__linux__)
int ulogfs_stat(int fd, struct stat* buf) {
    return -1;
}
#elif defined(_MSC_VER)
int ulogfs_stat(int fd, struct _stat64* buf) {
    return -1;
}
#endif

int64_t ulogfs_tell(int fd) {
    return -1;
}

int ulogfs_sparse(int fd, const char* name, off_t offset, int64_t length) {
    return -1;
}

int ulogfs_fflush(int fd, const char* name) {
    return -1;
}

int ulogfs_fflush2(int fd, const char* name) {
    return -1;
}

int ulogfs_dump(int type, long ino, void* buf, int64_t* len) {
    return -1;
}

int ulogfs_hello() {
    return -1;
}

int ulogfs_upgrade() {
    return -1;
}

#ifdef __cplusplus
}
#endif
