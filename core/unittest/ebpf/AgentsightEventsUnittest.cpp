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

#include <string>

#include "agentsight.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

class AgentsightEventsUnittest : public testing::Test {
public:
    void TestLlmRecordCopiesNonNullSizedBuffers();
    void TestLlmRecordCopiesProcessNameAndCmdline();
    void TestHttpsRecordCopiesProcessMetadata();
    void TestHttpsRecordCopiesPayloadsAndIdentity();
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

void AgentsightEventsUnittest::TestHttpsRecordCopiesProcessMetadata() {
    // The raw path carries the same process attribution as the LLM path, with the same buffer
    // shapes: process_name char[16], cmdline char[128], agent_name / container_id as C strings.
    {
        AgentsightHttpsData d{};
        std::memcpy(d.process_name, "node", 4U);
        std::memcpy(d.cmdline, "node /app/agent.js --serve", 26U);
        static const char kAgent[] = "hermes";
        static const char kCid[] = "abc123def456";
        d.agent_name = kAgent;
        d.container_id = kCid;

        AgentsightHttpsRecord r("pipe-a", d);
        APSARA_TEST_EQUAL(r.mProcessName, "node");
        APSARA_TEST_EQUAL(r.mCmdline, "node /app/agent.js --serve");
        APSARA_TEST_EQUAL(r.mAgentType, "hermes");
        APSARA_TEST_EQUAL(r.mContainerId, "abc123def456");
    }

    // Exited process / non-container / no rule match: empty buffer and null pointers all map to
    // empty strings, which FillAgentsightHttpCommon then skips instead of emitting blanks.
    {
        AgentsightHttpsData d{};
        AgentsightHttpsRecord r("pipe-a", d);
        APSARA_TEST_TRUE(r.mCmdline.empty());
        APSARA_TEST_TRUE(r.mAgentType.empty());
        APSARA_TEST_TRUE(r.mContainerId.empty());
    }

    // Non-NUL-terminated full buffer: the copy is bounded to the 128-byte buffer (strnlen), it does
    // not scan past the struct.
    {
        AgentsightHttpsData d{};
        std::memset(d.cmdline, 'x', sizeof(d.cmdline));
        AgentsightHttpsRecord r("pipe-a", d);
        APSARA_TEST_EQUAL(r.mCmdline.size(), sizeof(d.cmdline));
    }
}

void AgentsightEventsUnittest::TestHttpsRecordCopiesPayloadsAndIdentity() {
    // The four payloads are (ptr, len) pairs, not C strings: they are copied by length so embedded
    // NULs survive instead of truncating the body.
    static const char kReqHeaders[] = R"({"host":"api.example.com"})";
    static const char kReqBody[] = {'a', '\0', 'b'};
    static const char kResHeaders[] = R"({"content-type":"application/json"})";
    static const char kResBody[] = {'c', '\0', 'd', 'e'};

    AgentsightHttpsData d{};
    d.pid = 4242;
    d.timestamp_ns = 1234567890ULL;
    d.duration_ns = 5000ULL;
    d.method = "POST";
    d.path = "/v1/unknown";
    d.status_code = 503;
    d.is_sse = 1;
    d.request_headers = kReqHeaders;
    d.request_headers_len = sizeof(kReqHeaders) - 1;
    d.request_body = kReqBody;
    d.request_body_len = sizeof(kReqBody);
    d.response_headers = kResHeaders;
    d.response_headers_len = sizeof(kResHeaders) - 1;
    d.response_body = kResBody;
    d.response_body_len = sizeof(kResBody);

    AgentsightHttpsRecord r("pipe-raw", d);
    APSARA_TEST_EQUAL(PluginType::AGENTSIGHT_OBSERVE, r.GetPluginType());
    APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_HTTPS_RECORD, r.GetKernelEventType());
    APSARA_TEST_EQUAL("pipe-raw", r.GetPipelineConfigName());
    APSARA_TEST_EQUAL(4242, r.mPid);
    APSARA_TEST_EQUAL(1234567890ULL, r.mTimestampNs);
    APSARA_TEST_EQUAL(5000ULL, r.mDurationNs);
    APSARA_TEST_EQUAL("POST", r.mMethod);
    APSARA_TEST_EQUAL("/v1/unknown", r.mPath);
    APSARA_TEST_EQUAL(503, r.mStatusCode);
    APSARA_TEST_EQUAL(1, r.mIsSse);
    APSARA_TEST_EQUAL(kReqHeaders, r.mRequestHeaders);
    APSARA_TEST_EQUAL(std::string(kReqBody, sizeof(kReqBody)), r.mRequestBody);
    APSARA_TEST_EQUAL(kResHeaders, r.mResponseHeaders);
    APSARA_TEST_EQUAL(std::string(kResBody, sizeof(kResBody)), r.mResponseBody);
}

UNIT_TEST_CASE(AgentsightEventsUnittest, TestLlmRecordCopiesNonNullSizedBuffers)
UNIT_TEST_CASE(AgentsightEventsUnittest, TestLlmRecordCopiesProcessNameAndCmdline)
UNIT_TEST_CASE(AgentsightEventsUnittest, TestHttpsRecordCopiesProcessMetadata)
UNIT_TEST_CASE(AgentsightEventsUnittest, TestHttpsRecordCopiesPayloadsAndIdentity)

UNIT_TEST_MAIN
