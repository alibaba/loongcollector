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
    void TestAgentsightNoProbeConfigReturnsTrue();
    void TestAgentsightProbeConfigWrongTypeWarns();
    void TestAgentsightProbeConfigParsesOptionalFields();
    void TestAgentsightProbeConfigOptionalInvalidTypes();
    void TestAgentsightVerboseClampedWhenNegative();
    void TestAgentsightParsesFlatCmdlineAndDomainWhitelist();
    void TestAgentsightLegacyNestedCmdlineAndDomainRules();
    void TestAgentsightFlatCmdlineOverridesLegacyNested();
    void TestAgentsightDomainWhitelistOverridesDomainRules();
};

void SecurityOptionsUnittest::TestAgentsightNoProbeConfigReturnsTrue() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    Json::Value config;
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 0);
    APSARA_TEST_TRUE(opt.mLogPath.empty());
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineWhitelist.empty());
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineBlacklist.empty());
    APSARA_TEST_TRUE(opt.mAgentsightDomainWhitelist.empty());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigWrongTypeWarns() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig": "not_a_map"})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigParsesOptionalFields() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig": {"Verbose": 2, "LogPath": "/tmp/as"}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 2);
    APSARA_TEST_EQUAL(opt.mLogPath, "/tmp/as");
    // AGENTSIGHT branch returns before mOptionList.emplace_back (see Config.cpp).
    APSARA_TEST_EQUAL(0UL, opt.mOptionList.size());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigOptionalInvalidTypes() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig": {"Verbose": "bad", "LogPath": 1}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(0UL, opt.mOptionList.size());
}

void SecurityOptionsUnittest::TestAgentsightVerboseClampedWhenNegative() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"ProbeConfig": {"Verbose": -9}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 0);
}

void SecurityOptionsUnittest::TestAgentsightParsesFlatCmdlineAndDomainWhitelist() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Verbose":0,"LogPath":"","CmdlineWhitelist":[["node","*claude*"],["npm","run","*"]],"CmdlineBlacklist":[["node","*webpack*"]],"DomainWhitelist":["*.openai.com","*.anthropic.com"]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightCmdlineWhitelist[0].size());
    APSARA_TEST_EQUAL("node", opt.mAgentsightCmdlineWhitelist[0][0]);
    APSARA_TEST_EQUAL("*claude*", opt.mAgentsightCmdlineWhitelist[0][1]);
    APSARA_TEST_EQUAL(3UL, opt.mAgentsightCmdlineWhitelist[1].size());
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightCmdlineBlacklist.size());
    APSARA_TEST_EQUAL("*webpack*", opt.mAgentsightCmdlineBlacklist[0][1]);
    APSARA_TEST_EQUAL(2UL, opt.mAgentsightDomainWhitelist.size());
    APSARA_TEST_EQUAL("*.openai.com", opt.mAgentsightDomainWhitelist[0]);
}

void SecurityOptionsUnittest::TestAgentsightLegacyNestedCmdlineAndDomainRules() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"Verbose":0,"LogPath":"","CmdlineRules":{"whitelist":[["a","b"]],"blacklist":[["c","d"]]},"DomainRules":["x.y"]}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightCmdlineBlacklist.size());
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightDomainWhitelist.size());
    APSARA_TEST_EQUAL("x.y", opt.mAgentsightDomainWhitelist[0]);
}

void SecurityOptionsUnittest::TestAgentsightFlatCmdlineOverridesLegacyNested() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"CmdlineWhitelist":[["from","flat"]],"CmdlineRules":{"whitelist":[["from","nested"]]}}})",
        config,
        err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightCmdlineWhitelist.size());
    APSARA_TEST_EQUAL("from", opt.mAgentsightCmdlineWhitelist[0][0]);
    APSARA_TEST_EQUAL("flat", opt.mAgentsightCmdlineWhitelist[0][1]);
    APSARA_TEST_TRUE(opt.mAgentsightCmdlineBlacklist.empty());
}

void SecurityOptionsUnittest::TestAgentsightDomainWhitelistOverridesDomainRules() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"ProbeConfig":{"DomainWhitelist":["only.this"],"DomainRules":["ignored.example"]}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(1UL, opt.mAgentsightDomainWhitelist.size());
    APSARA_TEST_EQUAL("only.this", opt.mAgentsightDomainWhitelist[0]);
}

UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightNoProbeConfigReturnsTrue)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigWrongTypeWarns)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigParsesOptionalFields)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigOptionalInvalidTypes)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightVerboseClampedWhenNegative)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightParsesFlatCmdlineAndDomainWhitelist)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightLegacyNestedCmdlineAndDomainRules)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightFlatCmdlineOverridesLegacyNested)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightDomainWhitelistOverridesDomainRules)

UNIT_TEST_MAIN
