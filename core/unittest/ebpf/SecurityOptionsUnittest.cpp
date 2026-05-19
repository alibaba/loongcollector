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

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "ebpf/Config.h"
#include "unittest/Unittest.h"

using namespace logtail;
using namespace logtail::ebpf;

class SecurityOptionsUnittest : public testing::Test {
public:
    void TestAgentsightNoProbeConfigFallsBackToBuiltin();
    void TestAgentsightProbeConfigWrongTypeFallsBackToBuiltin();
    void TestAgentsightProbeConfigParsesOptionalFields();
    void TestAgentsightProbeConfigOptionalInvalidTypes();
    void TestAgentsightVerboseClampedWhenNegative();
    void TestAgentsightMissingCmdlineWhitelistFallsBackToBuiltin();
    void TestAgentsightParsesCmdlineWhitelistWithAgentType();
    void TestAgentsightAcceptsAnyAgentTypeShape();
    void TestAgentsightRejectsLegacyLowercaseKeys();
    void TestAgentsightRejectsEmptyCmdlineWhitelistArray();
    void TestAgentsightIgnoresLegacyCmdlineRulesAndDomainRules();
};

void SecurityOptionsUnittest::TestAgentsightNoProbeConfigFallsBackToBuiltin() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    Json::Value config;
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigWrongTypeFallsBackToBuiltin() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig": "not_a_map"})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigParsesOptionalFields() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Verbose":2,"LogPath":"/tmp/as","CmdlineWhitelist":[{"AgentType":"openclaw","Rule":["node*","*openclaw*"]}]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 2);
    APSARA_TEST_EQUAL(opt.mLogPath, "/tmp/as");
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL(0UL, opt.mOptionList.size());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigOptionalInvalidTypes() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Verbose":"bad","LogPath":1,"CmdlineWhitelist":[{"AgentType":"openclaw","Rule":["node*"]}]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(0UL, opt.mOptionList.size());
}

void SecurityOptionsUnittest::TestAgentsightVerboseClampedWhenNegative() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Verbose":-9,"CmdlineWhitelist":[{"AgentType":"openclaw","Rule":["*openclaw*"]}]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 0);
}

void SecurityOptionsUnittest::TestAgentsightMissingCmdlineWhitelistFallsBackToBuiltin() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"Verbose":0,"LogPath":""}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
}

void SecurityOptionsUnittest::TestAgentsightRejectsEmptyCmdlineWhitelistArray() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"CmdlineWhitelist":[]}})", config, err));
    APSARA_TEST_FALSE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
}

void SecurityOptionsUnittest::TestAgentsightParsesCmdlineWhitelistWithAgentType() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"CmdlineWhitelist":[{"AgentType":"openclaw","Rule":["node*","*openclaw*"]},{"AgentType":"hermes","Rule":["*python*","*hermes*"]}]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL("openclaw", opt.mAgentsightCmdlineWhitelist[0].agentType);
    APSARA_TEST_EQUAL("node*", opt.mAgentsightCmdlineWhitelist[0].patterns[0]);
    APSARA_TEST_EQUAL("*openclaw*", opt.mAgentsightCmdlineWhitelist[0].patterns[1]);
    APSARA_TEST_EQUAL("hermes", opt.mAgentsightCmdlineWhitelist[1].agentType);
}

void SecurityOptionsUnittest::TestAgentsightAcceptsAnyAgentTypeShape() {
    // AgentType is a free-form non-empty string; we recommend lowercase+hyphen but do not enforce.
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"CmdlineWhitelist":[{"AgentType":"OpenClaw","Rule":["node*","*openclaw*"]},{"AgentType":"my_custom_agent","Rule":["*foo*"]}]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL("OpenClaw", opt.mAgentsightCmdlineWhitelist[0].agentType);
    APSARA_TEST_EQUAL("my_custom_agent", opt.mAgentsightCmdlineWhitelist[1].agentType);
}

void SecurityOptionsUnittest::TestAgentsightRejectsLegacyLowercaseKeys() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"CmdlineWhitelist":[{"agent_type":"openclaw","rule":["node*","*openclaw*"]}]}})",
        config,
        err));
    APSARA_TEST_FALSE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
}

void SecurityOptionsUnittest::TestAgentsightIgnoresLegacyCmdlineRulesAndDomainRules() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"CmdlineRules":{"whitelist":[["a","b"]],"blacklist":[["c","d"]]},"DomainRules":["x.y"]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineBlacklist.empty());
    APSARA_TEST_TRUE(opt.mAgentsightDomainWhitelist.empty());
}

UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightNoProbeConfigFallsBackToBuiltin)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigWrongTypeFallsBackToBuiltin)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigParsesOptionalFields)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigOptionalInvalidTypes)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightVerboseClampedWhenNegative)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightMissingCmdlineWhitelistFallsBackToBuiltin)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightParsesCmdlineWhitelistWithAgentType)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightAcceptsAnyAgentTypeShape)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightRejectsLegacyLowercaseKeys)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightRejectsEmptyCmdlineWhitelistArray)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightIgnoresLegacyCmdlineRulesAndDomainRules)

UNIT_TEST_MAIN
