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

#include "ebpf/plugin/agentsight/AgentsightEvents.h"

#include <cstring>

namespace logtail::ebpf {

// All fields in AgentsightLlmRecord are std::string / scalars: this constructor deep-copies from
// AgentsightLLMData; it does not retain pointers into the FFI callback stack (RustCString holders)
// after the callback returns.

static std::string CopyCStr(const char* s) {
    // agentsight.h: these fields are C strings from Rust CString (NUL-terminated). No length field.
    return s ? std::string(s) : std::string();
}

static std::string CopyBuffer(const char* p, uint32_t len) {
    // agentsight.h: request_messages / response_messages / tools are (ptr, len), not necessarily
    // NUL-terminated in the middle; copy exactly len bytes (no read past len).
    if (!p || len == 0) {
        return {};
    }
    return std::string(p, static_cast<size_t>(len));
}

static std::string CopyProcessName(const char name[16]) {
    return std::string(name, strnlen(name, 16U));
}

static std::string CopyCmdline(const char cmdline[128]) {
    // agentsight.h: cmdline is a fixed char[128] buffer, space-joined argv truncated to 127 bytes
    // and NUL-terminated (empty when the process has already exited). Bound the read to the buffer.
    return std::string(cmdline, strnlen(cmdline, 128U));
}

AgentsightLlmRecord::AgentsightLlmRecord(std::string pipelineConfigName, const AgentsightLLMData& d)
    : CommonEvent(KernelEventType::AGENTSIGHT_LLM_RECORD), mPipelineConfigName(std::move(pipelineConfigName)) {
    mSessionId = CopyCStr(d.session_id);
    mConversationId = CopyCStr(d.conversation_id);
    mResponseId = CopyCStr(d.response_id);
    mPid = d.pid;
    mTimestampNs = d.timestamp_ns;
    mDurationNs = d.duration_ns;
    mStatusCode = d.status_code;
    mIsSse = d.is_sse;
    mLlmUsage = d.llm_usage;
    mInputTokens = d.input_tokens;
    mOutputTokens = d.output_tokens;
    mTotalTokens = d.total_tokens;
    mCacheCreationInputTokens = d.cache_creation_input_tokens;
    mCacheReadInputTokens = d.cache_read_input_tokens;
    mProcessName = CopyProcessName(d.process_name);
    mCmdline = CopyCmdline(d.cmdline);
    mAgentType = CopyCStr(d.agent_name);
    mContainerId = CopyCStr(d.container_id);
    mRequestUrl = CopyCStr(d.request_url);
    mProvider = CopyCStr(d.provider);
    mModel = CopyCStr(d.model);
    mFinishReason = CopyCStr(d.finish_reason);
    mRequestMessagesJson = CopyBuffer(d.request_messages, d.request_messages_len);
    mInputMessageDeltaJson = CopyBuffer(d.input_message_delta, d.input_message_delta_len);
    mResponseMessagesJson = CopyBuffer(d.response_messages, d.response_messages_len);
    mToolDefinitionsJson = CopyBuffer(d.tools, d.tools_len);
}

AgentsightHttpsRecord::AgentsightHttpsRecord(std::string pipelineConfigName, const AgentsightHttpsData& d)
    : CommonEvent(KernelEventType::AGENTSIGHT_HTTPS_RECORD), mPipelineConfigName(std::move(pipelineConfigName)) {
    mPid = d.pid;
    mProcessName = CopyProcessName(d.process_name);
    mCmdline = CopyCmdline(d.cmdline);
    mAgentType = CopyCStr(d.agent_name);
    mContainerId = CopyCStr(d.container_id);
    mTimestampNs = d.timestamp_ns;
    mDurationNs = d.duration_ns;
    // agentsight.h: method / path are NUL-terminated C strings; the four payloads are (ptr, len).
    mMethod = CopyCStr(d.method);
    mPath = CopyCStr(d.path);
    mStatusCode = d.status_code;
    mIsSse = d.is_sse;
    mRequestHeaders = CopyBuffer(d.request_headers, d.request_headers_len);
    mRequestBody = CopyBuffer(d.request_body, d.request_body_len);
    mResponseHeaders = CopyBuffer(d.response_headers, d.response_headers_len);
    mResponseBody = CopyBuffer(d.response_body, d.response_body_len);
}

namespace {

// Estimate helpers — one per Copy* flavour above so the accounted bytes track exactly what the
// deep copy would hold: (ptr, len) payloads are counted by len (they may contain embedded NULs),
// NUL-terminated strings by strlen. See EstimateAgentsight*RecordBytes in the header for the
// full accounting caveat (payload bytes, not malloc bytes).
inline uint64_t EstimateCStrBytes(const char* s) {
    return s ? static_cast<uint64_t>(std::strlen(s)) : 0;
}

inline uint64_t EstimateFixedBufferBytes(const char* buf, size_t cap) {
    return static_cast<uint64_t>(strnlen(buf, cap));
}

} // namespace

uint64_t EstimateAgentsightLlmRecordBytes(const AgentsightLLMData& d) {
    // Fixed overhead approximation: scalars plus the string headers / SSO buffers of the record.
    uint64_t bytes = sizeof(AgentsightLlmRecord);
    bytes += EstimateFixedBufferBytes(d.process_name, 16U);
    bytes += EstimateFixedBufferBytes(d.cmdline, 128U);
    bytes += EstimateCStrBytes(d.session_id) + EstimateCStrBytes(d.conversation_id) + EstimateCStrBytes(d.response_id)
        + EstimateCStrBytes(d.agent_name) + EstimateCStrBytes(d.container_id) + EstimateCStrBytes(d.request_url)
        + EstimateCStrBytes(d.provider) + EstimateCStrBytes(d.model) + EstimateCStrBytes(d.finish_reason);
    bytes += static_cast<uint64_t>(d.request_messages_len) + static_cast<uint64_t>(d.input_message_delta_len)
        + static_cast<uint64_t>(d.response_messages_len) + static_cast<uint64_t>(d.tools_len);
    return bytes;
}

uint64_t EstimateAgentsightHttpsRecordBytes(const AgentsightHttpsData& d) {
    uint64_t bytes = sizeof(AgentsightHttpsRecord);
    bytes += EstimateFixedBufferBytes(d.process_name, 16U);
    bytes += EstimateFixedBufferBytes(d.cmdline, 128U);
    bytes += EstimateCStrBytes(d.method) + EstimateCStrBytes(d.path) + EstimateCStrBytes(d.agent_name)
        + EstimateCStrBytes(d.container_id);
    bytes += static_cast<uint64_t>(d.request_headers_len) + static_cast<uint64_t>(d.request_body_len)
        + static_cast<uint64_t>(d.response_headers_len) + static_cast<uint64_t>(d.response_body_len);
    return bytes;
}

} // namespace logtail::ebpf
