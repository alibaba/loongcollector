// Copyright 2025 iLogtail Authors
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

#include <chrono>
#include <gtest/gtest.h>

#include <memory>
#include <thread>

#include "ebpf/plugin/network_observer/Connection.h"
#include "ebpf/plugin/network_observer/ConnectionManager.h"
#include "metadata/K8sMetadata.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {


class ConnectionUnittest : public ::testing::Test {
public:
    void TestBasicOperations();
    void TestStateTransitions();
    void TestProtocolHandling();
    void TestMetadataManagement();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    std::shared_ptr<Connection> CreateTestTracker() {
        ConnId connId(1, 1000, 123456);
        return std::make_shared<Connection>(connId);
    }

    void ValidateTrackerState(const std::shared_ptr<Connection>& tracker,
                              bool expectedClose,
                              support_role_e expectedRole,
                              int expectedEpoch) {
        APSARA_TEST_EQUAL(tracker->IsClose(), expectedClose);
        support_role_e role = tracker->GetRole();
        APSARA_TEST_EQUAL(role, expectedRole);
        APSARA_TEST_EQUAL(tracker->GetEpoch(), expectedEpoch);
    }
};

void ConnectionUnittest::TestBasicOperations() {
    auto tracker = CreateTestTracker();
    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 10);

    ConnId expectedId(1, 1000, 123456);
    APSARA_TEST_EQUAL(tracker->GetConnId(), expectedId);

    tracker->RecordActive();
    auto now = std::chrono::steady_clock::now();
    APSARA_TEST_FALSE(tracker->ReadyToDestroy(now));

    tracker->CountDown();
    APSARA_TEST_EQUAL(tracker->GetEpoch(), 9);
}

void ConnectionUnittest::TestStateTransitions() {
    auto tracker = CreateTestTracker();

    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 10);

    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;
    // set docker id
    statsEvent.wr_bytes = 1;
    tracker->UpdateConnStats(&statsEvent);
    tracker->RecordActive();

    auto futureTime = std::chrono::steady_clock::now() + std::chrono::seconds(121);
    APSARA_TEST_TRUE(tracker->ReadyToDestroy(futureTime));

    tracker->UnsafeMarkClose();
    tracker->RecordActive();
    futureTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    APSARA_TEST_FALSE(tracker->ReadyToDestroy(futureTime));
    for (size_t i = 0; i < 12; i++) {
        tracker->CountDown();
    }
    APSARA_TEST_TRUE(tracker->ReadyToDestroy(futureTime));
}

void ConnectionUnittest::TestProtocolHandling() {
    auto tracker = CreateTestTracker();

    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsUnknown;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;
    // set docker id
    statsEvent.wr_bytes = 1;

    // will update protocol but will not update role
    tracker->UpdateConnStats(&statsEvent);
    APSARA_TEST_FALSE(tracker->mProtocolAttached);
    ProtocolType pt = tracker->GetProtocol();
    auto& attrs = tracker->GetConnTrackerAttrs();
    APSARA_TEST_EQUAL(pt, ProtocolType::HTTP);
    APSARA_TEST_EQUAL(tracker->GetSourceIp(), "127.0.0.1");
    APSARA_TEST_EQUAL(tracker->GetRemoteIp(), "192.168.1.1");
    // role not set, so we cannot fill rpc attr
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "");

    // mock receive a data event
    tracker->SafeUpdateRole(support_role_e::IsClient);
    tracker->SafeUpdateProtocol(ProtocolType::HTTP);
    APSARA_TEST_TRUE(tracker->mProtocolAttached);
    // now rpc attributes all set
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");

    // role chage ...
    tracker->SafeUpdateRole(support_role_e::IsServer);
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");

    // protocol change ...
    tracker->SafeUpdateProtocol(ProtocolType::MYSQL);
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
    APSARA_TEST_EQUAL(tracker->GetProtocol(), ProtocolType::HTTP);
}

