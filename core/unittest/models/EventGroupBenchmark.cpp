// Copyright 2023 iLogtail Authors
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

#include <cstdlib>
#include "common/JsonUtil.h"
#include "common/TimeUtil.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

namespace logtail {

class EventGroupBenchmark {
public:
    void TestEraseInLoop();
    void TestWriteIndexInLoop();
};

void EraseInLoop(PipelineEventGroup& logGroup) {
    EventsContainer& events = logGroup.MutableEvents();
    for (auto it = events.begin(); it != events.end();) {
        if (false) {
            ++it;
        } else {
            it = events.erase(it);
        }
    }
}

void WriteIndexInLoop(PipelineEventGroup& logGroup) {
    EventsContainer& events = logGroup.MutableEvents();
    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (false) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
}

void EventGroupBenchmark::TestEraseInLoop() {
    // SetUp
    std::vector<PipelineEventGroup> eventGroups;
    for (int i = 0; i < 1000; ++i) {
        eventGroups.emplace_back(std::make_shared<SourceBuffer>());
    }
    for (auto& group : eventGroups) {
        for (int i = 0; i < 1000; ++i) {
            group.AddLogEvent();
        }
    }
    // Test
    uint64_t starttime = GetCurrentTimeInMilliSeconds();
    for (auto& group : eventGroups) {
        EraseInLoop(group);
    }
    uint64_t timeelapsed = GetCurrentTimeInMilliSeconds() - starttime;
    printf("%s costs %lums\n", __func__, timeelapsed);
}

void EventGroupBenchmark::TestWriteIndexInLoop() {
    // SetUp
    std::vector<PipelineEventGroup> eventGroups;
    for (int i = 0; i < 1000; ++i) {
        eventGroups.emplace_back(std::make_shared<SourceBuffer>());
    }
    for (auto& group : eventGroups) {
        for (int i = 0; i < 1000; ++i) {
            group.AddLogEvent();
        }
    }
    // Test
    uint64_t starttime = GetCurrentTimeInMilliSeconds();
    for (auto& group : eventGroups) {
        WriteIndexInLoop(group);
    }
    uint64_t timeelapsed = GetCurrentTimeInMilliSeconds() - starttime;
    printf("%s costs %lums\n", __func__, timeelapsed);
}

} // namespace logtail

int main(int argc, char* argv[]) {
    logtail::EventGroupBenchmark benchmark;
    benchmark.TestEraseInLoop();
    benchmark.TestWriteIndexInLoop();
    /* Result:
       TestEraseInLoop costs 453ms
       TestWriteIndexInLoop costs 22ms
     */
    return 0;
}