/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <json/json.h>

#include <iostream>
#include <memory>
#include <string>

#include "ScrapeScheduler.h"
#include "common/JsonUtil.h"
#include "prometheus/Constants.h"
#include "prometheus/labels/Labels.h"
#include "prometheus/schedulers/TargetSubscriberScheduler.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class TargetSubscriberSchedulerUnittest : public ::testing::Test {
public:
    void OnInitScrapeJobEvent();
    void TestProcess();
    void TestParseTargetGroups();
    void TestBuildScrapeSchedulerSet();
    void TestTargetLabels();
    void TestTargetsInfoToString();

protected:
    void SetUp() override {
        {
            mConfigString = R"JSON(
{
    "Type": "input_prometheus",
    "ScrapeConfig": {
        "enable_http2": true,
        "follow_redirects": true,
        "honor_timestamps": false,
        "job_name": "loong-collector/demo-podmonitor-500/0",
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
        "scheme": "http",
        "scrape_interval": "30s",
        "scrape_timeout": "30s"
    }
}
    )JSON";
        }
        std::string errMsg;
        if (!ParseJsonTable(mConfigString, mConfig, errMsg)) {
            std::cerr << "JSON parsing failed." << std::endl;
        }

        mHttpResponse.SetStatusCode(200);
        {
            *mHttpResponse.GetBody<string>() = R"JSON([
        {
            "targets": [
                "10.0.2.81:8080"
            ],
            "labels": {
                "__meta_kubernetes_pod_labelpresent_label_key_47": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_07": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_18": "true",
                "__meta_kubernetes_pod_annotationpresent_prometheus_io_port": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_08": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_15": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_32": "true",
                "__meta_kubernetes_pod_label_label_key_07": "label_value_07",
                "__meta_kubernetes_pod_labelpresent_label_key_33": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_26": "true",
                "__meta_kubernetes_pod_label_label_key_17": "label_value_17",
                "__meta_kubernetes_pod_label_label_key_38": "label_value_38",
                "__meta_kubernetes_pod_annotationpresent_prometheus_io_scrape": "true",
                "__meta_kubernetes_pod_container_init": "false",
                "__meta_kubernetes_pod_labelpresent_label_key_35": "true",
                "__meta_kubernetes_pod_label_label_key_13": "label_value_13",
                "__meta_kubernetes_pod_name": "demo-app-500-5c97455f77-brddj",
                "__meta_kubernetes_pod_label_label_key_32": "label_value_32",
                "__meta_kubernetes_pod_labelpresent_label_key_01": "true",
                "__meta_kubernetes_pod_label_label_key_12": "label_value_12",
                "__meta_kubernetes_pod_label_label_key_11": "label_value_11",
                "__meta_kubernetes_pod_label_label_key_04": "label_value_04",
                "__meta_kubernetes_pod_labelpresent_label_key_42": "true",
                "__meta_kubernetes_pod_uid": "c640e01c-0c1e-487e-9d1b-a743b88bb01a",
                "__meta_kubernetes_pod_labelpresent_label_key_10": "true",
                "__meta_kubernetes_pod_label_label_key_36": "label_value_36",
                "__meta_kubernetes_pod_label_label_key_43": "label_value_43",
                "__meta_kubernetes_pod_labelpresent_label_key_24": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_04": "true",
                "__meta_kubernetes_pod_label_label_key_01": "label_value_01",
                "__meta_kubernetes_pod_label_label_key_09": "label_value_09",
                "__meta_kubernetes_pod_label_label_key_00": "label_value_00",
                "__meta_kubernetes_pod_labelpresent_label_key_34": "true",
                "__meta_kubernetes_pod_labelpresent_pod_template_hash": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_41": "true",
                "__meta_kubernetes_pod_label_label_key_19": "label_value_19",
                "__meta_kubernetes_pod_label_label_key_10": "label_value_10",
                "__meta_kubernetes_pod_label_label_key_35": "label_value_35",
                "__meta_kubernetes_pod_labelpresent_app": "true",
                "__meta_kubernetes_pod_controller_kind": "ReplicaSet",
                "__meta_kubernetes_pod_label_label_key_49": "label_value_49",
                "__meta_kubernetes_pod_labelpresent_label_key_05": "true",
                "__meta_kubernetes_pod_label_label_key_25": "label_value_25",
                "__meta_kubernetes_pod_labelpresent_label_key_49": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_06": "true",
                "__meta_kubernetes_pod_label_label_key_23": "label_value_23",
                "__meta_kubernetes_pod_labelpresent_label_key_27": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_25": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_38": "true",
                "__meta_kubernetes_pod_label_label_key_02": "label_value_02",
                "__meta_kubernetes_pod_label_label_key_22": "label_value_22",
                "__meta_kubernetes_pod_annotation_prometheus_io_port": "8080",
                "__meta_kubernetes_pod_phase": "Running",
                "__meta_kubernetes_pod_label_label_key_27": "label_value_27",
                "__meta_kubernetes_pod_labelpresent_label_key_28": "true",
                "__meta_kubernetes_pod_label_label_key_41": "label_value_41",
                "__meta_kubernetes_pod_label_label_key_24": "label_value_24",
                "__meta_kubernetes_pod_label_label_key_20": "label_value_20",
                "__meta_kubernetes_pod_label_pod_template_hash": "5c97455f77",
                "__meta_kubernetes_pod_label_label_key_33": "label_value_33",
                "__meta_kubernetes_pod_labelpresent_label_key_40": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_03": "true",
                "__meta_kubernetes_pod_label_label_key_08": "label_value_08",
                "__meta_kubernetes_pod_container_port_number": "8080",
                "__meta_kubernetes_pod_labelpresent_label_key_31": "true",
                "__meta_kubernetes_pod_label_label_key_48": "label_value_48",
                "__meta_kubernetes_pod_label_label_key_40": "label_value_40",
                "__meta_kubernetes_pod_label_label_key_28": "label_value_28",
                "__meta_kubernetes_pod_labelpresent_label_key_23": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_48": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_43": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_46": "true",
                "__meta_kubernetes_pod_label_label_key_46": "label_value_46",
                "__meta_kubernetes_pod_controller_name": "demo-app-500-5c97455f77",
                "__meta_kubernetes_pod_host_ip": "10.0.4.54",
                "__meta_kubernetes_pod_labelpresent_label_key_44": "true",
                "__meta_kubernetes_pod_label_label_key_44": "label_value_44",
                "__meta_kubernetes_pod_node_name": "cn-heyuan.10.0.4.54",
                "__address__": "10.0.2.81:8080",
                "__meta_kubernetes_pod_container_name": "demo-app-500",
                "__meta_kubernetes_pod_labelpresent_label_key_30": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_17": "true",
                "__meta_kubernetes_pod_label_label_key_29": "label_value_29",
                "__meta_kubernetes_pod_labelpresent_label_key_02": "true",
                "__meta_kubernetes_pod_label_label_key_39": "label_value_39",
                "__meta_kubernetes_pod_label_label_key_21": "label_value_21",
                "__meta_kubernetes_pod_label_label_key_37": "label_value_37",
                "__meta_kubernetes_pod_labelpresent_label_key_37": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_45": "true",
                "__meta_kubernetes_pod_label_label_key_45": "label_value_45",
                "__meta_kubernetes_pod_label_label_key_26": "label_value_26",
                "__meta_kubernetes_pod_labelpresent_label_key_12": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_11": "true",
                "__meta_kubernetes_pod_label_app": "demo-app-500",
                "__meta_kubernetes_pod_labelpresent_label_key_21": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_13": "true",
                "__meta_kubernetes_pod_ip": "10.0.2.81",
                "__meta_kubernetes_pod_label_label_key_42": "label_value_42",
                "__meta_kubernetes_pod_annotation_k8s_aliyun_com_pod_ips": "10.0.2.81",
                "__meta_kubernetes_pod_label_label_key_34": "label_value_34",
                "__meta_kubernetes_pod_label_label_key_06": "label_value_06",
                "__meta_kubernetes_pod_label_label_key_31": "label_value_31",
                "__meta_kubernetes_pod_annotation_prometheus_io_path": "/metrics",
                "__meta_kubernetes_pod_container_id": "containerd://788da13840e1e8711f71b42015cdaabf590b10e8658524a9e3de910a4f373532",
                "__meta_kubernetes_pod_ready": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_00": "true",
                "__meta_kubernetes_pod_label_label_key_03": "label_value_03",
                "__meta_kubernetes_pod_labelpresent_label_key_16": "true",
                "__meta_kubernetes_pod_container_port_name": "",
                "__meta_kubernetes_pod_label_label_key_16": "label_value_16",
                "__meta_kubernetes_pod_label_label_key_15": "label_value_15",
                "__meta_kubernetes_pod_container_image": "arms-deploy-registry.cn-hangzhou.cr.aliyuncs.com/arms-deploy-repo/prometheus-sample-app:latest",
                "__meta_kubernetes_pod_annotation_prometheus_io_scrape": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_20": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_19": "true",
                "__meta_kubernetes_pod_label_label_key_18": "label_value_18",
                "__meta_kubernetes_pod_annotationpresent_prometheus_io_path": "true",
                "__meta_kubernetes_pod_annotationpresent_k8s_aliyun_com_pod_ips": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_39": "true",
                "__meta_kubernetes_pod_label_label_key_30": "label_value_30",
                "__meta_kubernetes_pod_labelpresent_label_key_14": "true",
                "__meta_kubernetes_pod_label_label_key_05": "label_value_05",
                "__meta_kubernetes_pod_labelpresent_label_key_22": "true",
                "__meta_kubernetes_pod_container_port_protocol": "TCP",
                "__meta_kubernetes_pod_labelpresent_label_key_09": "true",
                "__meta_kubernetes_pod_labelpresent_label_key_36": "true",
                "__meta_kubernetes_pod_label_label_key_47": "label_value_47",
                "__meta_kubernetes_pod_label_label_key_14": "label_value_14",
                "__meta_kubernetes_namespace": "default",
                "__meta_kubernetes_pod_labelpresent_label_key_29": "true"
            },
            "Load": 425
        },
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__meta_kubernetes_endpoint_port_protocol": "TCP",
                "__meta_kubernetes_service_label_provider": "kubernetes",
                "__meta_kubernetes_endpoints_name": "kubernetes",
                "__meta_kubernetes_service_name": "kubernetes",
                "__meta_kubernetes_endpoints_labelpresent_endpointslice_kubernetes_io_skip_mirror": "true",
                "__meta_kubernetes_service_labelpresent_provider": "true",
                "__meta_kubernetes_endpoint_port_name": "https",
                "__meta_kubernetes_namespace": "default",
                "__meta_kubernetes_service_label_component": "apiserver",
                "__meta_kubernetes_service_labelpresent_component": "true",
                "__meta_kubernetes_endpoint_ready": "true"
            }
        },
        {
            "targets": [
                "192.168.22.33:6443"
            ],
            "labels": {
                "__address__": "192.168.22.33:6443",
                "__meta_kubernetes_endpoint_port_protocol": "TCP",
                "__meta_kubernetes_service_label_provider": "kubernetes",
                "__meta_kubernetes_endpoints_name": "kubernetes",
                "__meta_kubernetes_service_name": "kubernetes",
                "__meta_kubernetes_endpoints_labelpresent_endpointslice_kubernetes_io_skip_mirror": "true",
                "__meta_kubernetes_service_labelpresent_provider": "true",
                "__meta_kubernetes_endpoint_port_name": "https",
                "__meta_kubernetes_namespace": "default",
                "__meta_kubernetes_service_label_component": "apiserver",
                "__meta_kubernetes_service_labelpresent_component": "true",
                "__meta_kubernetes_endpoint_ready": "true"
            }
        }
    ])JSON";
        }
    }
    void TearDown() override {}

