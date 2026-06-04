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
#include <optional>
#include <string>

namespace logtail::ebpf {

/// Cached per `gen_ai.session.id`: last `gen_ai.input.messages` JSON array length and its hash.
struct AgentsightSessionInputState {
    size_t messageCount = 0;
    std::string messagesHash;
};

struct AgentsightInputUploadPlan {
    bool sendFullMessages = true;
    size_t inputMessageCount = 0;
    std::string messagesHash;
};

struct AgentsightParsedRequestParams {
    std::optional<std::string> temperature;
    std::optional<std::string> maxTokens;
    std::optional<std::string> frequencyPenalty;
    std::optional<std::string> presencePenalty;
    std::optional<std::string> topP;
    std::optional<std::string> topK;
    std::optional<std::string> seed;
    std::optional<std::string> choiceCount;
};

std::string Sha256Hex(const std::string& data);

size_t CountJsonArrayElements(const std::string& messagesJson);

std::string SerializeJsonArrayPrefix(const std::string& messagesJson, size_t prefixCount);

std::string HashJsonArrayPrefix(const std::string& fullMessagesJson, size_t prefixCount);

std::string ExtractSystemInstructionsJson(const std::string& requestMessagesJson);

AgentsightParsedRequestParams ParseRequestParametersJson(const std::string& requestParamsJson);

/// `fullMessagesJson` is the `gen_ai.input.messages` field value (one JSON array string).
/// `previousState` is null when the state key (`session_id`, or `turn.id` if session is absent)
/// is not in the map yet (must upload full messages).
AgentsightInputUploadPlan PlanInputMessagesUpload(const std::string& fullMessagesJson,
                                                  const AgentsightSessionInputState* previousState);

} // namespace logtail::ebpf
