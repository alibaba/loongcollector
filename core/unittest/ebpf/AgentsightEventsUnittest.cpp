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

#include "agentsight.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

class AgentsightEventsUnittest : public testing::Test {
public:
    void TestLlmRecordCopiesNonNullSizedBuffers();
};

void AgentsightEventsUnittest::TestLlmRecordCopiesNonNullSizedBuffers() {
    static const char kReq[] = {'{', 'a'};
    static const char kRes[] = {'b', 'c', 'd'};
    static const char kTools[] = {'[', '{', '}', ']'};

    AgentsightLLMData d{};
    d.request_messages = kReq;
    d.request_messages_len = 2;
    d.response_messages = kRes;
    d.response_messages_len = 3;
    d.tools = kTools;
    d.tools_len = 4;

    AgentsightLlmRecord r("pipe-a", d);
    APSARA_TEST_EQUAL(r.mRequestMessagesJson, "{a");
    APSARA_TEST_EQUAL(r.mResponseMessagesJson, "bcd");
    APSARA_TEST_EQUAL(r.mToolDefinitionsJson, "[{}]");
}

UNIT_TEST_CASE(AgentsightEventsUnittest, TestLlmRecordCopiesNonNullSizedBuffers)

UNIT_TEST_MAIN
