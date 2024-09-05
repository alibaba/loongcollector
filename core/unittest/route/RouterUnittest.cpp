// Copyright 2024 iLogtail Authors
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

#include "common/JsonUtil.h"
#include "pipeline/Pipeline.h"
#include "pipeline/route/Router.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class RouterUnittest : public testing::Test {
public:
    void TestInit();
    void TestRoute();

private:
    PipelineContext ctx;
};

void RouterUnittest::TestInit() {
    Json::Value configJson;
    string errorMsg;
    {
        string configStr = R"(
            [
                {
                    "Type": "event_type",
                    "Value": "log"
                }
            ]
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        vector<pair<size_t, const Json::Value*>> configs;
        for (Json::Value::ArrayIndex i = 0; i < configJson.size(); ++i) {
            configs.emplace_back(i, &configJson[i]);
        }

        Router router;
        APSARA_TEST_TRUE(router.Init(configs, ctx));
        APSARA_TEST_EQUAL(1U, router.mConditions.size());
        APSARA_TEST_EQUAL(0U, router.mConditions[0].first);
    }
    {
        string configStr = R"(
            [
                {
                    "Type": "event_type"
                }
            ]
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        vector<pair<size_t, const Json::Value*>> configs;
        for (Json::Value::ArrayIndex i = 0; i < configJson.size(); ++i) {
            configs.emplace_back(i, &configJson[i]);
        }

        Router router;
        APSARA_TEST_FALSE(router.Init(configs, ctx));
    }
    {
        string configStr = R"(
            [
                "unknown"
            ]
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        vector<pair<size_t, const Json::Value*>> configs;
        for (Json::Value::ArrayIndex i = 0; i < configJson.size(); ++i) {
            configs.emplace_back(i, &configJson[i]);
        }

        Router router;
        APSARA_TEST_FALSE(router.Init(configs, ctx));
    }
}

void RouterUnittest::TestRoute() {
    Json::Value configJson;
    string errorMsg;
    string configStr = R"(
        [
            {
                "Type": "event_type",
                "Value": "log"
            }
        ]
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    vector<pair<size_t, const Json::Value*>> configs;
    for (Json::Value::ArrayIndex i = 0; i < configJson.size(); ++i) {
        configs.emplace_back(i, &configJson[i]);
    }
    configs.emplace_back(configJson.size(), nullptr);

    Router router;
    router.Init(configs, ctx);
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.AddLogEvent();
        auto res = router.Route(g);
        APSARA_TEST_EQUAL(2U, res.size());
        APSARA_TEST_EQUAL(1U, res[0]);
        APSARA_TEST_EQUAL(0U, res[1]);
    }
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        g.AddMetricEvent();
        auto res = router.Route(g);
        APSARA_TEST_EQUAL(1U, res.size());
        APSARA_TEST_EQUAL(1U, res[0]);
    }
}

UNIT_TEST_CASE(RouterUnittest, TestInit)
UNIT_TEST_CASE(RouterUnittest, TestRoute)

} // namespace logtail

UNIT_TEST_MAIN
