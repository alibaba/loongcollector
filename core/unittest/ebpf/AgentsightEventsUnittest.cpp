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

#include <cstring>

#include "agentsight.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

class AgentsightEventsUnittest : public testing::Test {
public:
    void TestLlmRecordCopiesNonNullSizedBuffers();
    void TestLlmRecordCopiesProcessNameAndCmdline();
    void TestHttpsRecordCopiesNonNullSizedBuffers();
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

void AgentsightEventsUnittest::TestLlmRecordCopiesProcessNameAndCmdline() {
    // Normal case: process_name and cmdline are NUL-terminated within their fixed buffers.
    {
        AgentsightLLMData d{};
        std::memcpy(d.process_name, "node", 4U);
        std::memcpy(d.cmdline, "node /app/server.js --port 8080", 31U);
        static const char kCid[] = "abc123def456";
        d.container_id = kCid;

        AgentsightLlmRecord r("pipe-a", d);
        APSARA_TEST_EQUAL(r.mProcessName, "node");
        APSARA_TEST_EQUAL(r.mCmdline, "node /app/server.js --port 8080");
        APSARA_TEST_EQUAL(r.mContainerId, "abc123def456");
    }

    // Exited process / non-container: empty cmdline and null container_id map to empty strings.
    {
        AgentsightLLMData d{};
        AgentsightLlmRecord r("pipe-a", d);
        APSARA_TEST_TRUE(r.mCmdline.empty());
        APSARA_TEST_TRUE(r.mContainerId.empty());
    }

    // Non-NUL-terminated full buffer: copy is bounded to the 128-byte buffer (strnlen).
    {
        AgentsightLLMData d{};
        std::memset(d.cmdline, 'x', sizeof(d.cmdline));
        AgentsightLlmRecord r("pipe-a", d);
        APSARA_TEST_EQUAL(r.mCmdline.size(), sizeof(d.cmdline));
    }
}

UNIT_TEST_CASE(AgentsightEventsUnittest, TestLlmRecordCopiesNonNullSizedBuffers)
UNIT_TEST_CASE(AgentsightEventsUnittest, TestLlmRecordCopiesProcessNameAndCmdline)

void AgentsightEventsUnittest::TestHttpsRecordCopiesNonNullSizedBuffers() {
    // (ptr, len) buffers without NUL terminators: must be copied by length
    static const char kReqHdrs[] = {'{', '"', 'h'};
    static const char kReqBody[] = {'r', 'e', 'q'};
    static const char kResHdrs[] = {'{', '"', 's'};
    static const char kResBody[] = {'r', 'e', 's', 'p'};

    AgentsightHttpsData d{};
    d.pid = 1234;
    d.timestamp_ns = 100;
    d.duration_ns = 50;
    d.status_code = 200;
    d.is_sse = 1;
    d.method = "POST";
    d.path = "/v1/chat";
    d.request_headers = kReqHdrs;
    d.request_headers_len = 3;
    d.request_body = kReqBody;
    d.request_body_len = 3;
    d.response_headers = kResHdrs;
    d.response_headers_len = 3;
    d.response_body = kResBody;
    d.response_body_len = 4;

    AgentsightHttpsRecord r("pipe-b", d);
    APSARA_TEST_EQUAL(r.GetKernelEventType(), KernelEventType::AGENTSIGHT_HTTPS_RECORD);
    APSARA_TEST_EQUAL(r.mPid, 1234);
    APSARA_TEST_EQUAL(r.mTimestampNs, 100UL);
    APSARA_TEST_EQUAL(r.mDurationNs, 50UL);
    APSARA_TEST_EQUAL(r.mStatusCode, 200);
    APSARA_TEST_EQUAL(r.mIsSse, 1);
    APSARA_TEST_EQUAL(r.mMethod, "POST");
    APSARA_TEST_EQUAL(r.mPath, "/v1/chat");
    APSARA_TEST_EQUAL(r.mRequestHeaders, "{\"h");
    APSARA_TEST_EQUAL(r.mRequestBody, "req");
    APSARA_TEST_EQUAL(r.mResponseHeaders, "{\"s");
    APSARA_TEST_EQUAL(r.mResponseBody, "resp");
    APSARA_TEST_EQUAL(r.GetPipelineConfigName(), "pipe-b");
}

UNIT_TEST_CASE(AgentsightEventsUnittest, TestHttpsRecordCopiesNonNullSizedBuffers)

UNIT_TEST_MAIN
