// Copyright 2024 iLogtail Authors
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

#include "common/timer/Timer.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

struct TimerEventMock : public TimerEvent {
    TimerEventMock(const chrono::steady_clock::time_point& execTime) : TimerEvent(execTime) {}

    bool IsValid() const override { return mIsValid; }
    bool Execute() { return true; }

    bool mIsValid = false;
};

class TimerUnittest : public ::testing::Test {
public:
    void TestPushEvent();
};

void TimerUnittest::TestPushEvent() {
    auto now = chrono::steady_clock::now();
    Timer timer;
    timer.PushEvent(make_unique<TimerEventMock>(now + chrono::seconds(2)));
    timer.PushEvent(make_unique<TimerEventMock>(now + chrono::seconds(1)));
    timer.PushEvent(make_unique<TimerEventMock>(now + chrono::seconds(3)));

    APSARA_TEST_EQUAL(3U, timer.mQueue.size());
    APSARA_TEST_EQUAL(now + chrono::seconds(1), timer.mQueue.top()->GetExecTime());
    timer.mQueue.pop();
    APSARA_TEST_EQUAL(now + chrono::seconds(2), timer.mQueue.top()->GetExecTime());
    timer.mQueue.pop();
    APSARA_TEST_EQUAL(now + chrono::seconds(3), timer.mQueue.top()->GetExecTime());
    timer.mQueue.pop();
}

UNIT_TEST_CASE(TimerUnittest, TestPushEvent)

} // namespace logtail

UNIT_TEST_MAIN
