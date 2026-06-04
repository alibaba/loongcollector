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

void ApplyPlanToState(const AgentsightInputUploadPlan& plan, AgentsightSessionInputState& state) {
    state.messageCount = plan.inputMessageCount;
    state.messagesHash = plan.messagesHash;
}

} // namespace

class AgentsightMessageUtilUnittest : public testing::Test {
public:
    void TestExtractSystemInstructions();
    void TestInputUploadSkipsFullWhenPrefixStable();
    void TestInputUploadSendsFullWhenPrefixChanges();
    void TestInputUploadSendsFullWhenSessionNotInMap();
    void TestParseRequestParameters();
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

void AgentsightMessageUtilUnittest::TestInputUploadSendsFullWhenSessionNotInMap() {
    const std::string full = R"([{"role":"user","content":"hello"}])";
    const auto plan = PlanInputMessagesUpload(full, nullptr);
    APSARA_TEST_TRUE(plan.sendFullMessages);
    APSARA_TEST_EQUAL(1UL, plan.inputMessageCount);
}

void AgentsightMessageUtilUnittest::TestInputUploadSkipsFullWhenPrefixStable() {
    const std::string full = R"([
      {"role":"user","content":"a"},
      {"role":"assistant","content":"b"},
      {"role":"user","content":"c"}
    ])";

    AgentsightSessionInputState state;
    const auto first = PlanInputMessagesUpload(full, nullptr);
    APSARA_TEST_TRUE(first.sendFullMessages);
    ApplyPlanToState(first, state);

    const auto second = PlanInputMessagesUpload(full, &state);
    APSARA_TEST_FALSE(second.sendFullMessages);
}

void AgentsightMessageUtilUnittest::TestInputUploadSendsFullWhenPrefixChanges() {
    const std::string full1 = R"([
      {"role":"user","content":"a"},
      {"role":"user","content":"b"}
    ])";

    AgentsightSessionInputState state;
    const auto first = PlanInputMessagesUpload(full1, nullptr);
    ApplyPlanToState(first, state);

    const std::string full2 = R"([
      {"role":"user","content":"COMPACT"},
      {"role":"user","content":"b"}
    ])";
    const auto second = PlanInputMessagesUpload(full2, &state);
    APSARA_TEST_TRUE(second.sendFullMessages);
}

void AgentsightMessageUtilUnittest::TestParseRequestParameters() {
    const std::string json = R"({"temperature":0.2,"max_tokens":128,"top_p":0.9})";
    const auto p = ParseRequestParametersJson(json);
    APSARA_TEST_TRUE(p.temperature.has_value());
    APSARA_TEST_EQUAL("0.2", *p.temperature);
    APSARA_TEST_TRUE(p.maxTokens.has_value());
    APSARA_TEST_EQUAL("128", *p.maxTokens);
    APSARA_TEST_TRUE(p.topP.has_value());
}

UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestExtractSystemInstructions)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputUploadSendsFullWhenSessionNotInMap)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputUploadSkipsFullWhenPrefixStable)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestInputUploadSendsFullWhenPrefixChanges)
UNIT_TEST_CASE(AgentsightMessageUtilUnittest, TestParseRequestParameters)

UNIT_TEST_MAIN
