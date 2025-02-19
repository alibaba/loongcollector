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

class ConnTrackerManagerUnittest : public ::testing::Test {
public:
    void TestBasicOperations();
    void TestEventHandling();
    void TestTimeoutMechanism();
    void TestConcurrentAccess();
    void TestMetadataHandling();
    void TestProtocolDetection();
    void TestResourceManagement();
    void TestErrorHandling();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    // 辅助函数
    std::shared_ptr<ConnTrackerManager> CreateManager() { return ConnTrackerManager::Create(200, 15); }

    ConnId CreateTestConnId(int fd = 1, uint32_t tgid = 1000, uint64_t start = 123456) {
        return ConnId(fd, tgid, start);
    }

    void ValidateTracker(const std::shared_ptr<ConnTracker>& tracker,
                         bool shouldExist,
                         support_role_e expectedRole = support_role_e::IsUnknown) {
        if (shouldExist) {
            EXPECT_NE(tracker, nullptr);
            if (tracker) {
                EXPECT_EQ(tracker->GetRole(), expectedRole);
            }
        } else {
            EXPECT_EQ(tracker, nullptr);
        }
    }
};

void ConnTrackerManagerUnittest::TestBasicOperations() {
    auto manager = CreateManager();

    // 测试创建跟踪器
    auto connId = CreateTestConnId();
    auto tracker = manager->GetOrCreateConntracker(connId);
    ValidateTracker(tracker, true);

    // 测试获取已存在的跟踪器
    auto existingTracker = manager->GetConntracker(connId);
    EXPECT_EQ(existingTracker, tracker);

    // 测试删除跟踪器
    manager->DeleteConnTracker(connId);
    auto nullTracker = manager->GetConntracker(connId);
    ValidateTracker(nullTracker, false);
}

void ConnTrackerManagerUnittest::TestEventHandling() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();

    // 测试连接建立事件
    struct conn_ctrl_event_t connectEvent = {};
    connectEvent.conn_id.fd = connId.fd;
    connectEvent.conn_id.tgid = connId.tgid;
    connectEvent.conn_id.start = connId.start;
    connectEvent.type = EventConnect;
    connectEvent.ts = 1;

    manager->AcceptNetCtrlEvent(&connectEvent);
    auto tracker = manager->GetConntracker(connId);
    ValidateTracker(tracker, true);

    // 测试数据事件
    struct conn_data_event_t dataEvent = {};
    dataEvent.conn_id = connectEvent.conn_id;
    dataEvent.protocol = support_proto_e::ProtoHTTP;

    manager->AcceptNetDataEvent(&dataEvent);

    // 测试统计事件
    struct conn_stats_event_t statsEvent = {};
    statsEvent.conn_id = connectEvent.conn_id;
    statsEvent.si.family = AF_INET;
    statsEvent.si.netns = 12345;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.ts = 2;

    manager->AcceptNetStatsEvent(&statsEvent);
    tracker = manager->GetConntracker(connId);
    ValidateTracker(tracker, true, support_role_e::IsClient);

    // 测试连接关闭事件
    struct conn_ctrl_event_t closeEvent = connectEvent;
    closeEvent.type = EventClose;

    manager->AcceptNetCtrlEvent(&closeEvent);
    tracker = manager->GetConntracker(connId);
    EXPECT_TRUE(tracker->IsClose());
}

void ConnTrackerManagerUnittest::TestTimeoutMechanism() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();

    // 创建并关闭连接
    auto tracker = manager->GetOrCreateConntracker(connId);
    ValidateTracker(tracker, true);

    struct conn_ctrl_event_t closeEvent = {};
    closeEvent.conn_id.fd = connId.fd;
    closeEvent.conn_id.tgid = connId.tgid;
    closeEvent.conn_id.start = connId.start;
    closeEvent.type = EventClose;

    manager->AcceptNetCtrlEvent(&closeEvent);

    // 执行清理
    for (size_t i = 0; i < 12; i++) {
        manager->IterationsInternal(i);
    }

    // 验证连接已被清理
    auto nullTracker = manager->GetConntracker(connId);
    ValidateTracker(nullTracker, false);
}

