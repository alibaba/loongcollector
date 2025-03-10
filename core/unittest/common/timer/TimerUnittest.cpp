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

#include <vector>

#include "common/timer/Timer.h"
#include "ebpf/type/PeriodicalEvent.h"
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
    void TestPeriodicEvent();

private:
    std::vector<int> mVec;
    int mFlag = 0;
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

void TimerUnittest::TestPeriodicEvent() {
    Timer::GetInstance()->Init();
    mFlag = true;
    std::unique_ptr<ebpf::AggregateEvent> event = std::make_unique<ebpf::AggregateEvent>(
        1, // interval second
        [this](const std::chrono::steady_clock::time_point& execTime) { // handler
            this->mVec.push_back(1);
            return true;
        },
        [this]() { // validator
            return !this->mFlag;
        });
    Timer::GetInstance()->PushEvent(std::move(event));
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // update start uid, invalid event schedule ...
    mFlag = false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_TRUE(Timer::GetInstance()->mQueue.empty());
    APSARA_TEST_EQUAL(mVec.size(), 5UL);
    Timer::GetInstance()->Stop();
}

UNIT_TEST_CASE(TimerUnittest, TestPushEvent)
UNIT_TEST_CASE(TimerUnittest, TestPeriodicEvent)


} // namespace logtail

UNIT_TEST_MAIN
