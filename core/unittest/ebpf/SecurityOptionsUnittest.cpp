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
};

void SecurityOptionsUnittest::TestAgentsightNoProbeConfigReturnsTrue() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    Json::Value config;
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 0);
    APSARA_TEST_TRUE(opt.mLogPath.empty());
    APSARA_TEST_FALSE(opt.mEnableHttps);
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
    APSARA_TEST_TRUE(
        ParseJsonTable(R"({"ProbeConfig": {"Verbose": 2, "LogPath": "/tmp/as", "EnableHttps": true}})", config, err));
    APSARA_TEST_TRUE(opt.Init(SecurityProbeType::AGENTSIGHT_OBSERVE, config, &ctx, "input_agentsight"));
    APSARA_TEST_EQUAL(opt.mVerbose, 2);
    APSARA_TEST_EQUAL(opt.mLogPath, "/tmp/as");
    APSARA_TEST_TRUE(opt.mEnableHttps);
    // AGENTSIGHT branch returns before mOptionList.emplace_back (see Config.cpp).
    APSARA_TEST_EQUAL(0UL, opt.mOptionList.size());
}

void SecurityOptionsUnittest::TestAgentsightProbeConfigOptionalInvalidTypes() {
    CollectionPipelineContext ctx;
    ctx.SetConfigName("cfg1");
    SecurityOptions opt;
    std::string err;
    Json::Value config;
    APSARA_TEST_TRUE(
        ParseJsonTable(R"({"ProbeConfig": {"Verbose": "bad", "LogPath": 1, "EnableHttps": "nope"}})", config, err));
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

UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightNoProbeConfigReturnsTrue)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigWrongTypeWarns)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigParsesOptionalFields)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightProbeConfigOptionalInvalidTypes)
UNIT_TEST_CASE(SecurityOptionsUnittest, TestAgentsightVerboseClampedWhenNegative)

UNIT_TEST_MAIN
