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

#include <cstdint>
#include <memory>
#include <string>

#include "PluginRegistry.h"
#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "inner/ProcessorPromParseMetricNative.h"
#include "inner/ProcessorPromRelabelMetricNative.h"
#include "input/InputPrometheus.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "prometheus/PrometheusInputRunner.h"
#include "prometheus/labels/Relabel.h"
#include "unittest/Unittest.h"


using namespace std;
namespace logtail {

class InputPrometheusUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();
    void TestCreateInnerProcessor();

protected:
    static void SetUpTestCase() {
        setenv("POD_NAME", "prometheus-test", 1);
        setenv("OPERATOR_HOST", "127.0.0.1", 1);
        setenv("OPERATOR_PORT", "12345", 1);

        AppConfig::GetInstance()->mPurageContainerMode = true;
        PluginRegistry::GetInstance()->LoadPlugins();
    }
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
    }
    static void TearDownTestCase() {
        unsetenv("POD_NAME");
        unsetenv("OPERATOR_HOST");
        unsetenv("OPERATOR_PORT");
        PluginRegistry::GetInstance()->UnloadPlugins();
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputPrometheusUnittest::OnSuccessfulInit() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;
    // only mandatory param
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input = make_unique<InputPrometheus>();
    input->SetContext(ctx);
    input->SetMetricsRecordRef(input->Name(), "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    APSARA_TEST_EQUAL("_arms-prom/node-exporter/0", input->mTargetSubscirber->mJobName);
    APSARA_TEST_EQUAL("/metrics", input->mTargetSubscirber->mScrapeConfigPtr->mMetricsPath);
    APSARA_TEST_EQUAL(15LL, input->mTargetSubscirber->mScrapeConfigPtr->mScrapeIntervalSeconds);
    APSARA_TEST_EQUAL(15LL, input->mTargetSubscirber->mScrapeConfigPtr->mScrapeTimeoutSeconds);
    APSARA_TEST_EQUAL(-1, input->mTargetSubscirber->mScrapeConfigPtr->mMaxScrapeSizeBytes);
    APSARA_TEST_EQUAL(-1, input->mTargetSubscirber->mScrapeConfigPtr->mSampleLimit);
    APSARA_TEST_EQUAL(-1, input->mTargetSubscirber->mScrapeConfigPtr->mSeriesLimit);

    // all useful config
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ],
                "max_scrape_size": "10MiB",
                "sample_limit": 1000000,
                "series_limit": 1000000
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input = make_unique<InputPrometheus>();
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    APSARA_TEST_EQUAL("_arms-prom/node-exporter/0", input->mTargetSubscirber->mJobName);
    APSARA_TEST_EQUAL("/metrics", input->mTargetSubscirber->mScrapeConfigPtr->mMetricsPath);
    APSARA_TEST_EQUAL(15, input->mTargetSubscirber->mScrapeConfigPtr->mScrapeIntervalSeconds);
    APSARA_TEST_EQUAL(15, input->mTargetSubscirber->mScrapeConfigPtr->mScrapeTimeoutSeconds);
    APSARA_TEST_EQUAL(10 * 1024 * 1024, input->mTargetSubscirber->mScrapeConfigPtr->mMaxScrapeSizeBytes);
    APSARA_TEST_EQUAL(1000000, input->mTargetSubscirber->mScrapeConfigPtr->mSampleLimit);
    APSARA_TEST_EQUAL(1000000, input->mTargetSubscirber->mScrapeConfigPtr->mSeriesLimit);
}

void InputPrometheusUnittest::OnFailedInit() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;
    // only mandatory param
    configStr = R"(
        {
            "Type": "input_prometheus",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input = make_unique<InputPrometheus>();
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));

    // with invalid ScrapeConfig
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input = make_unique<InputPrometheus>();
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
}

void InputPrometheusUnittest::OnPipelineUpdate() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;
    configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input = make_unique<InputPrometheus>();
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("_arms-prom/node-exporter/0")
                     != PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());

    APSARA_TEST_TRUE(input->Stop(true));
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("_arms-prom/node-exporter/0")
                     == PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());

}

