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

#include <filesystem>
#include <memory>
#include <string>

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
#include "monitor/Monitor.h"
#include "plugin/input/InputInternalMetrics.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);

using namespace std;

namespace logtail {

class InputInternalMetricsUnittest : public testing::Test {
public:
    void OnInit();
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() {
        LoongCollectorMonitor::GetInstance()->Init();
        PluginRegistry::GetInstance()->LoadPlugins();
    }

    static void TearDownTestCase() {
        PluginRegistry::GetInstance()->UnloadPlugins();
        LoongCollectorMonitor::GetInstance()->Stop();
    }

    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        p.mPluginID.store(0);
        ctx.SetPipeline(p);
    }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputInternalMetricsUnittest::OnInit() {
    unique_ptr<InputInternalMetrics> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_internal_metrics",
            "Agent": {
                "Enable": true,
                "Interval": 1
            },
            "Runner": {
                "Enable": false,
                "Interval": 2
            },
            "Pipeline": {
                "Enable": true,
                "Interval": 3
            },
            "PluginSource": {
                "Enable": true,
                "Interval": 4
            },
            "Plugin": {
                "Enable": true,
                "Interval": 5
            },
            "Component": {
                "Enable": false,
                "Interval": 6
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputInternalMetrics());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputInternalMetrics::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mAgentMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mAgentMetricsRule.mInterval, 1);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mComponentMetricsRule.mEnable, false);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mComponentMetricsRule.mInterval, 6);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPipelineMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPipelineMetricsRule.mInterval, 3);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPluginMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPluginMetricsRule.mInterval, 5);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPluginSourceMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mPluginSourceMetricsRule.mInterval, 4);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mRunnerMetricsRule.mEnable, false);
    APSARA_TEST_EQUAL(input->mSelfMonitorMetricRules.mRunnerMetricsRule.mInterval, 2);
    APSARA_TEST_TRUE(input->Stop(true));
}

void InputInternalMetricsUnittest::OnPipelineUpdate() {
    Json::Value configJson, optionalGoPipeline;
    InputInternalMetrics input;
    input.SetContext(ctx);
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_internal_metrics",
            "Agent": {
                "Enable": false,
                "Interval": 7
            },
            "Runner": {
                "Enable": true,
                "Interval": 8
            },
            "Pipeline": {
                "Enable": false,
                "Interval": 9
            },
            "PluginSource": {
                "Enable": false,
                "Interval": 10
            },
            "Plugin": {
                "Enable": false,
                "Interval": 11
            },
            "Component": {
                "Enable": true,
                "Interval": 12
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputInternalMetrics::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();

    APSARA_TEST_TRUE(input.Start());
    APSARA_TEST_NOT_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mAgentMetricsRule.mEnable, false);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mAgentMetricsRule.mInterval, 7);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mComponentMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mComponentMetricsRule.mInterval, 12);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPipelineMetricsRule.mEnable, false);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPipelineMetricsRule.mInterval, 9);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPluginMetricsRule.mEnable, false);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPluginMetricsRule.mInterval, 11);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPluginSourceMetricsRule.mEnable,
                      false);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mPluginSourceMetricsRule.mInterval,
                      10);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mRunnerMetricsRule.mEnable, true);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules->mRunnerMetricsRule.mInterval, 8);

    APSARA_TEST_TRUE(input.Stop(true));
    APSARA_TEST_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mSelfMonitorMetricRules);
    APSARA_TEST_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mMetricPipelineCtx);
}

UNIT_TEST_CASE(InputInternalMetricsUnittest, OnInit)
UNIT_TEST_CASE(InputInternalMetricsUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