void ConnectionUnittest::TestMetadataManagement() {
    auto tracker = CreateTestTracker();

    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsUnknown;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;
    // docker id
    std::string testCid
        = "/machine.slice/libpod-80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106.scope";
    memcpy(statsEvent.docker_id, testCid.c_str(), testCid.size());

    // attach net metadata
    tracker->UpdateConnStats(&statsEvent);
    APSARA_TEST_EQUAL(tracker->GetContainerId(), "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106");
    APSARA_TEST_FALSE(tracker->MetaAttachReadyForApp());

    APSARA_TEST_EQUAL(tracker->GetRemoteIp(), "192.168.1.1");

    APSARA_TEST_EQUAL(tracker->GetSelfMetadataAttachStatus(), MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER);
    APSARA_TEST_EQUAL(tracker->GetPeerMetadataAttachStatus(), MetadataAttachStatus::WAIT_QUERY_REMOTE_SERVER);

    APSARA_TEST_FALSE(tracker->mK8sMetaAttached);
    APSARA_TEST_FALSE(tracker->mK8sPeerMetaAttached);
    APSARA_TEST_TRUE(tracker->mNetMetaAttached);
    APSARA_TEST_FALSE(tracker->mProtocolAttached);
    LOG_INFO(sLogger, ("step", "0"));

    // add k8s metadata cache
    // attach self pod metadata
    auto podInfo = std::make_shared<k8sContainerInfo>();
    podInfo->containerIds = {"1", "2"};
    podInfo->podIp = "test-pod-ip";
    podInfo->podName = "test-pod-name";
    podInfo->k8sNamespace = "test-namespace";

    LOG_INFO(sLogger, ("step", "0-0"));
    K8sMetadata::GetInstance().containerCache.insert(tracker->GetContainerId(), podInfo);
    LOG_INFO(sLogger, ("step", "0-1"));

    tracker->TryAttachSelfMeta();
    LOG_INFO(sLogger, ("step", "0-2"));
    tracker->TryAttachPeerMeta();
    LOG_INFO(sLogger, ("step", "0-3"));
    APSARA_TEST_TRUE(tracker->mK8sMetaAttached);
    APSARA_TEST_FALSE(tracker->mK8sPeerMetaAttached);
    APSARA_TEST_TRUE(tracker->mNetMetaAttached);
    APSARA_TEST_FALSE(tracker->mProtocolAttached);

    LOG_INFO(sLogger, ("step", "1"));

    // attach peer pod metadata
    auto peerPodInfo = std::make_shared<k8sContainerInfo>();
    peerPodInfo->containerIds = {"3", "4"};
    peerPodInfo->podIp = "peer-pod-ip";
    peerPodInfo->podName = "peer-pod-name";
    peerPodInfo->k8sNamespace = "peer-namespace";
    K8sMetadata::GetInstance().ipCache.insert(tracker->GetRemoteIp(), peerPodInfo);
    LOG_INFO(sLogger, ("step", "2"));

    tracker->TryAttachSelfMeta();
    tracker->TryAttachPeerMeta();
    K8sMetadata::GetInstance().ipCache.remove(tracker->GetRemoteIp());
    K8sMetadata::GetInstance().containerCache.remove(tracker->GetContainerId());
    APSARA_TEST_TRUE(tracker->mK8sMetaAttached);
    APSARA_TEST_TRUE(tracker->mK8sPeerMetaAttached);
    APSARA_TEST_TRUE(tracker->mNetMetaAttached);
    APSARA_TEST_FALSE(tracker->mProtocolAttached);
    LOG_INFO(sLogger, ("step", "3"));

    // mock receive data event ...
    tracker->SafeUpdateRole(support_role_e::IsClient);
    tracker->SafeUpdateProtocol(ProtocolType::HTTP);
    tracker->RecordActive();
    APSARA_TEST_TRUE(tracker->mK8sMetaAttached);
    APSARA_TEST_TRUE(tracker->mK8sPeerMetaAttached);
    APSARA_TEST_TRUE(tracker->mNetMetaAttached);
    APSARA_TEST_TRUE(tracker->mProtocolAttached);
    LOG_INFO(sLogger, ("step", "4"));

    APSARA_TEST_TRUE(tracker->MetaAttachReadyForApp());
}

UNIT_TEST_CASE(ConnectionUnittest, TestBasicOperations);
UNIT_TEST_CASE(ConnectionUnittest, TestProtocolHandling);

UNIT_TEST_CASE(ConnectionUnittest, TestMetadataManagement);
UNIT_TEST_CASE(ConnectionUnittest, TestStateTransitions);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