private:
    HttpResponse mHttpResponse;
    Json::Value mConfig;
    std::string mConfigString;
};


void TargetSubscriberSchedulerUnittest::OnInitScrapeJobEvent() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));

    APSARA_TEST_NOT_EQUAL(targetSubscriber->mScrapeConfigPtr.get(), nullptr);
    APSARA_TEST_EQUAL(targetSubscriber->mJobName, "loong-collector/demo-podmonitor-500/0");
}

void TargetSubscriberSchedulerUnittest::TestProcess() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    auto metricLabels = MetricLabels();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));
    targetSubscriber->InitSelfMonitor(metricLabels);

    // if status code is not 200
    mHttpResponse.SetStatusCode(404);
    targetSubscriber->OnSubscription(mHttpResponse, 0);
    APSARA_TEST_EQUAL(0UL, targetSubscriber->mScrapeSchedulerMap.size());

    // if status code is 200
    mHttpResponse.SetStatusCode(200);
    targetSubscriber->OnSubscription(mHttpResponse, 0);
    APSARA_TEST_EQUAL(3UL, targetSubscriber->mScrapeSchedulerMap.size());
}

void TargetSubscriberSchedulerUnittest::TestParseTargetGroups() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));

    std::vector<PromTargetInfo> newScrapeSchedulerSet;
    APSARA_TEST_TRUE(
        targetSubscriber->ParseScrapeSchedulerGroup(*mHttpResponse.GetBody<string>(), newScrapeSchedulerSet));
    APSARA_TEST_EQUAL(3UL, newScrapeSchedulerSet.size());
}

