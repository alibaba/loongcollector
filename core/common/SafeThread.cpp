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

#include <cstdio>
#include <cstdlib>

#include "logger/Logger.h"

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

void HandleThreadCreateFailure(const char* threadName, const std::exception& ex) {
    const char* name = threadName != nullptr ? threadName : "unknown";
    if (sLogger) {
        LOG_ERROR(sLogger, ("failed to create thread", name)("error", ex.what()));
        sLogger->flush();
    } else {
        fprintf(stderr, "failed to create thread %s: %s\n", name, ex.what());
        fflush(stderr);
    }
#ifdef APSARA_UNIT_TEST_MAIN
    if (sThreadCreateFailHandlerForTest != nullptr) {
        sThreadCreateFailHandlerForTest(name, ex);
        return;
    }
#endif
    exit(1);
}

} // namespace logtail
