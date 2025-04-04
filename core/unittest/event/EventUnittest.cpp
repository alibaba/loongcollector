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

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "common/Flags.h"
#include "file_server/event/Event.h"
#include "unittest/Unittest.h"
using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);

namespace logtail {
class EventUnittest : public ::testing::Test {
public:
    void TestIsContainerStopped() {
        LOG_INFO(sLogger, ("TestIsContainerStopped() begin", time(NULL)));
        Event event0("/source", "object", EVENT_ISDIR, 0);
        APSARA_TEST_TRUE_FATAL(!event0.IsContainerStopped());

        Event event1("/source", "object", EVENT_CONTAINER_STOPPED, 0);
        APSARA_TEST_TRUE_FATAL(event1.IsContainerStopped());
    }
};

APSARA_UNIT_TEST_CASE(EventUnittest, TestIsContainerStopped, 0);
} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
