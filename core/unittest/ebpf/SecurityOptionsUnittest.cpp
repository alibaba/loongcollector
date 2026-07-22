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
    void TestAgentsightProbeConfigParsesHttpsAndHttp();
    void TestAgentsightProbeConfigHttpsHttpInvalidTypes();
    void TestAgentsightEventStreamFormatParse();
    void TestAgentsightMessageDeltaOnlyDefaultAndParse();
    void TestAgentsightEnableRawHttpsDefaultAndParse();
};

void SecurityOptionsUnittest::TestAgentsightNoProbeConfigFallsBackToBuiltin() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    Json::Value config;
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
    APSARA_TEST_TRUE(opt.mAgentsightMessageDeltaOnly);
    APSARA_TEST_TRUE(opt.mAgentsightEventStreamFormat);
    APSARA_TEST_FALSE(opt.mAgentsightEnableRawHttps);
}

void SecurityOptionsUnittest::TestAgentsightMessageDeltaOnlyDefaultAndParse() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"MessageDeltaOnly":false}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_FALSE(opt.mAgentsightMessageDeltaOnly);

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"MessageDeltaOnly":true}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightMessageDeltaOnly);
}

void SecurityOptionsUnittest::TestAgentsightEnableRawHttpsDefaultAndParse() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    // Default when absent: false (LLM-only collection).
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_FALSE(opt.mAgentsightEnableRawHttps);

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"EnableRawHttps":true}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightEnableRawHttps);

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"EnableRawHttps":false}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_FALSE(opt.mAgentsightEnableRawHttps);
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
        R"({"ProbeConfig":{"Verbose":2,"LogPath":"/tmp/as","CmdlineWhitelist":[{"AgentType":"openclaw","Args":["node*","*openclaw*"]}]}})",
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
        R"({"ProbeConfig":{"Verbose":"bad","LogPath":1,"CmdlineWhitelist":[{"AgentType":"openclaw","Args":["node*"]}]}})",
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
        R"({"ProbeConfig":{"Verbose":-9,"CmdlineWhitelist":[{"AgentType":"openclaw","Args":["*openclaw*"]}]}})",
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
        R"({"ProbeConfig":{"CmdlineWhitelist":[{"AgentType":"openclaw","Args":["node*","*openclaw*"]},{"AgentType":"hermes","Args":["*python*","*hermes*"]}]}})",
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
        R"({"ProbeConfig":{"CmdlineWhitelist":[{"AgentType":"OpenClaw","Args":["node*","*openclaw*"]},{"AgentType":"my_custom_agent","Args":["*foo*"]}]}})",
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

void SecurityOptionsUnittest::TestAgentsightProbeConfigParsesHttpsAndHttp() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Https":["*.openai.com","api.anthropic.com"],"Http":[":8080","10.0.0.1:9090","model-svc.default.svc","*.internal.svc"]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightHttps.size());
    APSARA_TEST_EQUAL("*.openai.com", opt.mAgentsightHttps[0]);
    APSARA_TEST_EQUAL("api.anthropic.com", opt.mAgentsightHttps[1]);
    APSARA_TEST_EQUAL(4UL, opt.mAgentsightHttp.size());
    APSARA_TEST_EQUAL(":8080", opt.mAgentsightHttp[0]);
    APSARA_TEST_EQUAL("10.0.0.1:9090", opt.mAgentsightHttp[1]);
    APSARA_TEST_EQUAL("model-svc.default.svc", opt.mAgentsightHttp[2]);
    APSARA_TEST_EQUAL("*.internal.svc", opt.mAgentsightHttp[3]);
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigHttpsHttpInvalidTypes() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"Https":"not-array","Http":[123,":80"]}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightHttps.empty());
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightHttp.size());
    APSARA_TEST_EQUAL(":80", opt.mAgentsightHttp[0]);
}

void SecurityOptionsUnittest::TestAgentsightEventStreamFormatParse() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    Json::Value config;
    std::string err;
    SecurityOptions opt;

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"EventStreamFormat":true}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightEventStreamFormat);

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{"EventStreamFormat":false}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_FALSE(opt.mAgentsightEventStreamFormat);

    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig":{}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_TRUE(opt.mAgentsightEventStreamFormat);
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
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigParsesHttpsAndHttp)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigHttpsHttpInvalidTypes)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightEventStreamFormatParse)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightMessageDeltaOnlyDefaultAndParse)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightEnableRawHttpsDefaultAndParse)

UNIT_TEST_MAIN
