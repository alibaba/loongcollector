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
    std::string lastTurnId;
    size_t nextStepNumber = 1;
};

/// Delta/dedup LRU key: `session_id`, or `turn.id` when session is absent.
std::string ResolveSessionStateKey(const std::string& sessionId, const std::string& turnId);

std::string FormatGenAiStepId(size_t stepNumber);

size_t CountJsonArrayElements(const std::string& messagesJson);

std::string SerializeJsonArrayPrefix(const std::string& messagesJson, size_t prefixCount);

/// Sub-array `[startIndex, startIndex + elementCount)` in message order.
std::string SerializeJsonArrayRange(const std::string& messagesJson, size_t startIndex, size_t elementCount);

/// Sub-array `[startIndex, end)` through the last element.
std::string SerializeJsonArraySuffix(const std::string& messagesJson, size_t startIndex);

/// SHA-256 of prefix messages after keeping only each message's `role` and `parts` (H_in / H_out).
std::string HashJsonArrayPrefix(const std::string& fullMessagesJson, size_t prefixCount);

/// SHA-256 of `[startIndex, startIndex + elementCount)` with the same role+parts normalization as H_in / H_out.
std::string HashJsonArrayRange(const std::string& fullMessagesJson, size_t startIndex, size_t elementCount);

std::string ExtractSystemInstructionsJson(const std::string& requestMessagesJson);

/// Builds `gen_ai.response.finish_reasons` as a JSON string array, e.g. `["stop"]`.
/// Collects `finish_reason` from each object in `responseMessagesJson`; uses
/// `fallbackFinishReason` when the array is empty or unparsable.
std::string FormatFinishReasonsJson(const std::string& responseMessagesJson, const std::string& fallbackFinishReason);

/// H_in over the full `gen_ai.input.messages` array (role+parts normalization per message).
std::string ComputeInputMessagesHash(const std::string& fullMessagesJson);

/// Derives `gen_ai.input.messages.delta` locally (does not use AgentSight FFI delta).
/// `previousState` stores the last round's **request** (`messageCount` / `messagesHash`) and
/// **response** (`outputMessageCount` / `outputMessagesHash`). When `cur`'s first `messageCount`
/// messages match `messagesHash` (H_in: role+parts-only per message), skip `outputMessageCount` messages only if
/// `hash(cur[N_in:N_in+N_out]) == outputMessagesHash`; otherwise delta starts at `N_in`.
/// Both H_in and H_out use the same role+parts normalization. System messages are omitted from delta.
std::string ComputeInputMessagesDelta(const std::string& fullMessagesJson,
                                      const AgentsightSessionInputState* previousState);

void UpdateSessionOutputState(const std::string& responseMessagesJson, AgentsightSessionInputState& state);

/// After each emit: `messageCount` / `messagesHash` cover **request** messages only; response
/// replay state is stored in `outputMessageCount` / `outputMessagesHash`.
void CommitSessionStateAfterEmit(const std::string& requestMessagesJson,
                                 const std::string& responseMessagesJson,
                                 AgentsightSessionInputState& state);

} // namespace logtail::ebpf
