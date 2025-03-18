// Copyright 2022 iLogtail Authors
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

#include <memory>
#include <string>
#include <vector>

#include "metadata/K8sMetadata.h"
#include "metadata/LabelingK8sMetadata.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class k8sMetadataUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        // You can set up common objects needed for each test case here
    }

    void TearDown() override {
        // Clean up after each test case if needed
    }

public:
    void TestAsyncQueryMetadata() {
        // AsyncQueryMetadata, will add to pending queue and batch keys
        // mock server handle ...
        // verify
    }

    void TestExternalIpOperations() {
        const std::string jsonData = R"({
            "10.41.0.2": {
                "namespace": "kube-system",
                "workloadName": "coredns-7b669cbb96",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "k8s-app": "kube-dns",
                    "pod-template-hash": "7b669cbb96"
                },
                "envs": {
                    "COREDNS_NAMESPACE": "",
                    "COREDNS_POD_NAME": ""
                },
                "images": {
                    "coredns": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/coredns:v1.9.3.10-7dfca203-aliyun"
                }
            },
            "10.41.0.3": {
                "namespace": "kube-system",
                "workloadName": "csi-provisioner-8bd988c55",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "app": "csi-provisioner",
                    "pod-template-hash": "8bd988c55"
                },
                "envs": {
                    "CLUSTER_ID": "c33235919ddad4f279b3a67c2f0046704",
                    "ENABLE_NAS_SUBPATH_FINALIZER": "true",
                    "KUBE_NODE_NAME": "",
                    "SERVICE_TYPE": "provisioner"
                },
                "images": {
                    "csi-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-plugin:v1.30.3-921e63a-aliyun",
                    "external-csi-snapshotter": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-snapshotter:v4.0.0-a230d5b-aliyun",
                    "external-disk-attacher": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-attacher:v4.5.0-4a01fda6-aliyun",
                    "external-disk-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-disk-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-nas-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-nas-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-oss-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-snapshot-controller": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/snapshot-controller:v4.0.0-a230d5b-aliyun"
                }
            },
            "172.16.20.108": {
                "namespace": "kube-system",
                "workloadName": "kube-proxy-worker",
                "workloadKind": "daemonset",
                "serviceName": "",
                "labels": {
                    "controller-revision-hash": "756748b889",
                    "k8s-app": "kube-proxy-worker",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "NODE_NAME": ""
                },
                "images": {
                    "kube-proxy-worker": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/kube-proxy:v1.30.1-aliyun.1"
                }
            }
        })";

        K8sMetadata::GetInstance().UpdateExternalIpCache({"10.41.0.2", "10.41.0.3", "172.16.20.108"},
                                                         {"10.41.0.2", "10.41.0.3"});
        APSARA_TEST_TRUE(K8sMetadata::GetInstance().IsExternalIp("172.16.20.108"));
        APSARA_TEST_FALSE(K8sMetadata::GetInstance().IsExternalIp("10.41.0.2"));
        APSARA_TEST_FALSE(K8sMetadata::GetInstance().IsExternalIp("10.41.0.3"));
    }

    void TestGetByContainerIds() {
        const std::string jsonData
            = R"({"286effd2650c0689b779018e42e9ec7aa3d2cb843005e038204e85fc3d4f9144":{"namespace":"default","workloadName":"oneagent-demo-658648895b","workloadKind":"replicaset","serviceName":"","labels":{"app":"oneagent-demo","pod-template-hash":"658648895b"},"envs":{},"images":{"oneagent-demo":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/centos7-cve-fix:1.0.0"}}})";

        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;
        auto res = reader->parse(jsonData.c_str(), jsonData.c_str() + jsonData.size(), &root, &errors);
        APSARA_TEST_TRUE(res);
        std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, containerInfoType::ContainerIdInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(containerInfoType::ContainerIdInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(
            "286effd2650c0689b779018e42e9ec7aa3d2cb843005e038204e85fc3d4f9144");
        APSARA_TEST_TRUE(container != nullptr);
        APSARA_TEST_EQUAL(container->k8sNamespace, "default");
        APSARA_TEST_EQUAL(container->workloadName, "oneagent-demo-658648895b");
        APSARA_TEST_EQUAL(container->workloadKind, "replicaset");
        APSARA_TEST_EQUAL(container->appId, "");
        APSARA_TEST_EQUAL(container->appName, "");
    }

    void TestGetByIps() {
        const std::string jsonData
            = R"({"192.16..10.1":{"namespace":"default","workloadName":"oneagent-demo-658648895b","workloadKind":"replicaset","serviceName":"","labels":{"app":"oneagent-demo","pod-template-hash":"658648895b"},"envs":{},"images":{"oneagent-demo":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/centos7-cve-fix:1.0.0"}}})";

        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;
        auto res = reader->parse(jsonData.c_str(), jsonData.c_str() + jsonData.size(), &root, &errors);
        APSARA_TEST_TRUE(res);
        std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, containerInfoType::IpInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(containerInfoType::IpInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache("192.16..10.1");
        APSARA_TEST_TRUE(container != nullptr);
        APSARA_TEST_EQUAL(container->k8sNamespace, "default");
        APSARA_TEST_EQUAL(container->workloadName, "oneagent-demo-658648895b");
        APSARA_TEST_EQUAL(container->workloadKind, "replicaset");
        APSARA_TEST_EQUAL(container->appId, "");
        APSARA_TEST_EQUAL(container->appName, "");
    }

    void TestGetByLocalHost() {
        LOG_INFO(sLogger, ("TestGetByLocalHost() begin", time(NULL)));
        // Sample JSON data
        const std::string jsonData = R"({
            "10.41.0.2": {
                "namespace": "kube-system",
                "workloadName": "coredns-7b669cbb96",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "k8s-app": "kube-dns",
                    "pod-template-hash": "7b669cbb96"
                },
                "envs": {
                    "COREDNS_NAMESPACE": "",
                    "COREDNS_POD_NAME": ""
                },
                "images": {
                    "coredns": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/coredns:v1.9.3.10-7dfca203-aliyun"
                }
            },
            "10.41.0.3": {
                "namespace": "kube-system",
                "workloadName": "csi-provisioner-8bd988c55",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "app": "csi-provisioner",
                    "pod-template-hash": "8bd988c55"
                },
                "envs": {
                    "CLUSTER_ID": "c33235919ddad4f279b3a67c2f0046704",
                    "ENABLE_NAS_SUBPATH_FINALIZER": "true",
                    "KUBE_NODE_NAME": "",
                    "SERVICE_TYPE": "provisioner"
                },
                "images": {
                    "csi-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-plugin:v1.30.3-921e63a-aliyun",
                    "external-csi-snapshotter": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-snapshotter:v4.0.0-a230d5b-aliyun",
                    "external-disk-attacher": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-attacher:v4.5.0-4a01fda6-aliyun",
                    "external-disk-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-disk-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-nas-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-nas-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-oss-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-snapshot-controller": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/snapshot-controller:v4.0.0-a230d5b-aliyun"
                }
            },
            "172.16.20.108": {
                "namespace": "kube-system",
                "workloadName": "kube-proxy-worker",
                "workloadKind": "daemonset",
                "serviceName": "",
                "labels": {
                    "controller-revision-hash": "756748b889",
                    "k8s-app": "kube-proxy-worker",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "NODE_NAME": ""
                },
                "images": {
                    "kube-proxy-worker": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/kube-proxy:v1.30.1-aliyun.1"
                }
            }
        })";

        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;
        auto res = reader->parse(jsonData.c_str(), jsonData.c_str() + jsonData.size(), &root, &errors);
        APSARA_TEST_TRUE(res);
        std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, containerInfoType::IpInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(containerInfoType::IpInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByIpFromCache("172.16.20.108");
        APSARA_TEST_TRUE(container != nullptr);

        auto& k8sMetadata = K8sMetadata::GetInstance();
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string eventStr = R"({
        "events" :
        [
{
        "name": "test",
        "tags": {
            "remote_ip": "172.16.20.108"
        },
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "type" : 2,
        "value": {
            "type": "untyped_single_value",
            "detail": 10.0
        }
    }
        ],
        "metadata" :
        {
            "log.file.path_resolved" : "/var/log/message"
        },
        "tags" :
        {
            "app_name" : "xxx"
        }
    })";
        eventGroup.FromJsonString(eventStr);
        eventGroup.AddMetricEvent();
        LabelingK8sMetadata processor;
        processor.AddLabelToLogGroup(eventGroup);
        EventsContainer& eventsEnd = eventGroup.MutableEvents();
        auto& metricEvent = eventsEnd[0].Cast<MetricEvent>();
        APSARA_TEST_EQUAL("kube-proxy-worker", metricEvent.GetTag("peerWorkloadName").to_string());
        APSARA_TEST_TRUE_FATAL(k8sMetadata.GetInfoByIpFromCache("10.41.0.2") != nullptr);
    }

    void TestAddLabelToSpan() {
        LOG_INFO(sLogger, ("TestProcessEventForSpan() begin", time(NULL)));
        // Sample JSON data
        const std::string jsonData = R"({
            "10.41.0.2": {
                "namespace": "kube-system",
                "workloadName": "coredns-7b669cbb96",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "k8s-app": "kube-dns",
                    "pod-template-hash": "7b669cbb96"
                },
                "envs": {
                    "COREDNS_NAMESPACE": "",
                    "COREDNS_POD_NAME": ""
                },
                "images": {
                    "coredns": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/coredns:v1.9.3.10-7dfca203-aliyun"
                }
            },
            "10.41.0.3": {
                "namespace": "kube-system",
                "workloadName": "csi-provisioner-8bd988c55",
                "workloadKind": "replicaset",
                "serviceName": "",
                "labels": {
                    "app": "csi-provisioner",
                    "pod-template-hash": "8bd988c55"
                },
                "envs": {
                    "CLUSTER_ID": "c33235919ddad4f279b3a67c2f0046704",
                    "ENABLE_NAS_SUBPATH_FINALIZER": "true",
                    "KUBE_NODE_NAME": "",
                    "SERVICE_TYPE": "provisioner"
                },
                "images": {
                    "csi-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-plugin:v1.30.3-921e63a-aliyun",
                    "external-csi-snapshotter": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-snapshotter:v4.0.0-a230d5b-aliyun",
                    "external-disk-attacher": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-attacher:v4.5.0-4a01fda6-aliyun",
                    "external-disk-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-disk-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-nas-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-nas-resizer": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-resizer:v1.3-e48d981-aliyun",
                    "external-oss-provisioner": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/csi-provisioner:v3.5.0-e7da67e52-aliyun",
                    "external-snapshot-controller": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/snapshot-controller:v4.0.0-a230d5b-aliyun"
                }
            },
            "172.16.20.108": {
                "namespace": "kube-system",
                "workloadName": "kube-proxy-worker",
                "workloadKind": "daemonset",
                "serviceName": "",
                "labels": {
                    "controller-revision-hash": "756748b889",
                    "k8s-app": "kube-proxy-worker",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "NODE_NAME": ""
                },
                "images": {
                    "kube-proxy-worker": "registry-cn-chengdu-vpc.ack.aliyuncs.com/acs/kube-proxy:v1.30.1-aliyun.1"
                }
            }
        })";

        auto& k8sMetadata = K8sMetadata::GetInstance();
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;
        auto res = reader->parse(jsonData.c_str(), jsonData.c_str() + jsonData.size(), &root, &errors);
        APSARA_TEST_TRUE(res);
        std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, containerInfoType::IpInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(containerInfoType::IpInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByIpFromCache("172.16.20.108");
        APSARA_TEST_TRUE(container != nullptr);

        unique_ptr<SpanEvent> mSpanEvent;
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        mSpanEvent = eventGroup.CreateSpanEvent();
        mSpanEvent->SetTimestamp(12345678901, 0);
        mSpanEvent->SetTraceId("test_trace_id");
        mSpanEvent->SetSpanId("test_span_id");
        mSpanEvent->SetTraceState("normal");
        mSpanEvent->SetParentSpanId("test_parent_span_id");
        mSpanEvent->SetName("test_name");
        mSpanEvent->SetKind(SpanEvent::Kind::Client);
        mSpanEvent->SetStartTimeNs(1715826723000000000);
        mSpanEvent->SetEndTimeNs(1715826725000000000);
        mSpanEvent->SetTag(string("key1"), string("value1"));
        mSpanEvent->SetTag(string("remote_ip"), string("172.16.20.108"));
        SpanEvent::InnerEvent* e = mSpanEvent->AddEvent();
        e->SetName("test_event");
        e->SetTimestampNs(1715826724000000000);
        SpanEvent::SpanLink* l = mSpanEvent->AddLink();
        l->SetTraceId("other_trace_id");
        l->SetSpanId("other_span_id");
        std::vector<std::string> container_vec;
        std::vector<std::string> remote_ip_vec;
        mSpanEvent->SetStatus(SpanEvent::StatusCode::Ok);
        mSpanEvent->SetScopeTag(string("key2"), string("value2"));
        LabelingK8sMetadata* processor = new LabelingK8sMetadata;
        processor->AddLabels(*mSpanEvent, container_vec, remote_ip_vec);
        APSARA_TEST_EQUAL("kube-proxy-worker", mSpanEvent->GetTag("peerWorkloadName").to_string());
        APSARA_TEST_TRUE_FATAL(k8sMetadata.GetInfoByIpFromCache("10.41.0.2") != nullptr);
        delete processor;
    }


    void TestAddLabelToMetric() {
        LOG_INFO(sLogger, ("TestGetByLocalHost() begin", time(NULL)));
        // Sample JSON data
        const std::string jsonData = R"({
            "07a0fd4b-0557-4f77-80f8-19d25b288b64": {
                "podName": "kube-proxy-worker-7gcb8",
                "startTime": 1736409295,
                "namespace": "kube-system",
                "workloadName": "kube-proxy-worker",
                "workloadKind": "daemonset",
                "labels": {
                    "controller-revision-hash": "6bffcf8957",
                    "k8s-app": "kube-proxy-worker",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "NODE_NAME": ""
                },
                "images": {
                    "kube-proxy-worker": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/kube-proxy:v1.32.0-aliyun.1"
                },
                "containerIDs": [
                    "45e54922d52338629edeec2273dd212c850ac82c943b0fc4553f9dfcae8195be"
                ],
                "podIP": "10.208.196.86"
            },
            "121f2f20-6f4d-42c0-ae93-0aae1e8c9fa8": {
                "podName": "loongcollector-ds-885zr",
                "startTime": 1740558656,
                "namespace": "loongcollector",
                "workloadName": "loongcollector-ds",
                "workloadKind": "daemonset",
                "labels": {
                    "controller-revision-hash": "7966868989",
                    "k8s-app": "loongcollector-ds",
                    "kubernetes.io/cluster-service": "true",
                    "pod-template-generation": "16"
                },
                "envs": {
                    "ALICLOUD_LOG_DEFAULT_MACHINE_GROUP": "k8s-group-c8486c0bcc0984900964c94c8d1e8cfea",
                    "ALICLOUD_LOG_DEFAULT_PROJECT": "k8s-log-c8486c0bcc0984900964c94c8d1e8cfea",
                    "ALICLOUD_LOG_DOCKER_ENV_CONFIG": "true",
                    "ALICLOUD_LOG_ECS_FLAG": "true",
                    "ALICLOUD_LOG_ENDPOINT": "cn-heyuan-intranet.log.aliyuncs.com",
                    "ALIYUN_LOGTAIL_USER_DEFINED_ID": "$(ALICLOUD_LOG_DEFAULT_MACHINE_GROUP),",
                    "ALIYUN_LOGTAIL_USER_ID": "1108555361245511",
                    "ALIYUN_LOG_ENV_TAGS": "_node_name_|_node_ip_|_cluster_id_",
                    "HTTP_PROBE_PORT": "7953",
                    "LD_LIBRARY_PATH": "/usr/local/loongcollector",
                    "_cluster_id_": "c8486c0bcc0984900964c94c8d1e8cfea",
                    "_node_ip_": "",
                    "_node_name_": "",
                    "check_point_dump_interval": "60",
                    "cpu_usage_limit": "2",
                    "enable_full_drain_mode": "false",
                    "max_bytes_per_sec": "2.097152e+08",
                    "mem_usage_limit": "2048",
                    "process_thread_count": "4",
                    "send_request_concurrency": "80",
                    "working_hostname": "",
                    "working_ip": ""
                },
                "images": {
                    "logtail": "registry.cn-beijing.aliyuncs.com/arms-docker-repo/cmonitor-agent:ql-test-lc-withserver-119"
                },
                "containerIDs": [
                    "9d4ac769845dded46ba697e44cf1a5dc80142aa20cde31c65953d9b70192e0d0"
                ],
                "podIP": "10.208.196.86"
            },
            "12518e84-3ad8-4323-a4d4-b739c6303911": {
                "podName": "ack-node-problem-detector-daemonset-4wxp4",
                "startTime": 1736409295,
                "namespace": "kube-system",
                "workloadName": "ack-node-problem-detector-daemonset",
                "workloadKind": "daemonset",
                "labels": {
                    "app": "ack-node-problem-detector",
                    "controller-revision-hash": "6577577645",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "NODE_NAME": "",
                    "NVIDIA_VISIBLE_DEVICES": "all",
                    "SYSTEMD_OFFLINE": "0"
                },
                "images": {
                    "ack-node-problem-detector": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/ack-node-problem-detector:v0.8.14-3c6002c-aliyun"
                },
                "containerIDs": [
                    "3d840f6d4acf6caa4784c4c9de6f0ad059ca092eccc7601a0c08c7f9c68c1617"
                ],
                "podIP": "10.208.196.86"
            },
            "1c66fe56-8ed2-4df1-ad75-95db231a7366": {
                "podName": "insights-mysql-connection-757bccc9dc-g4kpj",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "insights-mysql-connection",
                "workloadKind": "deployment",
                "labels": {
                    "app": "insights-mysql-connection",
                    "armsPilotAutoEnable": "on",
                    "armsPilotCreateAppName": "insights-mysql-connection",
                    "pod-template-hash": "757bccc9dc"
                },
                "envs": {
                    "MOCK_SERVER_ID": "0",
                    "SERVICE_NAME": "insights-mysql-connection",
                    "spring.datasource.password": "root@1234",
                    "spring.datasource.url": "jdbc:mysql://mysql-pod:3306/arms_mock?characterEncoding=utf-8&useSSL=false",
                    "spring.datasource.username": "root",
                    "spring.redis.host": "redis",
                    "spring.redis.password": ""
                },
                "images": {
                    "insights-mysql-connection": "registry.cn-hangzhou.aliyuncs.com/private-mesh/hellob:mockserver"
                },
                "containerIDs": [
                    "1a586c38da7c58014ae6184614df44f7523bff60f28fce4f98af8afecf41843e"
                ],
                "podIP": "10.208.196.126"
            },
            "1c954719-3839-46dd-808a-a476095dd897": {
                "podName": "terway-eniip-djwm6",
                "startTime": 1736409295,
                "namespace": "kube-system",
                "workloadName": "terway-eniip",
                "workloadKind": "daemonset",
                "labels": {
                    "app": "terway-eniip",
                    "controller-revision-hash": "9598c4d47",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "CILIUM_CNI_CHAINING_MODE": "terway-chainer",
                    "CILIUM_K8S_NAMESPACE": "",
                    "DISABLE_POLICY": "",
                    "FELIX_TYPHAK8SSERVICENAME": "",
                    "IN_CLUSTER_LOADBALANCE": "",
                    "K8S_NODE_NAME": "",
                    "NODENAME": "",
                    "NODE_NAME": "",
                    "POD_NAMESPACE": "",
                    "TERWAY_GC_RULES": "true"
                },
                "images": {
                    "policy": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/terway:v1.13.2-1718bc8",
                    "terway": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/terway:v1.13.2-1718bc8"
                },
                "containerIDs": [
                    "219d778db68fd2330e51ad39ba08da1674312af84580d0a8cb697f902f2e8372",
                    "9ed35f628c855d60cac0221d09c2ea1cf3257c90fcc8017a2e995ae72313baf8"
                ],
                "podIP": "10.208.196.86"
            },
            "27049128-ba8c-4fa4-9c27-4b293a115bf9": {
                "podName": "redis-8b8444bc8-lkg52",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "redis",
                "workloadKind": "deployment",
                "labels": {
                    "app": "redis",
                    "pod-template-hash": "8b8444bc8"
                },
                "envs": {

                },
                "images": {
                    "redis": "docker.m.daocloud.io/redis"
                },
                "containerIDs": [
                    "715b65784acbbde94804b0131907d50e772a9f09d30719978de413519ce5bbf8"
                ],
                "podIP": "10.208.196.119"
            },
            "45ed10bc-5b17-4fc1-bc36-c03671a166c3": {
                "podName": "insights-server-1-57cfdbc6cc-2wbtp",
                "startTime": 1740479458,
                "namespace": "insights",
                "workloadName": "insights-server-1",
                "workloadKind": "deployment",
                "labels": {
                    "app": "insights-server-1",
                    "armseBPFAppId": "98e4832cfdafb547d94a259616b4cedc",
                    "armseBPFAutoEnable": "on",
                    "armseBPFCreateAppName": "qianlu-test-insights-server-1",
                    "pod-template-hash": "57cfdbc6cc"
                },
                "envs": {
                    "MOCK_SERVER_ID": "0",
                    "MYSQL_DATABASE": "arms_mock",
                    "SERVICE_NAME": "insights-server-1",
                    "spring.datasource.password": "root@1234",
                    "spring.datasource.url": "jdbc:mysql://mysql-pod:3306/arms_mock?characterEncoding=utf-8&useSSL=false",
                    "spring.datasource.username": "root",
                    "spring.redis.host": "redis",
                    "spring.redis.password": ""
                },
                "images": {
                    "insights-server-1": "registry.cn-hangzhou.aliyuncs.com/private-mesh/hellob:mockserver"
                },
                "containerIDs": [
                    "95c5d07f280b5cf8c7b372653720b761dc37ac79e5ba5c0066ddeedac4290187"
                ],
                "podIP": "10.208.196.157"
            },
            "68a5b76a-4688-4fa3-a30a-49463fcda276": {
                "podName": "storage-controller-b7df47c7c-pnfdl",
                "startTime": 1736409407,
                "namespace": "kube-system",
                "workloadName": "storage-controller",
                "workloadKind": "deployment",
                "labels": {
                    "app": "storage-controller",
                    "pod-template-hash": "b7df47c7c"
                },
                "envs": {

                },
                "images": {
                    "storage-controller": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/storage-controller:v1.31.1"
                },
                "containerIDs": [
                    "6846f8f4dcc01b12aa4c5dbef704cf847a05657ecf383b419466ab65890facb3"
                ],
                "podIP": "10.208.196.106"
            },
            "7463db32-11f9-461d-b6e7-b277f2da42fc": {
                "podName": "ad-service-server-55b886474d-h5cms",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "ad-service-server",
                "workloadKind": "deployment",
                "labels": {
                    "aliyun.com/app-language": "golang",
                    "app": "ad-service-server",
                    "armsPilotAutoEnable": "on",
                    "armsPilotCreateAppName": "ad-service-server",
                    "pod-template-hash": "55b886474d"
                },
                "envs": {

                },
                "images": {
                    "ad-service-server": "registry.cn-hangzhou.aliyuncs.com/private-mesh/hellob:ad-service-server"
                },
                "containerIDs": [
                    "f6be772fdd253dbca6dad033f7462083b32ccaed11f940ca20c30ead4f907646"
                ],
                "podIP": "10.208.196.121"
            },
            "7a5e4ebf-2480-4dad-a75e-26128758ac54": {
                "podName": "node-exporter-zxq4d",
                "startTime": 1740474514,
                "namespace": "arms-prom",
                "workloadName": "node-exporter",
                "workloadKind": "daemonset",
                "labels": {
                    "app": "node-exporter",
                    "controller-revision-hash": "847955cd64",
                    "pod-template-generation": "1"
                },
                "envs": {

                },
                "images": {
                    "kube-rbac-proxy": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/kube-rbac-proxy:v0.11.0-703dc7e4-aliyun",
                    "node-exporter": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/node-exporter:v0.17.0-slim"
                },
                "containerIDs": [
                    "21e3dc939d4818cf55feb5d6e169684cbd591096a1db22fb594a3cc4e9c3c472",
                    "d2885e2528b89e7f5ce3d643bc5031931607c0f1fd9c80f60008847e754c2b96"
                ],
                "podIP": "10.208.196.86"
            },
            "81c62a89-6707-4deb-a78f-481f2489cb65": {
                "podName": "redis-5ddddbc6cc-2q44f",
                "startTime": 1740556078,
                "namespace": "apm-demo",
                "workloadName": "redis",
                "workloadKind": "deployment",
                "labels": {
                    "app": "redis",
                    "pod-template-hash": "5ddddbc6cc"
                },
                "envs": {

                },
                "images": {
                    "redis": "redis"
                },
                "containerIDs": [
                    ""
                ],
                "podIP": "10.208.196.124"
            },
            "850583d8-d5f3-488c-850f-2ac5b107ca70": {
                "podName": "ad-recommend-kratos-server-84d999cc4d-5tffs",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "ad-recommend-kratos-server",
                "workloadKind": "deployment",
                "labels": {
                    "aliyun.com/app-language": "golang",
                    "app": "ad-recommend-kratos-server",
                    "armsPilotAutoEnable": "on",
                    "armsPilotCreateAppName": "ad-recommend-kratos-server",
                    "pod-template-hash": "84d999cc4d"
                },
                "envs": {

                },
                "images": {
                    "ad-recommend-kratos-server": "registry.cn-hangzhou.aliyuncs.com/private-mesh/hellob:ad-recommend-kratos-server"
                },
                "containerIDs": [
                    "5289a0a2a3615a89dfbc718a91ed7c840767472fd9466378391bc08c07563877"
                ],
                "podIP": "10.208.196.118"
            },
            "87c1e2dd-6d1d-4833-b2f8-e0adbbdb3876": {
                "podName": "continuous-profiling-7ff8c7ddb-h65td",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "continuous-profiling",
                "workloadKind": "deployment",
                "labels": {
                    "app": "continuous-profiling",
                    "armsPilotAutoEnable": "on",
                    "armsPilotCreateAppName": "continuous-profiling",
                    "pod-template-hash": "7ff8c7ddb"
                },
                "envs": {

                },
                "images": {
                    "continuous-profiling": "xujing-register-registry.cn-hangzhou.cr.aliyuncs.com/public/register/test-pressure:1.1.1-SNAPSHOT"
                },
                "containerIDs": [
                    "8710b5c9139c039ae085e9e26340debc26005f051e5fb2b24d2ab1f155de29bb"
                ],
                "podIP": "10.208.196.120"
            },
            "8c61722e-280e-4052-9a8f-660a95723aef": {
                "podName": "mysql-pod-6648b55fdf-2fdq5",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "mysql-pod",
                "workloadKind": "deployment",
                "labels": {
                    "app": "mysql-pod",
                    "pod-template-hash": "6648b55fdf"
                },
                "envs": {
                    "MYSQL_ROOT_PASSWORD": "root@1234"
                },
                "images": {
                    "mysql-pod": "sy-demo-registry.cn-hangzhou.cr.aliyuncs.com/insights/demo-mysql:v1"
                },
                "containerIDs": [
                    "78d0aef316445ffceea6da49f00ae8c350ebc9745fad7c9e76397dedab860d87"
                ],
                "podIP": "10.208.196.127"
            },
            "99036709-7675-45f6-b247-dea9836ed915": {
                "podName": "kube-state-metrics-7d77db7d-rtsrm",
                "startTime": 1740474514,
                "namespace": "arms-prom",
                "workloadName": "kube-state-metrics",
                "workloadKind": "deployment",
                "labels": {
                    "k8s-app": "kube-state-metrics",
                    "pod-template-hash": "7d77db7d"
                },
                "envs": {

                },
                "images": {
                    "kube-state-metrics": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/kube-state-metrics:v2.3.0-b12986e-aliyun"
                },
                "containerIDs": [
                    "750e23f40cfe1b546e7069b867285b5609d89b2a5702aad6a690c9931b274e69"
                ],
                "podIP": "10.208.196.89"
            },
            "bd326144-b6d7-4044-a4c1-b39e6bfc3045": {
                "podName": "ot-collector-678795cb59-p6hxs",
                "startTime": 1740473057,
                "namespace": "arms-prom",
                "workloadName": "ot-collector",
                "workloadKind": "deployment",
                "labels": {
                    "app.kubernetes.io/component": "opentelemetry-collector",
                    "pod-template-hash": "678795cb59"
                },
                "envs": {
                    "CLUSTER_ID": "ceda8a4e802a9407db000c3802a2ef771",
                    "IS_K8S": "true",
                    "IS_PUBLIC": "private",
                    "MY_POD_NAME": "",
                    "NAMESPACE": "arms-prom",
                    "POD_NAME": "",
                    "REGION_ID": "cn-hangzhou",
                    "USER_ID": "1108555361245511",
                    "__ACCESSKEY_SECRET__": "__ACCESSKEY_SECRET__",
                    "__ACCESSKEY__": "__ACCESSKEY__"
                },
                "images": {
                    "ot-collector": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/alicollector:4.1.2"
                },
                "containerIDs": [
                    "35f69052bec86c17dd4aa4750e191f678333f9a88e89af6a6b562e6d10e43e05"
                ],
                "podIP": "10.208.196.138"
            },
            "c860ba4a-b9d5-4985-900e-bb101a249d58": {
                "podName": "o11y-addon-controller-78766bcc75-vksn8",
                "startTime": 1740474514,
                "namespace": "arms-prom",
                "workloadName": "o11y-addon-controller",
                "workloadKind": "deployment",
                "labels": {
                    "control-plane": "o11y-addon-controller",
                    "pod-template-hash": "78766bcc75"
                },
                "envs": {
                    "CLUSTER_ID": "ceda8a4e802a9407db000c3802a2ef771",
                    "CLUSTER_NAME": "ql-oneagent-test-heyuan",
                    "CLUSTER_RESOURCE_GROUP_ID": "",
                    "CLUSTER_TYPE": "ManagedKubernetes/Default",
                    "PROMETHEUS_TYPE": "default",
                    "REGION": "cn-hangzhou",
                    "TOKEN_MODE": "ack",
                    "USER_ID": "1108555361245511"
                },
                "images": {
                    "controller": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/o11y-addon-controller:v1.1.6-aliyun"
                },
                "containerIDs": [
                    "df44dccf9d5dac54af31dc265710f677302791f91e40107996ecd56c0bf5370a"
                ],
                "podIP": "10.208.196.156"
            },
            "d11c5f94-4fc6-452c-a570-9faaf82395f4": {
                "podName": "loongcollector-operator-5467d58c97-2qfjk",
                "startTime": 1740479341,
                "namespace": "loongcollector",
                "workloadName": "loongcollector-operator",
                "workloadKind": "deployment",
                "labels": {
                    "app.kubernetes.io/instance": "loongcollector",
                    "app.kubernetes.io/name": "loongcollector",
                    "pod-template-hash": "5467d58c97"
                },
                "envs": {
                    "ARMS_PILOT_NAMESPACE": "loongcollector",
                    "ENABLE_WEBHOOKS": "true",
                    "GLOBAL_ACS_K8S_FLAG": "true",
                    "GLOBAL_AGENT_ENDPOINT": "http://logtail-statefulset.loongcollector:18689/export/port",
                    "GLOBAL_CLIENT_AUTH_VERSION": "v4",
                    "GLOBAL_CLUSTER_AGENT_SERVICE_NAMESPACE": "loongcollector",
                    "GLOBAL_CLUSTER_AGENT_SERVICE_SELECTOR": "logtail-statefulset",
                    "GLOBAL_CLUSTER_AGENT_SERVICE_SWITCH": "false"
                },
                "images": {
                    "operator": "arms-deploy-registry.cn-hangzhou.cr.aliyuncs.com/arms-deploy-repo/arms-prometheus-agent:danque-0107200205"
                },
                "containerIDs": [
                    "e8929c128f9421e079609af2cd4f7aa48746635d9097aba7f532e562ed8e5a06"
                ],
                "podIP": "10.208.196.158"
            },
            "d1dd1c15-2af3-4f9e-b35c-ba32542b91de": {
                "podName": "ack-node-local-dns-admission-controller-547bf44d8d-bg8ml",
                "startTime": 1736409146,
                "namespace": "kube-system",
                "workloadName": "ack-node-local-dns-admission-controller",
                "workloadKind": "deployment",
                "labels": {
                    "app": "ack-node-local-dns-admission-controller",
                    "pod-template-hash": "547bf44d8d"
                },
                "envs": {

                },
                "images": {
                    "webhook": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/node-local-dns-admission-controller:v1.1.4-aliyun"
                },
                "containerIDs": [
                    "8c095c010729284dfff76ff31424c793948f5b8f406cb8f0b609d03a9b727394"
                ],
                "podIP": "10.208.196.104"
            },
            "dd495db5-2b26-4619-aa06-c9bb0c89c1db": {
                "podName": "ad-redis-server-58cc6ccd4d-4qnlz",
                "startTime": 1736414341,
                "namespace": "insights",
                "workloadName": "ad-redis-server",
                "workloadKind": "deployment",
                "labels": {
                    "aliyun.com/app-language": "golang",
                    "app": "ad-redis-server",
                    "armsPilotAutoEnable": "on",
                    "armsPilotCreateAppName": "ad-redis-server",
                    "pod-template-hash": "58cc6ccd4d"
                },
                "envs": {
                    "ARMS_REDIS_ADDR": "redis.insights:6379"
                },
                "images": {
                    "ad-redis-server": "registry.cn-hangzhou.aliyuncs.com/private-mesh/hellob:ad-redis-server"
                },
                "containerIDs": [
                    "1eaa940a752c3a189b76856e7b54393c110d097eaa70d971e4301025095bb4c5"
                ],
                "podIP": "10.208.196.117"
            },
            "e08b4fa5-696c-4665-83d5-700e8fe370ea": {
                "podName": "csi-plugin-mgg4n",
                "startTime": 1736409295,
                "namespace": "kube-system",
                "workloadName": "csi-plugin",
                "workloadKind": "daemonset",
                "labels": {
                    "app": "csi-plugin",
                    "app.kubernetes.io/name": "csi-plugin",
                    "controller-revision-hash": "674c577fc4",
                    "nodepool": "default",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "KUBE_NODE_NAME": ""
                },
                "images": {
                    "csi-plugin": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/csi-plugin:v1.32.1-35c87ee-aliyun",
                    "disk-driver-registrar": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/csi-node-driver-registrar:v2.9.0-d48d2e0-aliyun",
                    "nas-driver-registrar": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/csi-node-driver-registrar:v2.9.0-d48d2e0-aliyun",
                    "oss-driver-registrar": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/csi-node-driver-registrar:v2.9.0-d48d2e0-aliyun"
                },
                "containerIDs": [
                    "b42f6498ebdb75b578d25c2f021c7a47558d032df1efe5f74f4f793524a27ad5",
                    "ba210270c1a1930315c491ed4ea57a73b06de38aae322d509c69653f99055d56",
                    "5335d2adcacbb049ad35b397c87420a6177e7a40863bac62cb7310abe51bf449",
                    "989f8f65a056b0af4e9a2728f5be0601247187053b51bade118593b83ac8e772"
                ],
                "podIP": "10.208.196.86"
            },
            "e8d5d47a-af74-4504-8001-e9997bae4463": {
                "podName": "mall-gateway-f557ffd4b-klj5t",
                "startTime": 1740635636,
                "namespace": "apm-demo",
                "workloadName": "mall-gateway",
                "workloadKind": "deployment",
                "labels": {
                    "app": "mall-gateway",
                    "armseBPFAppId": "6f9ef8be18ab01725c35812ff4968dff",
                    "armseBPFAutoEnable": "on",
                    "armseBPFCreateAppName": "qianlu-pressure-test-app",
                    "pod-template-hash": "f557ffd4b"
                },
                "envs": {
                    "ARMS_APP_NAME": "perf-auto-mall-gateway",
                    "MOCK_SERVER_ID": "0",
                    "MYSQL_DATABASE": "arms_mock",
                    "SERVICE_NAME": "mall-gateway",
                    "VM_OPTS": "-Xms3500m -Xmx3500m -XX:MetaspaceSize=500m",
                    "spring.datasource.password": "root@1234",
                    "spring.datasource.url": "jdbc:mysql://mysql-pod:3306/arms_mock?characterEncoding=utf-8&useSSL=false",
                    "spring.datasource.username": "root",
                    "spring.redis.host": "redis",
                    "spring.redis.password": ""
                },
                "images": {
                    "mall-gateway": "sy-demo-registry.cn-hangzhou.cr.aliyuncs.com/insights/insights-demo:helm-stress-test"
                },
                "containerIDs": [
                    "988b1c7c87358a6b42d34d2643284f93e4ca5fbd0043d8ce4c4d75a46b1552e3"
                ],
                "podIP": "10.208.196.122"
            },
            "f3c6408e-e2b1-43c8-b7f6-7210f14e3088": {
                "podName": "node-local-dns-9ddhj",
                "startTime": 1736409295,
                "namespace": "kube-system",
                "workloadName": "node-local-dns",
                "workloadKind": "daemonset",
                "labels": {
                    "controller-revision-hash": "cfdddf64d",
                    "k8s-app": "node-local-dns",
                    "pod-template-generation": "1"
                },
                "envs": {

                },
                "images": {
                    "node-cache": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/k8s-dns-node-cache:v1.22.28.1-5f96b759-aliyun"
                },
                "containerIDs": [
                    "5983960e4a716f050a733fbe4832e2db9b94555db161dad4bea39a7ffe1148fd"
                ],
                "podIP": "10.208.196.86"
            },
            "f4adbbf6-a7ab-49c2-9ec1-f791c4bb15bd": {
                "podName": "kube-eventer-init-v1.8-e43647f-aliyun-1.2.20-tn65x",
                "startTime": 1736409356,
                "namespace": "kube-system",
                "workloadName": "kube-eventer-init-v1.8-e43647f-aliyun-1.2.20",
                "workloadKind": "job",
                "labels": {
                    "batch.kubernetes.io/controller-uid": "efb8d015-1773-4db1-b071-564d12dda629",
                    "batch.kubernetes.io/job-name": "kube-eventer-init-v1.8-e43647f-aliyun-1.2.20",
                    "controller-uid": "efb8d015-1773-4db1-b071-564d12dda629",
                    "job-name": "kube-eventer-init-v1.8-e43647f-aliyun-1.2.20"
                },
                "envs": {

                },
                "images": {
                    "kube-eventer-init": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/kube-eventer-init:v1.8-e43647f-aliyun"
                },
                "containerIDs": [
                    "9746a05ee9f59d32caca6d28bef4056eb72d5d70e5d6e39a4c4d2c0ccdf47907"
                ],
                "podIP": "10.208.196.106"
            },
            "f5ec8294-8b77-4867-a6bd-834ad030e383": {
                "podName": "o11y-init-environment-1-sgn8t",
                "startTime": 1740474514,
                "namespace": "arms-prom",
                "workloadName": "o11y-init-environment-1",
                "workloadKind": "job",
                "labels": {
                    "batch.kubernetes.io/controller-uid": "9bb9829d-8e0a-4708-b3f8-5b379075d839",
                    "batch.kubernetes.io/job-name": "o11y-init-environment-1",
                    "controller-uid": "9bb9829d-8e0a-4708-b3f8-5b379075d839",
                    "job-name": "o11y-init-environment-1"
                },
                "envs": {
                    "CLUSTER_ID": "ceda8a4e802a9407db000c3802a2ef771",
                    "CLUSTER_NAME": "ql-oneagent-test-heyuan",
                    "CLUSTER_RESOURCE_GROUP_ID": "",
                    "CLUSTER_TYPE": "ManagedKubernetes/Default",
                    "PROMETHEUS_TYPE": "default",
                    "REGION": "cn-hangzhou",
                    "TOKEN_MODE": "ack",
                    "USER_ID": "1108555361245511"
                },
                "images": {
                    "init-env": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/o11y-addon-controller:v1.1.6-aliyun"
                },
                "containerIDs": [
                    "c00c2c73ecdf5d11971640e6e567ed0714e506862c1f49501b06f17c5c1555ae"
                ],
                "podIP": "10.208.196.155"
            },
            "f8801c95-2e02-4de0-a20d-452cd6da936a": {
                "podName": "cmonitor-agent-fwhq5",
                "startTime": 1740473057,
                "namespace": "arms-prom",
                "workloadName": "cmonitor-agent",
                "workloadKind": "daemonset",
                "labels": {
                    "app": "cmonitor-agent",
                    "controller-revision-hash": "6b9d9f7bcc",
                    "pod-template-generation": "1"
                },
                "envs": {
                    "AGENT_TYPE": "cmonitor",
                    "AGENT_VERSION": "4.1.2",
                    "INSPECTOR_NODENAME": "",
                    "KUBERNETES_CLUSTERID": "ceda8a4e802a9407db000c3802a2ef771",
                    "KUBERNETES_NAME": "ceda8a4e802a9407db000c3802a2ef771",
                    "POD_IP": "",
                    "POD_NAME": "",
                    "REGION_ID": "cn-hangzhou",
                    "USER_ID": "1108555361245511"
                },
                "images": {
                    "inspector": "registry-cn-hangzhou-vpc.ack.aliyuncs.com/acs/cmonitor-agent:4.1.2"
                },
                "containerIDs": [
                    "18329be18a6e82fd6834f40f028c43e855f819cf1e7547344d53ec11e1e272e6"
                ],
                "podIP": "10.208.196.86"
            }
        })";

        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errors;
        auto res = reader->parse(jsonData.c_str(), jsonData.c_str() + jsonData.size(), &root, &errors);
        APSARA_TEST_TRUE(res);
        std::shared_ptr<ContainerData> data = std::make_shared<ContainerData>();
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, containerInfoType::HostInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(containerInfoType::HostInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByIpFromCache("10.208.196.155");
        APSARA_TEST_TRUE(container != nullptr);

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string eventStr = R"({
        "events" :
        [
{
        "name": "test",
        "tags": {
            "remote_ip": "10.208.196.155"
        },
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "type" : 2,
        "value": {
            "type": "untyped_single_value",
            "detail": 10.0
        }
    }
        ],
        "metadata" :
        {
            "log.file.path_resolved" : "/var/log/message"
        },
        "tags" :
        {
            "app_name" : "xxx"
        }
    })";
        auto& k8sMetadata = K8sMetadata::GetInstance();
        eventGroup.FromJsonString(eventStr);
        eventGroup.AddMetricEvent();
        auto processor = std::make_unique<LabelingK8sMetadata>();
        std::vector<std::string> container_vec;
        std::vector<std::string> remote_ip_vec;
        EventsContainer& events = eventGroup.MutableEvents();
        processor->AddLabels(events[0].Cast<MetricEvent>(), container_vec, remote_ip_vec);
        EventsContainer& eventsEnd = eventGroup.MutableEvents();
        auto& metricEvent = eventsEnd[0].Cast<MetricEvent>();
        APSARA_TEST_EQUAL("o11y-init-environment-1", metricEvent.GetTag("peerWorkloadName").to_string());
        APSARA_TEST_TRUE_FATAL(k8sMetadata.GetInfoByIpFromCache("10.208.196.86") != nullptr);
    }

    void TestNetworkCheck() {
        auto& k8sMetadata = K8sMetadata::GetInstance();
        APSARA_TEST_TRUE(k8sMetadata.mIsValid);
        APSARA_TEST_TRUE(k8sMetadata.mEnable);
        for (int i = 0; i < 10; i++) {
            // fail request
            k8sMetadata.AsyncQueryMetadata(containerInfoType::IpInfo, "192.168.0." + std::to_string(i));
            k8sMetadata.AsyncQueryMetadata(containerInfoType::IpInfo, "192.168.0." + std::to_string(i + 1));
            k8sMetadata.AsyncQueryMetadata(containerInfoType::IpInfo, "192.168.0." + std::to_string(i + 2));
            // shoule query for 10 times ...
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        APSARA_TEST_GT(k8sMetadata.mFailCount, 5);
        APSARA_TEST_FALSE(k8sMetadata.mIsValid);
    }
};

APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByContainerIds, 0);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByLocalHost, 1);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAddLabelToMetric, 2);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAddLabelToSpan, 3);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestExternalIpOperations, 4);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAsyncQueryMetadata, 5);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestNetworkCheck, 6);

} // end of namespace logtail

UNIT_TEST_MAIN
