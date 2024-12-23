/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MetricEvent.h"
#include "PrometheusInputRunner.h"
#include "common/JsonUtil.h"
#include "config/InstanceConfig.h"
#include "config/InstanceConfigManager.h"
#include "models/PipelineEventGroup.h"
#include "plugin/processor/inner/ProcessorPromDropMetricNative.h"
#include "prometheus/labels/TextParser.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ProcessorDropPrometheusMetricUnittest : public testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcess();

    PipelineContext mContext;
};

void ProcessorDropPrometheusMetricUnittest::TestInit() {
    Json::Value config;
    ProcessorPromDropMetricNative processor;
    processor.SetContext(mContext);

    // success config
    string configStr;
    string errorMsg;
    configStr = R"JSON(
        {
            "job_name": "test_job"
        }
    )JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));
}

void ProcessorDropPrometheusMetricUnittest::TestProcess() {
    // make config
    Json::Value config;

    ProcessorPromDropMetricNative processor;
    processor.SetContext(mContext);

    string configStr;
    string errorMsg;
    configStr = configStr + R"(
        {
            "job_name": "test_job"
        }
    )";

    // init
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));

    // make events
    auto parser = TextParser();
    string rawData = R"""(
                # begin

                test_metric1{k1="v1", k2="v2"} 1.0
                test_metric2{k1="v1", k2="v2"} 2.0 1234567890
                test_metric3{k1="v1",k2="v2"} 9.9410452992e+10
                test_metric4{k1="v1",k2="v2"} 9.9410452992e+10 1715829785083
                test_metric5{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083
                test_metric6{k1="v1",k2="v2",} 9.9410452992e+10 1715829785083
                test_metric7{k1="v1",k3="2", } 9.9410452992e+10 1715829785083  
                test_metric8{k1="v1", k3="v2", } 9.9410452992e+10 1715829785083

                # end
                    )""";
    auto eventGroup = parser.Parse(rawData, 0, 0);
    // run function
    APSARA_TEST_EQUAL((size_t)8, eventGroup.GetEvents().size());
    processor.Process(eventGroup);

    // judge result
    APSARA_TEST_EQUAL((size_t)8, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric3", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric5", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(6).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric8", eventGroup.GetEvents().at(7).Cast<MetricEvent>().GetName());

    // then update drop metrics
    prom::PrometheusServer::GetInstance()->Init();
    // Added
    {
        InstanceConfigDiff configDiff;
        {
            std::string content = R"({
                "prom_drop_metrics_list": "test_metric3"
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test0", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }

        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(1U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test0"));
    }
    // judge result
    APSARA_TEST_EQUAL(true, prom::PrometheusServer::GetInstance()->mGlobalConfig->HasDropMetrics());
    APSARA_TEST_EQUAL(true, prom::PrometheusServer::GetInstance()->mGlobalConfig->IsDropped(string("test_metric3")));
    processor.Process(eventGroup);

    APSARA_TEST_EQUAL((size_t)7, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName().to_string());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric5", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric8", eventGroup.GetEvents().at(6).Cast<MetricEvent>().GetName());
    // Modified
    {
        InstanceConfigDiff configDiff;
        {
            std::string content = R"({
                "prom_drop_metrics_list": "test_metric5"
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test0", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(1U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test0"));
    }
    APSARA_TEST_EQUAL(true, prom::PrometheusServer::GetInstance()->mGlobalConfig->HasDropMetrics());
    APSARA_TEST_EQUAL(true, prom::PrometheusServer::GetInstance()->mGlobalConfig->IsDropped(string("test_metric5")));
    processor.Process(eventGroup);

    APSARA_TEST_EQUAL((size_t)6, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric8", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    // Removed
    {
        InstanceConfigDiff configDiff;
        configDiff.mRemoved.emplace_back("test0");
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(0U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test0"));
    }
    APSARA_TEST_EQUAL(false, prom::PrometheusServer::GetInstance()->mGlobalConfig->HasDropMetrics());
    processor.Process(eventGroup);

    APSARA_TEST_EQUAL((size_t)6, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric8", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    prom::PrometheusServer::GetInstance()->Stop();
}

UNIT_TEST_CASE(ProcessorDropPrometheusMetricUnittest, TestInit)
UNIT_TEST_CASE(ProcessorDropPrometheusMetricUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN