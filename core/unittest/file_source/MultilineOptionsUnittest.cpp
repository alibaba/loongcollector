// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <json/json.h>

#include <memory>
#include <string>

#include "common/JsonUtil.h"
#include "file_server/MultilineOptions.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class MultilineOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;

private:
    const string pluginType = "test";
    PipelineContext ctx;
};

void MultilineOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<MultilineOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // only mandatory param
    config.reset(new MultilineOptions());
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("", config->mStartPattern);
    APSARA_TEST_EQUAL("", config->mContinuePattern);
    APSARA_TEST_EQUAL("", config->mEndPattern);
    APSARA_TEST_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_FALSE(config->IsMultiline());

    // valid optional param
    configStr = R"(
        {
            "Mode": "custom",
            "StartPattern": "\\d+:\\d+:\\d",
            "ContinuePattern": "aaa",
            "EndPattern": "\\S+",
            "UnmatchedContentTreatment": "single_line"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("\\d+:\\d+:\\d", config->mStartPattern);
    APSARA_TEST_EQUAL("aaa", config->mContinuePattern);
    APSARA_TEST_EQUAL("\\S+", config->mEndPattern);
    APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE, config->mUnmatchedContentTreatment);
    APSARA_TEST_NOT_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_NOT_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_TRUE(config->IsMultiline());

    // invalid optional param
    configStr = R"(
        {
            "Mode": true,
            "StartPattern": true,
            "ContinuePattern": true,
            "EndPattern": true,
            "UnmatchedContentTreatment": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("", config->mStartPattern);
    APSARA_TEST_EQUAL("", config->mContinuePattern);
    APSARA_TEST_EQUAL("", config->mEndPattern);
    APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE, config->mUnmatchedContentTreatment);
    APSARA_TEST_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_FALSE(config->IsMultiline());

    // mode
    configStr = R"(
        {
            "Mode": "JSON",
            "StartPattern": "\\d+:\\d+:\\d",
            "ContinuePattern": "",
            "EndPattern": "\\S+"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::JSON, config->mMode);
    APSARA_TEST_EQUAL("", config->mStartPattern);
    APSARA_TEST_EQUAL("", config->mContinuePattern);
    APSARA_TEST_EQUAL("", config->mEndPattern);
    APSARA_TEST_TRUE(config->IsMultiline());

    configStr = R"(
        {
            "Mode": "unknown",
            "StartPattern": "\\d+:\\d+:\\d",
            "ContinuePattern": "aaa",
            "EndPattern": "\\S+"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("\\d+:\\d+:\\d", config->mStartPattern);
    APSARA_TEST_EQUAL("aaa", config->mContinuePattern);
    APSARA_TEST_EQUAL("\\S+", config->mEndPattern);
    APSARA_TEST_TRUE(config->IsMultiline());

    // custom mode
    configStr = R"(
        {
            "StartPattern": ".*",
            "ContinuePattern": ".*",
            "EndPattern": ".*"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL(".*", config->mStartPattern);
    APSARA_TEST_EQUAL(".*", config->mContinuePattern);
    APSARA_TEST_EQUAL(".*", config->mEndPattern);
    APSARA_TEST_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_FALSE(config->IsMultiline());

    configStr = R"(
        {
            "StartPattern": "",
            "ContinuePattern": "",
            "EndPattern": ""
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("", config->mStartPattern);
    APSARA_TEST_EQUAL("", config->mContinuePattern);
    APSARA_TEST_EQUAL("", config->mEndPattern);
    APSARA_TEST_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_FALSE(config->IsMultiline());

    configStr = R"(
        {
            "StartPattern": "",
            "ContinuePattern": "aaa",
            "EndPattern": ""
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, config->mMode);
    APSARA_TEST_EQUAL("", config->mStartPattern);
    APSARA_TEST_EQUAL("aaa", config->mContinuePattern);
    APSARA_TEST_EQUAL("", config->mEndPattern);
    APSARA_TEST_EQUAL(nullptr, config->GetStartPatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetContinuePatternReg());
    APSARA_TEST_EQUAL(nullptr, config->GetEndPatternReg());
    APSARA_TEST_FALSE(config->IsMultiline());

    // UnmatchedContentTreatment
    configStr = R"(
        {
            "UnmatchedContentTreatment": "discard"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::DISCARD, config->mUnmatchedContentTreatment);

    configStr = R"(
        {
            "UnmatchedContentTreatment": "unknown"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config.reset(new MultilineOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE, config->mUnmatchedContentTreatment);
}

UNIT_TEST_CASE(MultilineOptionsUnittest, OnSuccessfulInit)

} // namespace logtail

UNIT_TEST_MAIN
