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

#include "common/JsonUtil.h"
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

protected:
    void SetUp() override {
        setenv("POD_NAME", "prometheus-test", 1);
        setenv("OPERATOR_HOST", "127.0.0.1", 1);
        setenv("OPERATOR_PORT", "12345", 1);
        {
            mConfigString = R"JSON(
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

        mHttpResponse.mStatusCode = 200;
        {
            mHttpResponse.mBody = R"JSON([
        {
            "targets": [
                "192.168.22.7:8080"
            ],
            "labels": {
                "__meta_kubernetes_pod_controller_kind": "ReplicaSet",
                "__meta_kubernetes_pod_container_image": "registry-vpc.cn-hangzhou.aliyuncs.com/acs/kube-state-metrics:v2.3.0-a71f78c-aliyun",
                "__meta_kubernetes_namespace": "arms-prom",
                "__meta_kubernetes_pod_labelpresent_pod_template_hash": "true",
                "__meta_kubernetes_pod_uid": "00d1897f-d442-47c4-8423-e9bf32dea173",
                "__meta_kubernetes_pod_container_init": "false",
                "__meta_kubernetes_pod_container_port_protocol": "TCP",
                "__meta_kubernetes_pod_host_ip": "192.168.21.234",
                "__meta_kubernetes_pod_controller_name": "kube-state-metrics-64cf88c8f4",
                "__meta_kubernetes_pod_annotation_k8s_aliyun_com_pod_ips": "192.168.22.7",
                "__meta_kubernetes_pod_ready": "true",
                "__meta_kubernetes_pod_node_name": "cn-hangzhou.192.168.21.234",
                "__meta_kubernetes_pod_annotationpresent_k8s_aliyun_com_pod_ips": "true",
                "__address__": "192.168.22.7:8080",
                "__meta_kubernetes_pod_labelpresent_k8s_app": "true",
                "__meta_kubernetes_pod_label_k8s_app": "kube-state-metrics",
                "__meta_kubernetes_pod_container_id": "containerd://57c4dfd8d9ea021defb248dfbc5cc3bd3758072c4529be351b8cc6838bdff02f",
                "__meta_kubernetes_pod_container_port_number": "8080",
                "__meta_kubernetes_pod_ip": "192.168.22.7",
                "__meta_kubernetes_pod_phase": "Running",
                "__meta_kubernetes_pod_container_name": "kube-state-metrics",
                "__meta_kubernetes_pod_container_port_name": "http-metrics",
                "__meta_kubernetes_pod_label_pod_template_hash": "64cf88c8f4",
                "__meta_kubernetes_pod_name": "kube-state-metrics-64cf88c8f4-jtn6v"
            }
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
        }
    ])JSON";
        }
    }
    void TearDown() override {
        unsetenv("POD_NAME");
        unsetenv("OPERATOR_HOST");
        unsetenv("OPERATOR_PORT");
    }

private:
    HttpResponse mHttpResponse;
    Json::Value mConfig;
    std::string mConfigString;
};


void TargetSubscriberSchedulerUnittest::OnInitScrapeJobEvent() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));

    APSARA_TEST_NOT_EQUAL(targetSubscriber->mScrapeConfigPtr.get(), nullptr);
    APSARA_TEST_EQUAL(targetSubscriber->mJobName, "_kube-state-metrics");
}



void TargetSubscriberSchedulerUnittest::TestProcess() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));

    // if status code is not 200
    mHttpResponse.mStatusCode = 404;
    targetSubscriber->OnSubscription(mHttpResponse);
    APSARA_TEST_EQUAL(0UL, targetSubscriber->mScrapeSchedulerMap.size());

    // if status code is 200
    mHttpResponse.mStatusCode = 200;
    targetSubscriber->OnSubscription(mHttpResponse);
    APSARA_TEST_EQUAL(2UL, targetSubscriber->mScrapeSchedulerMap.size());
}

void TargetSubscriberSchedulerUnittest::TestParseTargetGroups() {
    std::shared_ptr<TargetSubscriberScheduler> targetSubscriber = std::make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(targetSubscriber->Init(mConfig["ScrapeConfig"]));

    std::vector<Labels> newScrapeSchedulerSet;
    APSARA_TEST_TRUE(targetSubscriber->ParseScrapeSchedulerGroup(mHttpResponse.mBody, newScrapeSchedulerSet));
    APSARA_TEST_EQUAL(2UL, newScrapeSchedulerSet.size());
}

UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, OnInitScrapeJobEvent)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestProcess)
UNIT_TEST_CASE(TargetSubscriberSchedulerUnittest, TestParseTargetGroups)

} // namespace logtail

UNIT_TEST_MAIN