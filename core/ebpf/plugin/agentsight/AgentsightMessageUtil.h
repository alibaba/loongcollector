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

#include <cstddef>

#include <string>

namespace logtail::ebpf {

/// Cached per `gen_ai.session.id` (see `ResolveSessionStateKey`): last completed LLM round for delta/dedup
/// and per-turn `gen_ai.step.id` sequencing.
struct AgentsightSessionInputState {
    size_t messageCount = 0;
    std::string messagesHash;
    size_t outputMessageCount = 0;
    std::string outputMessagesHash;
    std::string systemInstructionsHash;
    std::string toolDefinitionsHash;
    std::string lastTurnId;
    size_t nextStepNumber = 1;
    /// Per-turn monotonic sequence for split-mode `gen_ai.event.sequence` (each emitted log row).
    size_t nextEventSequence = 1;
};

/// Delta/dedup LRU key: `session_id`, or `turn.id` when session is absent.
std::string ResolveSessionStateKey(const std::string& sessionId, const std::string& turnId);

/// `{turn.id}:s{N}` (e.g. `278a5a71…:s2`); empty when `turnId` is empty.
std::string FormatGenAiStepId(const std::string& turnId, size_t stepNumber);

std::string ExtractSystemInstructionsJson(const std::string& requestMessagesJson);

/// SHA-256 (hex) of `ExtractSystemInstructionsJson` output.
std::string ComputeSystemInstructionsHash(const std::string& requestMessagesJson);

/// SHA-256 (hex) of the raw tool-definitions JSON string.
std::string ComputeToolDefinitionsHash(const std::string& toolDefinitionsJson);

/// Builds `gen_ai.response.finish_reasons` as a JSON string array, e.g. `["stop"]`.
/// Collects `finish_reason` from each object in `responseMessagesJson`; uses
/// `fallbackFinishReason` when the array is empty or unparsable.
std::string FormatFinishReasonsJson(const std::string& responseMessagesJson, const std::string& fallbackFinishReason);

/// Derives `gen_ai.input.messages_delta` locally (does not use AgentSight FFI delta).
/// `previousState` stores the last round's **request** (`messageCount` / `messagesHash`) and
/// **response** (`outputMessageCount` / `outputMessagesHash`). `messageCount` counts **non-system**
/// request messages only. When `cur`'s first `messageCount` non-system messages match `messagesHash`
/// (H_in: role+parts-only per message), skip `outputMessageCount` messages only if
/// `hash(cur[idxAfterIn:idxAfterIn+N_out]) == outputMessagesHash`; otherwise delta starts at `idxAfterIn`.
/// H_in uses role+parts normalization; H_out uses role-only normalization. System messages are omitted from delta.
std::string ComputeInputMessagesDelta(const std::string& fullMessagesJson,
                                      const AgentsightSessionInputState* previousState);

/// After each emit: `messageCount` / `messagesHash` cover **non-system request** messages only;
/// response replay state is stored in `outputMessageCount` / `outputMessagesHash`.
void CommitSessionStateAfterEmit(const std::string& requestMessagesJson,
                                 const std::string& responseMessagesJson,
                                 const std::string& toolDefinitionsJson,
                                 AgentsightSessionInputState& state);

} // namespace logtail::ebpf
