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
    mAgentType = CopyCStr(d.agent_name);
    mRequestUrl = CopyCStr(d.request_url);
    mProvider = CopyCStr(d.provider);
    mModel = CopyCStr(d.model);
    mFinishReason = CopyCStr(d.finish_reason);
    mRequestMessagesJson = CopyBuffer(d.request_messages, d.request_messages_len);
    mResponseMessagesJson = CopyBuffer(d.response_messages, d.response_messages_len);
    mToolDefinitionsJson = CopyBuffer(d.tools, d.tools_len);
}

} // namespace logtail::ebpf
