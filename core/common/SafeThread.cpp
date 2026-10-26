// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/SafeThread.h"

#include <cstdlib>
#include <cstring>
#if defined(_MSC_VER)
#include <io.h>
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#else
#include <unistd.h>
#endif

namespace logtail {

#ifdef APSARA_UNIT_TEST_MAIN
namespace {
ThreadCreateFailHandler sThreadCreateFailHandlerForTest = nullptr;
bool sForceThreadCreateFailureForTest = false;
} // namespace

void SetThreadCreateFailHandlerForTest(ThreadCreateFailHandler handler) {
    sThreadCreateFailHandlerForTest = handler;
}

void SetForceThreadCreateFailureForTest(bool enable) {
    sForceThreadCreateFailureForTest = enable;
}

bool IsForceThreadCreateFailureForTest() {
    return sForceThreadCreateFailureForTest;
}
#endif

namespace {
void writeFd(int fd, const char* data, size_t len) {
    while (len > 0) {
#if defined(_MSC_VER)
        const int n = _write(fd, data, static_cast<unsigned int>(len));
#else
        const ssize_t n = write(fd, data, len);
#endif
        if (n <= 0) {
            return;
        }
        data += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
}

void writeLiteral(int fd, const char* data) {
    writeFd(fd, data, strlen(data));
}
} // namespace

void HandleThreadCreateFailure(const char* threadName, const std::exception& ex) {
    const char* name = threadName != nullptr ? threadName : "unknown";
    const char* err = ex.what() != nullptr ? ex.what() : "";
#ifdef APSARA_UNIT_TEST_MAIN
    if (sThreadCreateFailHandlerForTest != nullptr) {
        sThreadCreateFailHandlerForTest(name, ex);
        return;
    }
#endif
    writeLiteral(STDERR_FILENO, "failed to create thread ");
    writeLiteral(STDERR_FILENO, name);
    writeLiteral(STDERR_FILENO, ": ");
    writeLiteral(STDERR_FILENO, err);
    writeLiteral(STDERR_FILENO, "\n");
    _Exit(1);
}

} // namespace logtail