void InputPrometheusUnittest::TestCreateInnerProcessor() {
    unique_ptr<InputPrometheus> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;
    {
        // only mandatory param
        configStr = R"(
        {
            "Type": "input_prometheus",
            "ScrapeConfig": {
                "job_name": "_arms-prom/node-exporter/0",
                "metrics_path": "/metrics",
                "scheme": "http",
                "scrape_interval": "15s",
                "scrape_timeout": "15s",
                "scrape_targets": [
                    {
                        "host": "172.17.0.3:9100",
                    }
                ]
            }
        }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        input = make_unique<InputPrometheus>();
        input->SetContext(ctx);
        input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");

        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

        APSARA_TEST_EQUAL(2U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorPromParseMetricNative::sName, input->mInnerProcessors[0]->Name());
        APSARA_TEST_EQUAL(ProcessorPromRelabelMetricNative::sName, input->mInnerProcessors[1]->Name());
        APSARA_TEST_EQUAL(0U,
                          dynamic_cast<ProcessorPromRelabelMetricNative*>(input->mInnerProcessors[1]->mPlugin.get())
                              ->mRelabelConfigs.size());
    }
    {
        // with metric relabel config
        configStr = R"JSON(
            {
                "Type": "input_prometheus",
                "ScrapeConfig": {
                    "enable_http2": true,
                    "follow_redirects": true,
                    "honor_timestamps": false,
                    "job_name": "_kube-state-metrics",
                    "kubernetes_sd_configs": [
                        {
                            "enable_http2": true,
                            "follow_redirects": true,
                            "kubeconfig_file": "",
                            "namespaces": {
                                "names": [
                                    "arms-prom"
                                ],
                                "own_namespace": false
                            },
                            "role": "pod"
                        }
                    ],
                    "metrics_path": "/metrics",
                    "relabel_configs": [
                        {
                            "action": "keep",
                            "regex": "kube-state-metrics",
                            "replacement": "$1",
                            "separator": ";",
                            "source_labels": [
                                "__meta_kubernetes_pod_label_k8s_app"
                            ]
                        },
                        {
                            "action": "keep",
                            "regex": "8080",
                            "replacement": "$1",
                            "separator": ";",
                            "source_labels": [
                                "__meta_kubernetes_pod_container_port_number"
                            ]
                        },
                        {
                            "action": "replace",
                            "regex": "([^:]+)(?::\\d+)?;(\\d+)",
                            "replacement": "$1:$2",
                            "separator": ";",
                            "source_labels": [
                                "__address__",
                                "__meta_kubernetes_pod_container_port_number"
                            ],
                            "target_label": "__address__"
                        }
                    ],
                    "metric_relabel_configs": [
                        {
                            "action": "keep",
                            "regex": "kube-state-metrics",
                            "replacement": "$1",
                            "separator": ";",
                            "source_labels": [
                                "__meta_kubernetes_pod_label_k8s_app"
                            ]
                        },
                        {
                            "action": "keep",
                            "regex": "8080",
                            "replacement": "$1",
                            "separator": ";",
                            "source_labels": [
                                "__meta_kubernetes_pod_container_port_number"
                            ]
                        },
                        {
                            "action": "replace",
                            "regex": "([^:]+)(?::\\d+)?;(\\d+)",
                            "replacement": "$1:$2",
                            "separator": ";",
                            "source_labels": [
                                "__address__",
                                "__meta_kubernetes_pod_container_port_number"
                            ],
                            "target_label": "__address__"
                        }
                    ],
                    "scheme": "http",
                    "scrape_interval": "3s",
                    "scrape_timeout": "3s"
                }
            }
        )JSON";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        input = make_unique<InputPrometheus>();
        input->SetContext(ctx);
        input->SetMetricsRecordRef(InputPrometheus::sName, "1", "1", "1");

        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));

        APSARA_TEST_EQUAL(2U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorPromParseMetricNative::sName, input->mInnerProcessors[0]->Name());
        APSARA_TEST_EQUAL(ProcessorPromRelabelMetricNative::sName, input->mInnerProcessors[1]->Name());
        APSARA_TEST_EQUAL(ProcessorPromRelabelMetricNative::sName, input->mInnerProcessors[1]->mPlugin->Name());
        APSARA_TEST_EQUAL(3U,
                          dynamic_cast<ProcessorPromRelabelMetricNative*>(input->mInnerProcessors[1]->mPlugin.get())
                              ->mRelabelConfigs.size());
        APSARA_TEST_EQUAL(Action::KEEP,
                          dynamic_cast<ProcessorPromRelabelMetricNative*>(input->mInnerProcessors[1]->mPlugin.get())
                              ->mRelabelConfigs[0]
                              .mAction);
        APSARA_TEST_EQUAL(Action::KEEP,
                          dynamic_cast<ProcessorPromRelabelMetricNative*>(input->mInnerProcessors[1]->mPlugin.get())
                              ->mRelabelConfigs[1]
                              .mAction);
        APSARA_TEST_EQUAL(Action::REPLACE,
                          dynamic_cast<ProcessorPromRelabelMetricNative*>(input->mInnerProcessors[1]->mPlugin.get())
                              ->mRelabelConfigs[2]
                              .mAction);
    }
}

UNIT_TEST_CASE(InputPrometheusUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputPrometheusUnittest, OnFailedInit)
UNIT_TEST_CASE(InputPrometheusUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputPrometheusUnittest, TestCreateInnerProcessor)

} // namespace logtail

UNIT_TEST_MAIN