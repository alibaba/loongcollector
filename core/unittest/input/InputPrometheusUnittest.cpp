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

#include <json/json.h>

#include <filesystem>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "input/InputPrometheus.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "prometheus/PrometheusInputRunner.h"
#include "unittest/Unittest.h"
using namespace std;
namespace logtail {
class InputPrometheusUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();

protected:
    static void SetUpTestCase() { AppConfig::GetInstance()->mPurageContainerMode = true; }
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputPrometheusUnittest::OnSuccessfulInit() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    // only mandatory param
    configStr = R"(
        {
            "Type": "input_prometheus",
            "PrometheusGlobalConfig": [],
            "PrometheusScrapeConfig": [
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->scrapeJobs.empty());

    // with scrape job
    configStr = R"(
        {
            "Type": "input_prometheus",
            "PrometheusGlobalConfig": [],
            "PrometheusScrapeConfig": [
                {
                    "jobName": "_arms-prom/node-exporter/0",
                    "metricsPath": "/metrics",
                    "scheme": "http",
                    "scrapeInterval": 15,
                    "scrapeTimeout": 15,
                    "scrapeTargets": [
                        {
                            "host": "172.17.0.3:9100",
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("_arms-prom/node-exporter/0", input->scrapeJobs[0].jobName);
    APSARA_TEST_EQUAL("/metrics", input->scrapeJobs[0].metricsPath);
    APSARA_TEST_EQUAL(15, input->scrapeJobs[0].scrapeInterval);
    APSARA_TEST_EQUAL(15, input->scrapeJobs[0].scrapeTimeout);
    APSARA_TEST_EQUAL("172.17.0.3:9100", input->scrapeJobs[0].scrapeTargets[0].host);
    APSARA_TEST_EQUAL(9100, input->scrapeJobs[0].scrapeTargets[0].port);
}

void InputPrometheusUnittest::OnPipelineUpdate() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "input_prometheus",
            "PrometheusGlobalConfig": [],
            "PrometheusScrapeConfig": [
                {
                    "jobName": "_arms-prom/node-exporter/0",
                    "metricsPath": "/metrics",
                    "scheme": "http",
                    "scrapeInterval": 15,
                    "scrapeTimeout": 16,
                    "scrapeTargets": [
                        {
                            "host": "172.17.0.3:9100",
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputPrometheus());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_EQUAL((size_t)1, PrometheusInputRunner::GetInstance()->scrapeInputsMap["test_config"].size());

    APSARA_TEST_TRUE(input->Stop(true));
    APSARA_TEST_EQUAL((size_t)0, PrometheusInputRunner::GetInstance()->scrapeInputsMap["test_config"].size());
}

UNIT_TEST_CASE(InputPrometheusUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputPrometheusUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN