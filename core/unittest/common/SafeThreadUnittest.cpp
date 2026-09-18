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

#include <future>
#include <string>
#include <thread>

#include "common/SafeThread.h"
#include "common/Thread.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

namespace {
string gLastFailedThread;

void RecordFailure(const char* name, const exception&) {
    gLastFailedThread = name != nullptr ? name : "";
}
} // namespace

class SafeThreadUnittest : public ::testing::Test {
public:
    void TestLaunchAsyncSuccess();
    void TestLaunchAsyncFailureDoesNotTerminate();
    void TestLaunchAsyncFailureResetsExistingFuture();
    void TestLaunchStdThreadFailureDoesNotTerminate();
    void TestCreateThreadFailureDoesNotTerminate();
    void TestMakeStdThreadFailureReturnsNull();

protected:
    void TearDown() override {
        SetForceThreadCreateFailureForTest(false);
        SetThreadCreateFailHandlerForTest(nullptr);
        gLastFailedThread.clear();
    }
};

void SafeThreadUnittest::TestLaunchAsyncSuccess() {
    future<void> fut;
    LaunchAsync(fut, "ok-async", []() {});
    APSARA_TEST_TRUE(fut.valid());
    fut.wait();
}

void SafeThreadUnittest::TestLaunchAsyncFailureDoesNotTerminate() {
    gLastFailedThread.clear();
    SetThreadCreateFailHandlerForTest(&RecordFailure);
    SetForceThreadCreateFailureForTest(true);

    future<void> fut;
    LaunchAsync(fut, "test-async", []() {});

    APSARA_TEST_EQUAL(string("test-async"), gLastFailedThread);
    APSARA_TEST_FALSE(fut.valid());
}

void SafeThreadUnittest::TestLaunchAsyncFailureResetsExistingFuture() {
    future<void> fut;
    LaunchAsync(fut, "ok-async", []() {});
    APSARA_TEST_TRUE(fut.valid());
    fut.wait();

    gLastFailedThread.clear();
    SetThreadCreateFailHandlerForTest(&RecordFailure);
    SetForceThreadCreateFailureForTest(true);
    LaunchAsync(fut, "test-async-reset", []() {});

    APSARA_TEST_EQUAL(string("test-async-reset"), gLastFailedThread);
    APSARA_TEST_FALSE(fut.valid());
}

void SafeThreadUnittest::TestLaunchStdThreadFailureDoesNotTerminate() {
    gLastFailedThread.clear();
    SetThreadCreateFailHandlerForTest(&RecordFailure);
    SetForceThreadCreateFailureForTest(true);

    thread t;
    LaunchStdThread(t, "test-std-thread", []() {});

    APSARA_TEST_EQUAL(string("test-std-thread"), gLastFailedThread);
    APSARA_TEST_FALSE(t.joinable());
}

void SafeThreadUnittest::TestCreateThreadFailureDoesNotTerminate() {
    gLastFailedThread.clear();
    SetThreadCreateFailHandlerForTest(&RecordFailure);
    SetForceThreadCreateFailureForTest(true);

    auto ptr = CreateThread("test-boost-thread", []() {});

    APSARA_TEST_EQUAL(string("test-boost-thread"), gLastFailedThread);
    APSARA_TEST_TRUE(ptr == nullptr);
}

void SafeThreadUnittest::TestMakeStdThreadFailureReturnsNull() {
    gLastFailedThread.clear();
    SetThreadCreateFailHandlerForTest(&RecordFailure);
    SetForceThreadCreateFailureForTest(true);

    auto ptr = MakeStdThread("test-make-thread", []() {});

    APSARA_TEST_EQUAL(string("test-make-thread"), gLastFailedThread);
    APSARA_TEST_TRUE(ptr == nullptr);
}

UNIT_TEST_CASE(SafeThreadUnittest, TestLaunchAsyncSuccess)
UNIT_TEST_CASE(SafeThreadUnittest, TestLaunchAsyncFailureDoesNotTerminate)
UNIT_TEST_CASE(SafeThreadUnittest, TestLaunchAsyncFailureResetsExistingFuture)
UNIT_TEST_CASE(SafeThreadUnittest, TestLaunchStdThreadFailureDoesNotTerminate)
UNIT_TEST_CASE(SafeThreadUnittest, TestCreateThreadFailureDoesNotTerminate)
UNIT_TEST_CASE(SafeThreadUnittest, TestMakeStdThreadFailureReturnsNull)

} // namespace logtail

UNIT_TEST_MAIN
