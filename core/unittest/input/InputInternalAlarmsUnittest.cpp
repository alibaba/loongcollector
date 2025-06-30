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
#include "monitor/SelfMonitorServer.h"
#include "plugin/input/InputInternalAlarms.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);

using namespace std;

namespace logtail {

class InputInternalAlarmsUnittest : public testing::Test {
public:
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

void InputInternalAlarmsUnittest::OnPipelineUpdate() {
    Json::Value configJson, optionalGoPipeline;
    InputInternalAlarms input;
    input.SetContext(ctx);
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_internal_alarms"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputInternalAlarms::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();

    APSARA_TEST_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mAlarmPipelineCtx);
    APSARA_TEST_TRUE(input.Start());
    APSARA_TEST_NOT_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mAlarmPipelineCtx);
    APSARA_TEST_TRUE(input.Stop(true));
    APSARA_TEST_EQUAL(nullptr, SelfMonitorServer::GetInstance()->mAlarmPipelineCtx);
}

UNIT_TEST_CASE(InputInternalAlarmsUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
