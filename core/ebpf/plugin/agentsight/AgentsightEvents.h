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

#pragma once

#include <cstdint>

#include <string>

#include "agentsight.h"
#include "ebpf/include/export.h"
#include "ebpf/type/CommonDataEvent.h"

namespace logtail::ebpf {

class AgentsightLlmRecord : public CommonEvent {
public:
    AgentsightLlmRecord(std::string pipelineConfigName, const AgentsightLLMData& d);

    PluginType GetPluginType() const override { return PluginType::AGENTSIGHT_OBSERVE; }

    const std::string& GetPipelineConfigName() const { return mPipelineConfigName; }

    std::string mPipelineConfigName;

    std::string mSessionId;
    std::string mConversationId;
    std::string mResponseId;
    int32_t mPid = 0;
    uint64_t mTimestampNs = 0;
    uint64_t mDurationNs = 0;
    uint16_t mStatusCode = 0;
    uint8_t mIsSse = 0;
    bool mLlmUsage = false;
    uint32_t mInputTokens = 0;
    uint32_t mOutputTokens = 0;
    uint32_t mTotalTokens = 0;
    uint32_t mCacheCreationInputTokens = 0;
    uint32_t mCacheReadInputTokens = 0;
    std::string mProcessName;
    // Space-joined process command line (argv), truncated to 127 bytes; empty when the
    // process has already exited. Maps to AgentsightLLMData.cmdline (char[128]).
    std::string mCmdline;
    std::string mAgentType;
    // Container id resolved from the pid on the agentsight side; empty when the process is
    // not in a container (or resolution failed). Maps to AgentsightLLMData.container_id.
    std::string mContainerId;
    std::string mRequestUrl;
    std::string mProvider;
    std::string mModel;
    std::string mFinishReason;
    std::string mRequestMessagesJson;
    // Incremental ("latest round") input messages: system messages dropped and only the
    // messages from the last `user` message onward are kept. Maps to AgentsightLLMData.input_message_delta.
    std::string mInputMessageDeltaJson;
    std::string mResponseMessagesJson;
    std::string mToolDefinitionsJson;
};

/// Raw HTTP exchange reported when AgentSight could not parse the traffic into LLM semantics
/// (unknown API path or unrecognised body shape). Requires `RawHttpsFallback: true` plus
/// libagentsight >= 0.9.0; see AgentsightManager::OnHttpsCallback.
///
/// Deliberately narrower than AgentsightLlmRecord: AgentsightHttpsData carries no session id,
/// conversation id, agent type or container id, so correlation is limited to (pid, comm). There is
/// no request_url either — only method + path; the target host is inside mRequestHeaders.
class AgentsightHttpsRecord : public CommonEvent {
public:
    AgentsightHttpsRecord(std::string pipelineConfigName, const AgentsightHttpsData& d);

    PluginType GetPluginType() const override { return PluginType::AGENTSIGHT_OBSERVE; }

    const std::string& GetPipelineConfigName() const { return mPipelineConfigName; }

    std::string mPipelineConfigName;

    int32_t mPid = 0;
    std::string mProcessName;
    uint64_t mTimestampNs = 0;
    uint64_t mDurationNs = 0;
    std::string mMethod;
    std::string mPath;
    uint16_t mStatusCode = 0;
    uint8_t mIsSse = 0;
    // Length-delimited payloads: AgentsightHttpsData reports these as (ptr, len) pairs and they may
    // contain embedded NULs (compressed or binary bodies), so they are copied by length, not by
    // NUL scan, and may not be valid UTF-8.
    std::string mRequestHeaders;
    std::string mRequestBody;
    std::string mResponseHeaders;
    std::string mResponseBody;
};

} // namespace logtail::ebpf