void TargetSubscriberSchedulerUnittest::TestBuildScrapeSchedulerSet() {
    // prepare data
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));
    std::vector<PromTargetInfo> newScrapeSchedulerSet;
    APSARA_TEST_TRUE(
        targetSubscriber->ParseScrapeSchedulerGroup(*mHttpResponse.GetBody<string>(), newScrapeSchedulerSet));
    APSARA_TEST_EQUAL(3UL, newScrapeSchedulerSet.size());

    auto result = targetSubscriber->BuildScrapeSchedulerSet(newScrapeSchedulerSet);

    vector<pair<string, chrono::steady_clock::time_point>> startTimeList;
    startTimeList.reserve(result.size());
    for (auto& it : result) {
        startTimeList.emplace_back(it.second->GetId(), it.second->GetNextExecTime());
    }
    APSARA_TEST_EQUAL(3UL, startTimeList.size());
    APSARA_TEST_NOT_EQUAL(startTimeList[0].second, startTimeList[1].second);
    APSARA_TEST_NOT_EQUAL(startTimeList[1].second, startTimeList[2].second);
    APSARA_TEST_NOT_EQUAL(startTimeList[0].second, startTimeList[2].second);

    APSARA_TEST_EQUAL(1UL, result.count("loong-collector/demo-podmonitor-500/010.0.2.81:808093796c8e4493906d"));
}

