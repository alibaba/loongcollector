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

// Records copy every FFI string exactly once: the FFI pointers (RustCString / JsonBuffer holders,
// or borrows into HttpRecord) die when the callback returns and must not be retained. Emit fields
// land in the record's SourceBuffer arena (StringView members) — HandleLlmEvent/HandleHttpsEvent
// hand that arena to the PipelineEventGroup so emitted contents need no second copy. Compute-only
// fields (never emitted by default) are value-owned std::strings that die with the record at
// handler batch end, so un-emitted payloads are not pinned inside the group until flush.
//
// All helpers short-circuit on null/zero so CopyString never sees (nullptr, 0).

static StringView CopyCStr(SourceBuffer& sb, const char* s) {
    // agentsight.h: these fields are C strings from Rust CString (NUL-terminated). No length field.
    if (s == nullptr) {
        return kEmptyStringView;
    }
    const size_t len = std::strlen(s);
    if (len == 0) {
        return kEmptyStringView;
    }
    const StringBuffer buf = sb.CopyString(s, len);
    return StringView(buf.data, buf.size);
}

static StringView CopyBuffer(SourceBuffer& sb, const char* p, uint32_t len) {
    // agentsight.h: response_messages and the raw HTTPS bodies are (ptr, len), not necessarily
    // NUL-terminated in the middle; copy exactly len bytes (no read past len). Embedded NULs are
    // preserved — consumers stay length-bounded.
    if (p == nullptr || len == 0) {
        return kEmptyStringView;
    }
    const StringBuffer buf = sb.CopyString(p, len);
    return StringView(buf.data, buf.size);
}

// Same (ptr, len) contract as CopyBuffer, but for compute-only fields: value-owned std::string,
// deliberately kept out of the arena so the bytes are released with the record.
static std::string CopyBufferStr(const char* p, uint32_t len) {
    if (p == nullptr || len == 0) {
        return {};
    }
    return std::string(p, len);
}

static StringView CopyProcessName(SourceBuffer& sb, const char name[16]) {
    return CopyBuffer(sb, name, static_cast<uint32_t>(strnlen(name, 16U)));
}

static StringView CopyCmdline(SourceBuffer& sb, const char cmdline[128]) {
    // agentsight.h: cmdline is a fixed char[128] buffer, space-joined argv truncated to 127 bytes
    // and NUL-terminated (empty when the process has already exited). Bound the read to the buffer.
    return CopyBuffer(sb, cmdline, static_cast<uint32_t>(strnlen(cmdline, 128U)));
}

AgentsightLlmRecord::AgentsightLlmRecord(std::string pipelineConfigName, const AgentsightLLMData& d)
    : CommonEvent(KernelEventType::AGENTSIGHT_LLM_RECORD),
      mPipelineConfigName(std::move(pipelineConfigName)),
      mSourceBuffer(std::make_shared<SourceBuffer>(kAgentsightRecordChunkSize)) {
    SourceBuffer& sb = *mSourceBuffer;
    mSessionId = CopyCStr(sb, d.session_id);
    mConversationId = CopyCStr(sb, d.conversation_id);
    mResponseId = CopyCStr(sb, d.response_id);
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
    mProcessName = CopyProcessName(sb, d.process_name);
    mCmdline = CopyCmdline(sb, d.cmdline);
    mAgentType = CopyCStr(sb, d.agent_name);
    mContainerId = CopyCStr(sb, d.container_id);
    mRequestUrl = CopyCStr(sb, d.request_url);
    mProvider = CopyCStr(sb, d.provider);
    mModel = CopyCStr(sb, d.model);
    mFinishReason = CopyCStr(sb, d.finish_reason);
    // d.input_message_delta is deliberately not retained; see header note on the record layout.
    mResponseMessagesJson = CopyBuffer(sb, d.response_messages, d.response_messages_len);
    // Compute-only (header comment): value-owned, released with the record.
    mRequestMessagesJson = CopyBufferStr(d.request_messages, d.request_messages_len);
    mToolDefinitionsJson = CopyBufferStr(d.tools, d.tools_len);
}

AgentsightHttpsRecord::AgentsightHttpsRecord(std::string pipelineConfigName, const AgentsightHttpsData& d)
    : CommonEvent(KernelEventType::AGENTSIGHT_HTTPS_RECORD),
      mPipelineConfigName(std::move(pipelineConfigName)),
      mSourceBuffer(std::make_shared<SourceBuffer>(kAgentsightRecordChunkSize)) {
    SourceBuffer& sb = *mSourceBuffer;
    mPid = d.pid;
    mProcessName = CopyProcessName(sb, d.process_name);
    mCmdline = CopyCmdline(sb, d.cmdline);
    mAgentType = CopyCStr(sb, d.agent_name);
    mContainerId = CopyCStr(sb, d.container_id);
    mTimestampNs = d.timestamp_ns;
    mDurationNs = d.duration_ns;
    // agentsight.h: method / path are NUL-terminated C strings; the four payloads are (ptr, len).
    mMethod = CopyCStr(sb, d.method);
    mPath = CopyCStr(sb, d.path);
    mStatusCode = d.status_code;
    mIsSse = d.is_sse;
    mRequestBody = CopyBuffer(sb, d.request_body, d.request_body_len);
    mResponseBody = CopyBuffer(sb, d.response_body, d.response_body_len);
    // Compute-only (header comment): parsed into per-header copies by the handler, then released
    // with the record — filtered-out credential bytes never reach the event group.
    mRequestHeaders = CopyBufferStr(d.request_headers, d.request_headers_len);
    mResponseHeaders = CopyBufferStr(d.response_headers, d.response_headers_len);
}

} // namespace logtail::ebpf
