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

void ApplyRoundState(const std::string& inputJson, const std::string& outputJson, AgentsightSessionInputState& state) {
    CommitSessionStateAfterEmit(inputJson, outputJson, state);
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
    void TestComputeDeltaToolLoopWhenOutputHashMatches();
    void TestComputeDeltaT1ReplayWithoutFinishReason();
    void TestComputeDeltaOmitsSystem();
    void TestComputeDeltaFromNinWhenOutputHashMismatch();
    void TestComputeDeltaIgnoresToolNameForInputHash();
    void TestResolveSessionStateKey();
    void TestFormatGenAiStepId();
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
    const std::string hash = ComputeInputMessagesHash(full);
    APSARA_TEST_TRUE(!hash.empty());
    APSARA_TEST_EQUAL(64UL, hash.size());
}

void AgentsightMessageUtilUnittest::TestInputMessagesHashStableAcrossRounds() {
    const std::string full = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"assistant","parts":[{"type":"text","content":"b"}]},
      {"role":"user","parts":[{"type":"text","content":"c"}]}
    ])";

    const std::string firstHash = ComputeInputMessagesHash(full);
    AgentsightSessionInputState state;
    ApplyRoundState(full, R"([{"role":"assistant","parts":[{"type":"text","content":"b"}]}])", state);

    const std::string secondHash = ComputeInputMessagesHash(full);
    APSARA_TEST_EQUAL(firstHash, secondHash);
}

void AgentsightMessageUtilUnittest::TestInputMessagesHashChangesWhenContentChanges() {
    const std::string full1 = R"([
      {"role":"user","parts":[{"type":"text","content":"a"}]},
      {"role":"user","parts":[{"type":"text","content":"b"}]}
    ])";
    const std::string hash1 = ComputeInputMessagesHash(full1);

    const std::string full2 = R"([
      {"role":"user","parts":[{"type":"text","content":"COMPACT"}]},
      {"role":"user","parts":[{"type":"text","content":"b"}]}
    ])";
    const std::string hash2 = ComputeInputMessagesHash(full2);
    APSARA_TEST_TRUE(hash1 != hash2);
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

// Multi-step tool loop: replay assistant matches H_out (role+parts; finish_reason ignored).
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

// H_out uses role+parts normalization; finish_reason on response does not affect replay match.
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

    const std::string in2 = R"([
      {"role":"user","parts":[{"type":"text","content":"q1"}]},
      {"role":"assistant","parts":[{"type":"text","content":"a1-changed"}]},
      {"role":"tool","parts":[{"type":"tool_call_response","response":"t1"}]},
      {"role":"user","parts":[{"type":"text","content":"q2"}]}
    ])";
    const std::string delta = ComputeInputMessagesDelta(in2, &state);
    APSARA_TEST_TRUE(delta.find("a1-changed") != std::string::npos);
    APSARA_TEST_TRUE(delta.find("t1") != std::string::npos);
    APSARA_TEST_TRUE(delta.find("q2") != std::string::npos);
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
    APSARA_TEST_EQUAL("step_1", FormatGenAiStepId(1));
    APSARA_TEST_EQUAL("step_3", FormatGenAiStepId(3));
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
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaToolLoopWhenOutputHashMatches)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaT1ReplayWithoutFinishReason)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaOmitsSystem)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaFromNinWhenOutputHashMismatch)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestComputeDeltaIgnoresToolNameForInputHash)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestResolveSessionStateKey)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestFormatGenAiStepId)

UNIT_TEST_MAIN
