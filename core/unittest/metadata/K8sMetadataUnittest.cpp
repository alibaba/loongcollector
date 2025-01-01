#include "unittest/Unittest.h"
#include <string>
#include <memory>
#include <vector>
#include "metadata/LabelingK8sMetadata.h"
#include "metadata/K8sMetadata.h"
#include "models/PipelineEventGroup.h"
#include <json/json.h>

using namespace std;

DECLARE_FLAG_STRING(ilogtail_config);

namespace logtail {

class k8sMetadataUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for k8s metadata
        const std::string jsonData = R"({
            "containerd://286effd2650c0689b779018e42e9ec7aa3d2cb843005e038204e85fc3d4f9144":{"namespace":"default","workloadName":"oneagent-demo-658648895b","workloadKind":"replicaset","serviceName":"","labels":{"app":"oneagent-demo","pod-template-hash":"658648895b"},"envs":{},"images":{"oneagent-demo":"sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/centos7-cve-fix:1.0.0"}},
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
        std::istringstream jsonStream(jsonData);
        std::string errors;

        // Parse JSON data
        if (!Json::parseFromStream(readerBuilder, jsonStream, &parsedJsonData, &errors)) {
            std::cerr << "Failed to parse JSON: " << errors << std::endl;
            return;
        }

        k8sMetadata = K8sMetadata::GetInstance();
        k8sMetadata->SetContainerCache(parsedJsonData);
        k8sMetadata->SetIpCache(parsedJsonData);
        
    }

    void TearDown() override {
        // Clear the data cache for K8sMetadata to speed up the recovery time between tests.
        //auto& k8sMetadata = K8sMetadata::GetInstance();
        k8sMetadata->ClearCache();
        //delete k8sMetadata;
        return;
    }

private:
    Json::Value parsedJsonData;
    K8sMetadata* k8sMetadata;
public:
    void TestGetByContainerIds() {
        LOG_INFO(sLogger, ("TestGetByContainerIds() begin", time(NULL)));
        //auto& k8sMetadata = K8sMetadata::GetInstance();
        APSARA_TEST_TRUE_FATAL(k8sMetadata->GetInfoByContainerIdFromCache("containerd://286effd2650c0689b779018e42e9ec7aa3d2cb843005e038204e85fc3d4f9144") != nullptr);
    }

    void TestGetByLocalHost() {
        LOG_INFO(sLogger, ("TestGetByLocalHost() begin", time(NULL)));
        //auto& k8sMetadata = K8sMetadata::GetInstance();
        
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
            "log.file.path" : "/var/log/message"
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
        APSARA_TEST_TRUE_FATAL(k8sMetadata->GetInfoByIpFromCache("10.41.0.2") != nullptr);
    }

    void TestAddLabelToSpan() {
        LOG_INFO(sLogger, ("TestProcessEventForSpan() begin", time(NULL)));
        //auto& k8sMetadata = K8sMetadata::GetInstance();
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
        LabelingK8sMetadata processor;
        processor.AddLabels(*mSpanEvent, container_vec, remote_ip_vec);
        APSARA_TEST_EQUAL("kube-proxy-worker", mSpanEvent->GetTag("peerWorkloadName").to_string());
        APSARA_TEST_TRUE_FATAL(k8sMetadata->GetInfoByIpFromCache("10.41.0.2") != nullptr);
        delete l;
        delete e;
    }

    void TestAddLabelToMetric() {
        LOG_INFO(sLogger, ("TestGetByLocalHost() begin", time(NULL)));
       // auto& k8sMetadata = K8sMetadata::GetInstance();
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
            "log.file.path" : "/var/log/message"
        },
        "tags" :
        {
            "app_name" : "xxx"
        }
    })";
        eventGroup.FromJsonString(eventStr);
        eventGroup.AddMetricEvent();
        LabelingK8sMetadata processor;
        std::vector<std::string> container_vec;
        std::vector<std::string> remote_ip_vec;
        EventsContainer& events = eventGroup.MutableEvents();
        processor.AddLabels(events[0].Cast<MetricEvent>(), container_vec, remote_ip_vec);
        EventsContainer& eventsEnd = eventGroup.MutableEvents();
        auto& metricEvent = eventsEnd[0].Cast<MetricEvent>();
        APSARA_TEST_EQUAL("kube-proxy-worker", metricEvent.GetTag("peerWorkloadName").to_string());
        APSARA_TEST_TRUE_FATAL(k8sMetadata->GetInfoByIpFromCache("10.41.0.2") != nullptr);
    }
};

APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByContainerIds, 0);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestGetByLocalHost, 1);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAddLabelToMetric, 2);
APSARA_UNIT_TEST_CASE(k8sMetadataUnittest, TestAddLabelToSpan, 3);

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
