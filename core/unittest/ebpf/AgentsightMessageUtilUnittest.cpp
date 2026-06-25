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

#include "ebpf/plugin/agentsight/AgentsightMessageUtil.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

namespace {

void ApplyRoundState(const std::string& inputJson,
                     const std::string& outputJson,
                     AgentsightSessionInputState& state,
                     const std::string& toolsJson = "[]") {
    CommitSessionStateAfterEmit(inputJson, outputJson, toolsJson, state);
}

} // namespace

class AgentsightMessageUtilUnittest : public testing::Test {
public:
    void TestExtractSystemInstructions();
    void TestInputMessagesHashFirstRound();
    void TestInputMessagesHashStableAcrossRounds();
    void TestInputMessagesHashChangesWhenContentChanges();
    void TestFormatFinishReasonsFromOutputMessages();
    void TestFormatFinishReasonsFallback();
    void TestFormatFinishReasonsFromParts();
    void TestFormatFinishReasonsFallbackAlwaysArray();
    void TestComputeDeltaFirstRound();
    void TestComputeDeltaAfterOutputMatch();
    void TestComputeDeltaWhenOutputSliceMismatch();
    void TestComputeDeltaWhenInputPrefixMismatch();
    void TestComputeDeltaWhenSystemChanges();
    void TestComputeDeltaToolLoopWhenOutputHashMatches();
    void TestComputeDeltaT1ReplayWithoutFinishReason();
    void TestComputeDeltaOmitsSystem();
    void TestComputeDeltaFromNinWhenOutputHashMismatch();
    void TestComputeDeltaOutputReplayIgnoresPartsDifference();
    void TestComputeDeltaIgnoresToolNameForInputHash();
    void TestResolveSessionStateKey();
    void TestFormatGenAiStepId();
    void TestSystemInstructionsHashStable();
    void TestSystemInstructionsHashChanges();
    void TestToolDefinitionsHashStable();
    void TestToolDefinitionsHashChanges();
    void TestCommitSessionStateStoresSystemAndToolHashes();
};

void AgentsightMessageUtilUnittest::TestExtractSystemInstructions() {
    const std::string messages = R"([
      {"role":"system","parts":[{"type":"text","content":"sys-a"}]},
      {"role":"user","parts":[{"type":"text","content":"hi"}]}
    ])";
    const std::string out = ExtractSystemInstructionsJson(messages);
    APSARA_TEST_TRUE(out.find("sys-a") != std::string::npos);
    APSARA_TEST_TRUE(out.find("user") == std::string::npos);
}

void AgentsightMessageUtilUnittest::TestInputMessagesHashFirstRound() {
    const std::string full = R"([{"role":"user","parts":[{"type":"text","content":"hello"}]}])";
    AgentsightSessionInputState state;
    ApplyRoundState(full, "[]", state);
    APSARA_TEST_TRUE(!state.messagesHash.empty());
    APSARA_TEST_EQUAL(64UL, state.messagesHash.size());
}

void AgentsightMessageUtilUnittest::TestInputMessagesHashStableAcrossRounds() {
    const std::string full = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"assistant","parts":[{"type":"text","content":"b"}]},
      {"role":"user","parts":[{"type":"text","content":"c"}]}
    ])";

    AgentsightSessionInputState state1;
    ApplyRoundState(full, R"([{"role":"assistant","parts":[{"type":"text","content":"b"}]}])", state1);

    AgentsightSessionInputState state2;
    ApplyRoundState(full, R"([{"role":"assistant","parts":[{"type":"text","content":"b"}]}])", state2);
    APSARA_TEST_EQUAL(state1.messagesHash, state2.messagesHash);
}

void AgentsightMessageUtilUnittest::TestInputMessagesHashChangesWhenContentChanges() {
    const std::string full1 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"user","parts":[{"type":"text","content":"b"}]}
    ])";
    AgentsightSessionInputState state1;
    ApplyRoundState(full1, "[]", state1);

    const std::string full2 = R"([
      {"role":"user","parts":[{"type":"text","content":"COMPACT"}]},
      {"role":"user","parts":[{"type":"text","content":"b"}]}
    ])";
    AgentsightSessionInputState state2;
    ApplyRoundState(full2, "[]", state2);
    APSARA_TEST_TRUE(state1.messagesHash != state2.messagesHash);
}