void TargetSubscriberSchedulerUnittest::TestTargetLabels() {
    // prepare data
    auto judgeFunc = [](const Json::Value& scrapeConfig,
                        const string& targetResponse,
                        const string& metricsPath,
                        const string& scheme,
                        int64_t scrapeIntervalSeconds,
                        uint64_t scrapeTimeoutSeconds,
                        const string& ip,
                        int32_t port) {
        std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
        APSARA_TEST_TRUE(targetSubscriber->Init(scrapeConfig));
        std::vector<PromTargetInfo> newScrapeSchedulerSet;
        APSARA_TEST_TRUE(targetSubscriber->ParseScrapeSchedulerGroup(targetResponse, newScrapeSchedulerSet));
        APSARA_TEST_EQUAL(1UL, newScrapeSchedulerSet.size());

        auto result = targetSubscriber->BuildScrapeSchedulerSet(newScrapeSchedulerSet);
        APSARA_TEST_EQUAL(1UL, result.size());
        APSARA_TEST_EQUAL(result.begin()->second->mMetricsPath, metricsPath);
        APSARA_TEST_EQUAL(result.begin()->second->mInterval, scrapeIntervalSeconds);
        APSARA_TEST_EQUAL(result.begin()->second->mScrapeTimeoutSeconds, scrapeTimeoutSeconds);
        APSARA_TEST_EQUAL(result.begin()->second->mScheme, scheme);
        APSARA_TEST_EQUAL(result.begin()->second->mHost, ip);
        APSARA_TEST_EQUAL(result.begin()->second->mPort, port);
    };

    string case1 = R"JSON([
        {
            "targets": [
                "192.168.22.7:8080"
            ],
            "labels": {
                "__address__": "192.168.22.7:8080",
                "__scheme__": "https"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case1, "/metrics", "https", 30, 30, "192.168.22.7", 8080);
    string case2 = R"JSON([
        {
            "targets": [
                "192.168.22.7:8080"
            ],
            "labels": {
                "__address__": "192.168.22.7:8080",
                "__scheme__": "http",
                "__param_xx": "yy",
                "__param_yy": "zz"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case2, "/metrics?xx=yy&yy=zz", "http", 30, 30, "192.168.22.7", 8080);
    string case3 = R"JSON([
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__scheme__": "http",
                "__metrics_path__": "/metrics/ab/c?d=ef&aa=bb"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case3, "/metrics/ab/c?d=ef&aa=bb", "http", 30, 30, "192.168.22.31", 6443);
    string case4 = R"JSON([
        {
            "targets": [
                "192.168.22.7:8080"
            ],
            "labels": {
                "__address__": "192.168.22.7:8080",
                "__scheme__": "https",
                "__metrics_path__": "/custom/metrics",
                "__param_xx": "yy",
                "__param_yy": "zz"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case4, "/custom/metrics?xx=yy&yy=zz", "https", 30, 30, "192.168.22.7", 8080);
    string case5 = R"JSON([
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__scheme__": "http",
                "__metrics_path__": "/metrics/ab/c?d=ef&aa=bb",
                "__param_yy": "zz"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case5, "/metrics/ab/c?d=ef&aa=bb&yy=zz", "http", 30, 30, "192.168.22.31", 6443);
    string case6 = R"JSON([
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__scheme__": "http",
                "__metrics_path__": "/metrics/ab/c?d=ef&aa=bb",
                "__param_xx": "yy",
                "__param_yy": "zz"
            }
        }
    ])JSON";
    judgeFunc(
        mConfig["ScrapeConfig"], case6, "/metrics/ab/c?d=ef&aa=bb&xx=yy&yy=zz", "http", 30, 30, "192.168.22.31", 6443);
    string case7 = R"JSON([
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__scheme__": "http",
                "__scrape_interval__": "5s",
                "__scrape_timeout__": "5s"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"], case7, "/metrics", "http", 5, 5, "192.168.22.31", 6443);
    string configStr8 = R"JSON(
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
                "params" : {
                    "__param_query": [
                        "test_query"
                    ],
                    "__param_query_1": [
                        "test_query_1"
                    ]
                },
                "metrics_path": "/metrics",
                "scheme": "https",
                "scrape_interval": "30s",
                "scrape_timeout": "30s"
            }
        }
            )JSON";

    std::string errMsg;
    if (!ParseJsonTable(configStr8, mConfig, errMsg)) {
        std::cerr << "JSON parsing failed." << std::endl;
    }
    string case8 = R"JSON([
        {
            "targets": [
                "192.168.22.31:6443"
            ],
            "labels": {
                "__address__": "192.168.22.31:6443",
                "__scrape_interval__": "5s",
                "__scrape_timeout__": "5s"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"],
              case8,
              "/metrics?__param_query=test_query&__param_query_1=test_query_1",
              "https",
              5,
              5,
              "192.168.22.31",
              6443);
    string configStr9 = R"JSON(
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
                "params" : {
                    "__param_query": [
                        "test_query"
                    ],
                    "__param_query_1": [
                        "test_query_1"
                    ]
                },
                "metrics_path": "/metrics",
                "scheme": "https",
                "scrape_interval": "30s",
                "scrape_timeout": "30s"
            }
        }
            )JSON";

    if (!ParseJsonTable(configStr9, mConfig, errMsg)) {
        std::cerr << "JSON parsing failed." << std::endl;
    }
    string case9 = R"JSON([
        {
            "targets": [
                "192.168.22.31"
            ],
            "labels": {
                "__address__": "192.168.22.31",
                "__scheme__": "http",
                "__param_xx": "yy",
                "__scrape_interval__": "5s",
                "__scrape_timeout__": "5s"
            }
        }
    ])JSON";
    judgeFunc(mConfig["ScrapeConfig"],
              case9,
              "/metrics?__param_query=test_query&__param_query_1=test_query_1&xx=yy",
              "http",
              5,
              5,
              "192.168.22.31",
              80);
}

void TargetSubscriberSchedulerUnittest::TestTargetsInfoToString() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    auto metricLabels = MetricLabels();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));
    targetSubscriber->InitSelfMonitor(metricLabels);
    // if status code is 200
    mHttpResponse.SetStatusCode(200);
    targetSubscriber->OnSubscription(mHttpResponse, 0);
    APSARA_TEST_EQUAL(3UL, targetSubscriber->mScrapeSchedulerMap.size());
    auto res = targetSubscriber->TargetsInfoToString();
    string errorMsg;
    Json::Value data;
    ParseJsonTable(res, data, errorMsg);
    APSARA_TEST_EQUAL(2.0, data[prometheus::AGENT_INFO][prometheus::CPU_LIMIT].asFloat());
    APSARA_TEST_EQUAL((uint64_t)3, data[prometheus::TARGETS_INFO].size());
}

UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, OnInitScrapeJobEvent)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestProcess)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestParseTargetGroups)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestBuildScrapeSchedulerSet)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestTargetLabels)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestTargetsInfoToString)

} // namespace logtail

UNIT_TEST_MAIN