void ConnTrackerManagerUnittest::TestConcurrentAccess() {
    auto manager = CreateManager();
    const int threadCount = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([this, manager, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                auto connId = CreateTestConnId(j, i * 1000 + j);

                // 创建跟踪器
                auto tracker = manager->GetOrCreateConntracker(connId);
                ValidateTracker(tracker, true);

                // 更新状态
                struct conn_stats_event_t statsEvent = {};
                statsEvent.conn_id.fd = connId.fd;
                statsEvent.conn_id.tgid = connId.tgid;
                statsEvent.conn_id.start = connId.start;
                statsEvent.protocol = support_proto_e::ProtoHTTP;
                statsEvent.role = support_role_e::IsClient;

                manager->AcceptNetStatsEvent(&statsEvent);

                // 删除跟踪器
                manager->DeleteConnTracker(connId);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

void ConnTrackerManagerUnittest::TestMetadataHandling() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();
    auto tracker = manager->GetOrCreateConntracker(connId);

    // 测试网络元数据
    struct conn_stats_event_t statsEvent = {};
    statsEvent.conn_id.fd = connId.fd;
    statsEvent.conn_id.tgid = connId.tgid;
    statsEvent.conn_id.start = connId.start;
    statsEvent.si.family = AF_INET;
    statsEvent.si.netns = 12345;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;

    manager->AcceptNetStatsEvent(&statsEvent);

    // 验证元数据
    auto updatedTracker = manager->GetConntracker(connId);
    EXPECT_EQ(updatedTracker->GetSourceIp(), "127.0.0.1");
    EXPECT_EQ(updatedTracker->GetRemoteIp(), "192.168.1.1");
}

void ConnTrackerManagerUnittest::TestProtocolDetection() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();
    auto tracker = manager->GetOrCreateConntracker(connId);

    // 测试网络元数据
    struct conn_stats_event_t statsEvent = {};
    statsEvent.conn_id.fd = connId.fd;
    statsEvent.conn_id.tgid = connId.tgid;
    statsEvent.conn_id.start = connId.start;
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.si.family = AF_INET;
    statsEvent.si.netns = 12345;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;
    manager->AcceptNetStatsEvent(&statsEvent);

    // 验证协议属性
    auto attrs = manager->GetConnTrackerAttrs(connId);
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kIp.name())], "127.0.0.1");
}

void ConnTrackerManagerUnittest::TestResourceManagement() {
    auto manager = CreateManager();
    std::vector<ConnId> connIds;
    const int connectionCount = 100;

    // 创建多个连接
    for (int i = 0; i < connectionCount; ++i) {
        auto connId = CreateTestConnId(i);
        connIds.push_back(connId);
        auto tracker = manager->GetOrCreateConntracker(connId);
        ValidateTracker(tracker, true);
    }

    // 关闭一半的连接
    for (int i = 0; i < connectionCount / 2; ++i) {
        struct conn_ctrl_event_t closeEvent = {};
        closeEvent.conn_id.fd = connIds[i].fd;
        closeEvent.conn_id.tgid = connIds[i].tgid;
        closeEvent.conn_id.start = connIds[i].start;
        closeEvent.type = EventClose;

        manager->AcceptNetCtrlEvent(&closeEvent);
    }

    // 执行清理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (size_t i = 0; i < 12; i++) {
        manager->IterationsInternal(i);
    }

    // 验证资源状态
    for (int i = 0; i < connectionCount; ++i) {
        auto tracker = manager->GetConntracker(connIds[i]);
        if (i < connectionCount / 2) {
            ValidateTracker(tracker, false);
        } else {
            ValidateTracker(tracker, true);
        }
    }
}

void ConnTrackerManagerUnittest::TestErrorHandling() {
    auto manager = CreateManager();

    // 测试无效的连接ID
    ConnId invalidConnId(-1, 0, 0);
    auto nullTracker = manager->GetConntracker(invalidConnId);
    ValidateTracker(nullTracker, false);

    // 测试重复删除
    auto connId = CreateTestConnId();
    manager->DeleteConnTracker(connId);
    manager->DeleteConnTracker(connId); // 不应导致错误
}

UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestBasicOperations);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestEventHandling);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestTimeoutMechanism);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestConcurrentAccess);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestMetadataHandling);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestProtocolDetection);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestResourceManagement);
UNIT_TEST_CASE(ConnTrackerManagerUnittest, TestErrorHandling);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