void AgentsightMessageUtilUnittest::TestFormatFinishReasonsFromOutputMessages() {
    const std::string output = R"([
      {"role":"assistant","finish_reason":"tool_calls"},
      {"role":"assistant","finish_reason":"stop"}
    ])";
    APSARA_TEST_EQUAL(R"(["tool_calls","stop"])", FormatFinishReasonsJson(output, ""));
}

void AgentsightMessageUtilUnittest::TestFormatFinishReasonsFallback() {
    APSARA_TEST_EQUAL(R"(["stop"])", FormatFinishReasonsJson("[]", "stop"));
    APSARA_TEST_TRUE(FormatFinishReasonsJson("", "").empty());
}

void AgentsightMessageUtilUnittest::TestFormatFinishReasonsFromParts() {
    const std::string output = R"([
      {"role":"assistant","parts":[{"type":"text","content":"ok","finish_reason":"stop"}]}
    ])";
    APSARA_TEST_EQUAL(R"(["stop"])", FormatFinishReasonsJson(output, ""));
}

void AgentsightMessageUtilUnittest::TestFormatFinishReasonsFallbackAlwaysArray() {
    const std::string out = FormatFinishReasonsJson("", "tool_calls");
    APSARA_TEST_EQUAL(R"(["tool_calls"])", out);
    APSARA_TEST_TRUE(!out.empty() && out.front() == '[');
}

