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

#include "unittest/Unittest.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include "common/Flags.h"
#include "common/util.h"
#include "common/FileSystemUtil.h"
#include "polling/PollingEventQueue.h"
#include "event/Event.h"
#include "event_handler/LogInput.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_INT32(batch_send_interval);

namespace logtail {
class LogInputUnittest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        LogInput::GetInstance()->mModifyEventSet.clear();
        std::queue<Event*> empty;
        std::swap(LogInput::GetInstance()->mInotifyEventQueue, empty);
    }

public:
    void TestTryReadEventsPollingEvents() {
        LOG_INFO(sLogger, ("TestTryReadEventsPollingEvents() begin", time(NULL)));
        Event* event0 = new Event("/source", "object1", EVENT_MODIFY, 0);
        PollingEventQueue::GetInstance()->PushEvent(event0);
        LogInput::GetInstance()->TryReadEvents(true);
        APSARA_TEST_EQUAL_FATAL(LogInput::GetInstance()->mInotifyEventQueue.size(), 1L);
        Event* ev = LogInput::GetInstance()->PopEventQueue();
        APSARA_TEST_EQUAL_FATAL(ev, event0);
        delete ev;
    }

    void TestTryReadEventsDuplicatedEvents() {
        LOG_INFO(sLogger, ("TestTryReadEventsContainerStoppedEvents() begin", time(NULL)));
        Event* event0 = new Event("/source", "object1", EVENT_MODIFY, 0);
        PollingEventQueue::GetInstance()->PushEvent(event0);
        Event* event1 = new Event("/source", "object1", EVENT_MODIFY, 0);
        PollingEventQueue::GetInstance()->PushEvent(event1);
        Event* event2 = new Event("/source", "object1", EVENT_MODIFY, 0);
        PollingEventQueue::GetInstance()->PushEvent(event2);
        LogInput::GetInstance()->TryReadEvents(true);
        APSARA_TEST_EQUAL_FATAL(LogInput::GetInstance()->mInotifyEventQueue.size(), 1L);
        Event* ev = LogInput::GetInstance()->PopEventQueue();
        delete ev;
    }
};

APSARA_UNIT_TEST_CASE(LogInputUnittest, TestTryReadEventsPollingEvents, 0);
APSARA_UNIT_TEST_CASE(LogInputUnittest, TestTryReadEventsDuplicatedEvents, 0);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
