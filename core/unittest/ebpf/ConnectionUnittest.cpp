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

#include <gtest/gtest.h>

#include <chrono>
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
    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 4);

    ConnId expectedId(1, 1000, 123456);
    APSARA_TEST_EQUAL(tracker->GetConnId(), expectedId);

    tracker->RecordActive();
    auto now = std::chrono::steady_clock::now();
    APSARA_TEST_FALSE(tracker->ReadyToDestroy(now));

    tracker->CountDown();
    APSARA_TEST_EQUAL(tracker->GetEpoch(), 3);
}

void ConnectionUnittest::TestStateTransitions() {
    auto tracker = CreateTestTracker();

    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 4);

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

    tracker->MarkClose();
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
    APSARA_TEST_FALSE(tracker->IsL7MetaAttachReady());
    support_proto_e pt = tracker->GetProtocol();
    const StaticDataRow<&kConnTrackerTable>& attrs = tracker->GetConnTrackerAttrs();
    APSARA_TEST_EQUAL(pt, support_proto_e::ProtoHTTP);
    APSARA_TEST_EQUAL(tracker->GetSourceIp(), "127.0.0.1");
    APSARA_TEST_EQUAL(tracker->GetRemoteIp(), "192.168.1.1");
    // role not set, so we cannot fill rpc attr
    APSARA_TEST_EQUAL(attrs.Get<kRpcType>(), "");
    APSARA_TEST_EQUAL(attrs.Get<kCallKind>(), "");
    APSARA_TEST_EQUAL(attrs.Get<kCallType>(), "");

    LOG_DEBUG(sLogger, ("connection", tracker->DumpConnection()));

    // mock receive a data event
    // tracker->UpdateRole(support_role_e::IsClient);
    // tracker->UpdateProtocol(support_proto_e::ProtoHTTP);
    tracker->TryAttachL7Meta(support_role_e::IsClient, support_proto_e::ProtoHTTP);
    APSARA_TEST_TRUE(tracker->IsL7MetaAttachReady());
    APSARA_TEST_EQUAL(attrs.Get<kRpcType>(), "25");
    APSARA_TEST_EQUAL(attrs.Get<kCallKind>(), "http_client");
    APSARA_TEST_EQUAL(attrs.Get<kCallType>(), "http_client");
    // now rpc attributes all set
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
    LOG_DEBUG(sLogger, ("connection", tracker->DumpConnection()));

    // role chage ...
    // tracker->UpdateRole(support_role_e::IsServer);
    // APSARA_TEST_EQUAL(attrs.Get<kRpcType>(), "25");
    // APSARA_TEST_EQUAL(attrs.Get<kCallKind>(), "http_client");
    // APSARA_TEST_EQUAL(attrs.Get<kCallType>(), "http_client");

    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
    // LOG_DEBUG(sLogger, ("connection", tracker->DumpConnection()));

    // protocol change ...
    // tracker->UpdateProtocol(support_proto_e::ProtoMySQL);
    // APSARA_TEST_EQUAL(std::string(attrs.Get<kRpcType>()), "25");
    // APSARA_TEST_EQUAL(std::string(attrs.Get<kCallKind>()), "http_client");
    // APSARA_TEST_EQUAL(std::string(attrs.Get<kCallType>()), "http_client");

    // APSARA_TEST_EQUAL(std::string(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())]), "25");
    // APSARA_TEST_EQUAL(std::string(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())]), "http_client");
    // APSARA_TEST_EQUAL(std::string(attrs[kConnTrackerTable.ColIndex(kCallType.Name())]), "http_client");
    // APSARA_TEST_EQUAL(tracker->GetProtocol(), support_proto_e::ProtoHTTP);
    // LOG_DEBUG(sLogger, ("connection", tracker->DumpConnection()));
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

    LOG_DEBUG(sLogger, ("flags", tracker->GetMetaFlags()));

    // attach net metadata
    tracker->UpdateConnStats(&statsEvent);
    APSARA_TEST_EQUAL(tracker->GetContainerId(), "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106");

    APSARA_TEST_FALSE(tracker->IsPeerMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsL4MetaAttachReady());
    APSARA_TEST_FALSE(tracker->IsL7MetaAttachReady());
    APSARA_TEST_FALSE(tracker->IsSelfMetaAttachReady());
    LOG_DEBUG(sLogger, ("flags", tracker->GetMetaFlags()));
    APSARA_TEST_FALSE(tracker->IsMetaAttachReadyForAppRecord());

    APSARA_TEST_EQUAL(tracker->GetRemoteIp(), "192.168.1.1");

    LOG_INFO(sLogger, ("step", "0"));

    // add k8s metadata cache
    // attach self pod metadata
    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {"1", "2"};
    podInfo->mPodIp = "test-pod-ip";
    podInfo->mPodName = "test-pod-name";
    podInfo->mNamespace = "test-namespace";

    LOG_INFO(sLogger, ("step", "0-0"));
    K8sMetadata::GetInstance().mContainerCache.insert(std::string(tracker->GetContainerId()), podInfo);
    LOG_INFO(sLogger, ("step", "0-1"));

    tracker->TryAttachSelfMeta();
    LOG_INFO(sLogger, ("step", "0-2"));
    tracker->TryAttachPeerMeta();
    LOG_INFO(sLogger, ("step", "0-3"));
    APSARA_TEST_TRUE(tracker->IsSelfMetaAttachReady());
    APSARA_TEST_FALSE(tracker->IsPeerMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsL4MetaAttachReady());
    APSARA_TEST_FALSE(tracker->IsL7MetaAttachReady());

    LOG_INFO(sLogger, ("step", "1"));

    // attach peer pod metadata
    auto peerPodInfo = std::make_shared<K8sPodInfo>();
    peerPodInfo->mContainerIds = {"3", "4"};
    peerPodInfo->mPodIp = "peer-pod-ip";
    peerPodInfo->mPodName = "peer-pod-name";
    peerPodInfo->mNamespace = "peer-namespace";
    K8sMetadata::GetInstance().mIpCache.insert(std::string(tracker->GetRemoteIp()), peerPodInfo);
    LOG_INFO(sLogger, ("step", "2"));

    tracker->TryAttachSelfMeta();
    tracker->TryAttachPeerMeta();
    K8sMetadata::GetInstance().mIpCache.remove(std::string(tracker->GetRemoteIp()));
    K8sMetadata::GetInstance().mContainerCache.remove(std::string(tracker->GetContainerId()));
    tracker->IsL4MetaAttachReady();
    APSARA_TEST_TRUE(tracker->IsSelfMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsPeerMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsL4MetaAttachReady());
    APSARA_TEST_FALSE(tracker->IsL7MetaAttachReady());
    LOG_INFO(sLogger, ("step", "3"));

    // mock receive data event ...
    // tracker->UpdateRole(support_role_e::IsClient);
    // tracker->UpdateProtocol(support_proto_e::ProtoHTTP);
    tracker->TryAttachL7Meta(support_role_e::IsClient, support_proto_e::ProtoHTTP);
    tracker->RecordActive();
    APSARA_TEST_TRUE(tracker->IsSelfMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsPeerMetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsL4MetaAttachReady());
    APSARA_TEST_TRUE(tracker->IsL7MetaAttachReady());
    LOG_INFO(sLogger, ("step", "4"));

    APSARA_TEST_TRUE(tracker->IsMetaAttachReadyForAppRecord());
}

UNIT_TEST_CASE(ConnectionUnittest, TestBasicOperations);
UNIT_TEST_CASE(ConnectionUnittest, TestProtocolHandling);
UNIT_TEST_CASE(ConnectionUnittest, TestMetadataManagement);
UNIT_TEST_CASE(ConnectionUnittest, TestStateTransitions);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
