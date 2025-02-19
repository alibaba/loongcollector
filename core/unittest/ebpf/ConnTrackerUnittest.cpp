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

#include "ebpf/plugin/network_observer/ConnTracker.h"
#include "ebpf/plugin/network_observer/ConnTrackerManager.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class ConnTrackerUnittest : public ::testing::Test {
public:
    void TestBasicOperations();
    void TestStateTransitions();
    void TestProtocolHandling();
    void TestMetadataManagement();
    void TestConcurrentOperations();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    // 辅助函数
    std::shared_ptr<ConnTracker> CreateTestTracker() {
        ConnId connId(1, 1000, 123456);
        return std::make_shared<ConnTracker>(connId);
    }

    void ValidateTrackerState(const std::shared_ptr<ConnTracker>& tracker,
                              bool expectedClose,
                              support_role_e expectedRole,
                              int expectedEpoch) {
        EXPECT_EQ(tracker->IsClose(), expectedClose);
        EXPECT_EQ(tracker->GetRole(), expectedRole);
        EXPECT_EQ(tracker->GetEpoch(), expectedEpoch);
    }
};

void ConnTrackerUnittest::TestBasicOperations() {
    // 测试基本构造
    auto tracker = CreateTestTracker();
    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 10);

    // 测试连接ID
    ConnId expectedId(1, 1000, 123456);
    EXPECT_EQ(tracker->GetConnId(), expectedId);

    // 测试活跃度记录
    tracker->RecordActive();
    auto now = std::chrono::steady_clock::now();
    EXPECT_FALSE(tracker->ReadyToDestroy(now));

    // 测试计数器
    tracker->CountDownLatch();
    EXPECT_EQ(tracker->GetEpoch(), 9);
}

void ConnTrackerUnittest::TestStateTransitions() {
    auto tracker = CreateTestTracker();

    // 测试初始状态
    ValidateTrackerState(tracker, false, support_role_e::IsUnknown, 10);

    tracker->RecordActive();

    // 测试角色转换
    tracker->SafeUpdateRole(support_role_e::IsClient);
    EXPECT_EQ(tracker->GetRole(), support_role_e::IsClient);

    tracker->SafeUpdateRole(support_role_e::IsServer);
    EXPECT_EQ(tracker->GetRole(), support_role_e::IsServer);

    // 测试关闭状态
    tracker->MarkClose();
    EXPECT_TRUE(tracker->IsClose());

    // 测试超时检测
    auto futureTime = std::chrono::steady_clock::now() + std::chrono::seconds(121);
    EXPECT_TRUE(tracker->ReadyToDestroy(futureTime));
}

void ConnTrackerUnittest::TestProtocolHandling() {
    auto tracker = CreateTestTracker();

    // 测试协议更新
    tracker->SafeUpdateProtocol(ProtocolType::HTTP);

    // 测试连接统计更新
    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;

    tracker->UpdateConnStats(&statsEvent);

    // 验证IP地址
    EXPECT_EQ(tracker->GetSourceIp(), "127.0.0.1");
    EXPECT_EQ(tracker->GetRemoteIp(), "192.168.1.1");
}

void ConnTrackerUnittest::TestMetadataManagement() {
    auto tracker = CreateTestTracker();

    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;

    // attach net metadata
    tracker->UpdateConnStats(&statsEvent);
    EXPECT_FALSE(tracker->MetaAttachReadyForApp());

    // attach self pod metadata
    auto podInfo = std::make_shared<k8sContainerInfo>();
    podInfo->containerIds = {"1", "2"};
    podInfo->podIp = "test-pod-ip";
    podInfo->podName = "test-pod-name";
    podInfo->k8sNamespace = "test-namespace";

    tracker->UpdateSelfPodMeta(podInfo);
    EXPECT_FALSE(tracker->MetaAttachReadyForApp());

    // attach peer pod metadata
    auto peerPodInfo = std::make_shared<k8sContainerInfo>();
    peerPodInfo->containerIds = {"3", "4"};
    peerPodInfo->podIp = "peer-pod-ip";
    peerPodInfo->podName = "peer-pod-name";
    peerPodInfo->k8sNamespace = "peer-namespace";

    tracker->UpdatePeerPodMeta(peerPodInfo);
    EXPECT_TRUE(tracker->MetaAttachReadyForApp());
}

void ConnTrackerUnittest::TestConcurrentOperations() {
    auto tracker = CreateTestTracker();
    const int threadCount = 10;
    std::vector<std::thread> threads;

    // 并发更新状态
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([tracker]() {
            for (int j = 0; j < 100; ++j) {
                tracker->RecordActive();
                tracker->SafeUpdateRole(support_role_e::IsClient);
                tracker->SafeUpdateProtocol(ProtocolType::HTTP);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 验证最终状态
    EXPECT_EQ(tracker->GetRole(), support_role_e::IsClient);
    EXPECT_FALSE(tracker->IsClose());
}

UNIT_TEST_CASE(ConnTrackerUnittest, TestConcurrentOperations);
UNIT_TEST_CASE(ConnTrackerUnittest, TestMetadataManagement);
UNIT_TEST_CASE(ConnTrackerUnittest, TestProtocolHandling);
UNIT_TEST_CASE(ConnTrackerUnittest, TestBasicOperations);
UNIT_TEST_CASE(ConnTrackerUnittest, TestStateTransitions);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