void AgentsightMessageUtilUnittest::TestComputeDeltaFirstRound() {
    const std::string cur = R"([{"role":"user","parts":[{"type":"text","content":"hello"}]}])";
    const std::string delta = ComputeInputMessagesDelta(cur, nullptr);
    APSARA_TEST_EQUAL(cur, delta);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaAfterOutputMatch() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"b"}]}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"assistant","parts":[{"type":"text","content":"b"}]},
      {"role":"user","parts":[{"type":"text","content":"c"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(R"([{"role":"user","parts":[{"type":"text","content":"c"}]}])", delta);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaWhenOutputSliceMismatch() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"b"}]}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"user","parts":[{"type":"text","content":"c"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(R"([{"role":"user","parts":[{"type":"text","content":"c"}]}])", delta);
}

// Multi-step tool loop: replay assistant matches H_out (role-only; parts/finish_reason ignored).
void AgentsightMessageUtilUnittest::TestComputeDeltaToolLoopWhenOutputHashMatches() {
    const std::string in1 = R"([
      {"role":"system","parts":[{"type":"text","content":"sys"}]},
      {"role":"user","parts":[{"type":"text","content":"看看io读利用率"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"call-io"}],"finish_reason":"tool_calls"}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"system","parts":[{"type":"text","content":"sys"}]},
      {"role":"user","parts":[{"type":"text","content":"看看io读利用率"}]},
      {"role":"assistant","parts":[{"type":"text","content":"call-io"}]},
      {"role":"tool","parts":[{"type":"tool_call_response","response":"io-stats"}]},
      {"role":"assistant","parts":[{"type":"text","content":"io-answer"}]},
      {"role":"user","parts":[{"type":"text","content":"看看网络利用率"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_TRUE(delta.find("system") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("看看io读利用率") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("call-io") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("io-stats") != std::string::npos);
    APSARA_TEST_TRUE(delta.find("看看网络利用率") != std::string::npos);
}

// H_out uses role-only normalization; finish_reason and parts on response do not affect replay match.
void AgentsightMessageUtilUnittest::TestComputeDeltaT1ReplayWithoutFinishReason() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"call-tool"}],"finish_reason":"tool_calls"}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]},
      {"role":"assistant","parts":[{"type":"text","content":"call-tool"}]},
      {"role":"tool","parts":[{"type":"tool_call_response","response":"tool-result"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(R"([{"role":"tool","parts":[{"type":"tool_call_response","response":"tool-result"}]}])", delta);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaFromNinWhenOutputHashMismatch() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"q1"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"a1"}],"finish_reason":"tool_calls"}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    // Replay slice role differs from stored output (assistant vs user) → H_out mismatch, delta from N_in.
    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"q1"}]},
      {"role":"user","parts":[{"type":"text","content":"unexpected-at-replay"}]},
      {"role":"tool","parts":[{"type":"tool_call_response","response":"t1"}]},
      {"role":"user","parts":[{"type":"text","content":"q2"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_TRUE(delta.find("unexpected-at-replay") != std::string::npos);
    APSARA_TEST_TRUE(delta.find("t1") != std::string::npos);
    APSARA_TEST_TRUE(delta.find("q2") != std::string::npos);
}

// OpenClaw-style replay: response tool_call id `call_<hex>` vs replay `call<hex>`; H_out is role-only.
void AgentsightMessageUtilUnittest::TestComputeDeltaOutputReplayIgnoresPartsDifference() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"tool_call","id":"call_abc123","name":"fn"}],"finish_reason":"tool_calls"}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]},
      {"role":"assistant","parts":[{"type":"tool_call","id":"callabc123","name":"fn"}]},
      {"role":"tool","parts":[{"type":"tool_call_response","response":"tool-result"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(R"([{"role":"tool","parts":[{"type":"tool_call_response","response":"tool-result"}]}])", delta);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaOmitsSystem() {
    const std::string cur = R"([
      {"role":"system","parts":[{"type":"text","content":"sys"}]},
      {"role":"user","parts":[{"type":"text","content":"hi"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(cur, nullptr);
    APSARA_TEST_TRUE(delta.find("system") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("hi") != std::string::npos);
}

// H_in ignores top-level tool `name`; only role+parts participate in prefix hash.
void AgentsightMessageUtilUnittest::TestComputeDeltaIgnoresToolNameForInputHash() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]},
      {"role":"assistant","parts":[{"type":"text","content":"call"}],"finish_reason":"tool_calls"},
      {"role":"tool","name":"fn_a","parts":[{"type":"tool_call_response","response":"r"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"call"}],"finish_reason":"tool_calls"}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"q"}]},
      {"role":"assistant","parts":[{"type":"text","content":"call"}]},
      {"role":"tool","name":"fn_b","parts":[{"type":"tool_call_response","response":"r"}]},
      {"role":"user","parts":[{"type":"text","content":"follow-up"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(R"([{"role":"user","parts":[{"type":"text","content":"follow-up"}]}])", delta);
}

void AgentsightMessageUtilUnittest::TestResolveSessionStateKey() {
    APSARA_TEST_EQUAL("sess", ResolveSessionStateKey("sess", "turn"));
    APSARA_TEST_EQUAL("sess-only", ResolveSessionStateKey("sess-only", ""));
    APSARA_TEST_EQUAL("turn-only", ResolveSessionStateKey("", "turn-only"));
    APSARA_TEST_EQUAL("", ResolveSessionStateKey("", ""));
}

void AgentsightMessageUtilUnittest::TestFormatGenAiStepId() {
    APSARA_TEST_EQUAL("turn-abc:s1", FormatGenAiStepId("turn-abc", 1));
    APSARA_TEST_EQUAL("278a5a71:s3", FormatGenAiStepId("278a5a71", 3));
    APSARA_TEST_TRUE(FormatGenAiStepId("", 1).empty());
}

void AgentsightMessageUtilUnittest::TestSystemInstructionsHashStable() {
    const std::string messages = R"([
      {"role":"system","parts":[{"type":"text","content":"sys-a"}]},
      {"role":"user","parts":[{"type":"text","content":"hi"}]}
    ])";
    const std::string hash1 = ComputeSystemInstructionsHash(messages);
    const std::string hash2 = ComputeSystemInstructionsHash(messages);
    APSARA_TEST_TRUE(!hash1.empty());
    APSARA_TEST_EQUAL(64UL, hash1.size());
    APSARA_TEST_EQUAL(hash1, hash2);
}

void AgentsightMessageUtilUnittest::TestSystemInstructionsHashChanges() {
    const std::string messages1 = R"([{"role":"system","parts":[{"type":"text","content":"a"}]}])";
    const std::string messages2 = R"([{"role":"system","parts":[{"type":"text","content":"b"}]}])";
    APSARA_TEST_TRUE(ComputeSystemInstructionsHash(messages1) != ComputeSystemInstructionsHash(messages2));
}

void AgentsightMessageUtilUnittest::TestToolDefinitionsHashStable() {
    const std::string tools = R"([{"type":"function","function":{"name":"read"}}])";
    const std::string hash1 = ComputeToolDefinitionsHash(tools);
    const std::string hash2 = ComputeToolDefinitionsHash(tools);
    APSARA_TEST_TRUE(!hash1.empty());
    APSARA_TEST_EQUAL(64UL, hash1.size());
    APSARA_TEST_EQUAL(hash1, hash2);
}

void AgentsightMessageUtilUnittest::TestToolDefinitionsHashChanges() {
    const std::string tools1 = R"([{"type":"function","function":{"name":"read"}}])";
    const std::string tools2 = R"([{"type":"function","function":{"name":"write"}}])";
    APSARA_TEST_TRUE(ComputeToolDefinitionsHash(tools1) != ComputeToolDefinitionsHash(tools2));
}

void AgentsightMessageUtilUnittest::TestCommitSessionStateStoresSystemAndToolHashes() {
    const std::string input = R"([
      {"role":"system","parts":[{"type":"text","content":"sys"}]},
      {"role":"user","parts":[{"type":"text","content":"q"}]}
    ])";
    const std::string tools = R"([{"type":"function","function":{"name":"fn"}}])";
    AgentsightSessionInputState state;
    ApplyRoundState(input, R"([{"role":"assistant","parts":[{"type":"text","content":"a"}]}])", state, tools);
    APSARA_TEST_EQUAL(1UL, state.messageCount);
    APSARA_TEST_EQUAL(ComputeSystemInstructionsHash(input), state.systemInstructionsHash);
    APSARA_TEST_EQUAL(ComputeToolDefinitionsHash(tools), state.toolDefinitionsHash);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaWhenInputPrefixMismatch() {
    const std::string in1 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"b"}]}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"RESET"}]},
      {"role":"user","parts":[{"type":"text","content":"c"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_EQUAL(
        R"([{"role":"user","parts":[{"type":"text","content":"RESET"}]},{"role":"user","parts":[{"type":"text","content":"c"}]}])",
        delta);
}

void AgentsightMessageUtilUnittest::TestComputeDeltaWhenSystemChanges() {
    const std::string in1 = R"([
      {"role":"system","parts":[{"type":"text","content":"heartbeat-system"}]},
      {"role":"user","parts":[{"type":"text","content":"a"}]}
    ])";
    const std::string out1 = R"([
      {"role":"assistant","parts":[{"type":"text","content":"HEARTBEAT_OK"}]}
    ])";
    AgentsightSessionInputState state;
    ApplyRoundState(in1, out1, state);

    const std::string in2 = R"([
      {"role":"system","parts":[{"type":"text","content":"webchat-system"}]},
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"assistant","parts":[{"type":"text","content":"HEARTBEAT_OK"}]},
      {"role":"user","parts":[{"type":"text","content":"今天天气如何"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_TRUE(delta.find("system") == std::string::npos);
    APSARA_TEST_TRUE(delta.find(R"("content":"a")") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("HEARTBEAT_OK") == std::string::npos);
    APSARA_TEST_TRUE(delta.find("今天天气如何") != std::string::npos);
}

UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestExtractSystemInstructions)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputMessagesHashFirstRound)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputMessagesHashStableAcrossRounds)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputMessagesHashChangesWhenContentChanges)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatFinishReasonsFromOutputMessages)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatFinishReasonsFallback)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatFinishReasonsFromParts)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatFinishReasonsFallbackAlwaysArray)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaFirstRound)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaAfterOutputMatch)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaWhenOutputSliceMismatch)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaWhenInputPrefixMismatch)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaWhenSystemChanges)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaToolLoopWhenOutputHashMatches)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaT1ReplayWithoutFinishReason)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaOmitsSystem)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaFromNinWhenOutputHashMismatch)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaOutputReplayIgnoresPartsDifference)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaIgnoresToolNameForInputHash)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestResolveSessionStateKey)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatGenAiStepId)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestSystemInstructionsHashStable)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestSystemInstructionsHashChanges)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestToolDefinitionsHashStable)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestToolDefinitionsHashChanges)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestCommitSessionStateStoresSystemAndToolHashes)

UNIT_TEST_MAIN
