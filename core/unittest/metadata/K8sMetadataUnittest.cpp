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
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, PodInfoType::ContainerIdInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(PodInfoType::ContainerIdInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(
            "286effd2650c0689b779018e42e9ec7aa3d2cb843005e038204e85fc3d4f9144");
        APSARA_TEST_TRUE(container != nullptr);
        APSARA_TEST_EQUAL(container->mNamespace, "default");
        APSARA_TEST_EQUAL(container->mWorkloadName, "oneagent-demo-658648895b");
        APSARA_TEST_EQUAL(container->mWorkloadKind, "replicaset");
        APSARA_TEST_EQUAL(container->mAppId, "");
        APSARA_TEST_EQUAL(container->mAppName, "");
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
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, PodInfoType::IpInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(PodInfoType::IpInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache("192.16..10.1");
        APSARA_TEST_TRUE(container != nullptr);
        APSARA_TEST_EQUAL(container->mNamespace, "default");
        APSARA_TEST_EQUAL(container->mWorkloadName, "oneagent-demo-658648895b");
        APSARA_TEST_EQUAL(container->mWorkloadKind, "replicaset");
        APSARA_TEST_EQUAL(container->mAppId, "");
        APSARA_TEST_EQUAL(container->mAppName, "");
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
        res = K8sMetadata::GetInstance().FromContainerJson(root, data, PodInfoType::IpInfo);
        APSARA_TEST_TRUE(res);
        APSARA_TEST_TRUE(data != nullptr);
        std::vector<std::string> resKey;
        // update cache
        K8sMetadata::GetInstance().HandleMetadataResponse(PodInfoType::IpInfo, data, resKey);
        auto container = K8sMetadata::GetInstance().GetInfoByIpFromCache("172.16.20.108");
        APSARA_TEST_TRUE(container != nullptr);
    }

    void TestNetworkCheck() {
        auto& k8sMetadata = K8sMetadata::GetInstance();
        APSARA_TEST_TRUE(k8sMetadata.mIsValid);
        APSARA_TEST_TRUE(k8sMetadata.mEnable);
        APSARA_TEST_EQUAL(k8sMetadata.mFailCount, 0);
        for (int i = 0; i < 10; i++) {
            // fail request
            k8sMetadata.AsyncQueryMetadata(PodInfoType::IpInfo, "192.168.0." + std::to_string(i));
            k8sMetadata.AsyncQueryMetadata(PodInfoType::IpInfo, "192.168.0." + std::to_string(i + 1));
            k8sMetadata.AsyncQueryMetadata(PodInfoType::IpInfo, "192.168.0." + std::to_string(i + 2));
            // shoule query for 10 times ...
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        APSARA_TEST_GT(k8sMetadata.mFailCount, 5);
        APSARA_TEST_FALSE(k8sMetadata.mIsValid);
    }

    void TestBuildAsyncQuery() {
        std::vector<std::string> keys = {"1", "2", "3"};
        auto req = K8sMetadata::GetInstance().BuildAsyncRequest(
            keys,
            PodInfoType::ContainerIdInfo,
            []() { return true; },
            [](const std::vector<std::string>& podIpVec) { LOG_INFO(sLogger, ("size", podIpVec.size())); });
        APSARA_TEST_TRUE(req != nullptr);
        LOG_INFO(sLogger,
                 ("host", req->mHost)("url", req->mUrl)("query string",
                                                        req->mQueryString)("body", req->mBody)("method", req->mMethod));
        APSARA_TEST_EQUAL(req->mHost, K8sMetadata::GetInstance().mServiceHost);
        APSARA_TEST_EQUAL(req->mMethod, "GET");
        APSARA_TEST_EQUAL(req->mUrl, "/metadata/containerid");

        req = K8sMetadata::GetInstance().BuildAsyncRequest(
            keys,
            PodInfoType::IpInfo,
            []() { return true; },
            [](const std::vector<std::string>& podIpVec) { LOG_INFO(sLogger, ("size", podIpVec.size())); });
        APSARA_TEST_TRUE(req != nullptr);
        LOG_INFO(sLogger,
                 ("host", req->mHost)("url", req->mUrl)("query string",
                                                        req->mQueryString)("body", req->mBody)("method", req->mMethod));
        APSARA_TEST_EQUAL(req->mHost, K8sMetadata::GetInstance().mServiceHost);
        APSARA_TEST_EQUAL(req->mMethod, "GET");
        APSARA_TEST_EQUAL(req->mUrl, "/metadata/ipport");

        req = K8sMetadata::GetInstance().BuildAsyncRequest(
            keys,
            PodInfoType::HostInfo,
            []() { return true; },
            [](const std::vector<std::string>& podIpVec) { LOG_INFO(sLogger, ("size", podIpVec.size())); });
        APSARA_TEST_TRUE(req != nullptr);
        LOG_INFO(sLogger,
                 ("host", req->mHost)("url", req->mUrl)("query string",
                                                        req->mQueryString)("body", req->mBody)("method", req->mMethod));
        APSARA_TEST_EQUAL(req->mHost, K8sMetadata::GetInstance().mServiceHost);
        APSARA_TEST_EQUAL(req->mMethod, "GET");
        APSARA_TEST_EQUAL(req->mUrl, "/metadata/host");
    }
};

APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByContainerIds, 0);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByLocalHost, 1);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestExternalIpOperations, 2);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAsyncQueryMetadata, 3);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestNetworkCheck, 4);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestBuildAsyncQuery, 5);

} // end of namespace logtail

UNIT_TEST_MAIN
