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
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class ConnectionManagerUnittest : public ::testing::Test {
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
    std::shared_ptr<ConnectionManager> CreateManager() { return ConnectionManager::Create(200); }

    ConnId CreateTestConnId(int fd = 1, uint32_t tgid = 1000, uint64_t start = 123456) {
        return ConnId(fd, tgid, start);
    }

    void ValidateTracker(const std::shared_ptr<Connection>& tracker,
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

void ConnectionManagerUnittest::TestBasicOperations() {
    auto manager = CreateManager();

    auto connId = CreateTestConnId();
    auto tracker = manager->getOrCreateConnection(connId);
    ValidateTracker(tracker, true);

    auto existingTracker = manager->getConnection(connId);
    EXPECT_EQ(existingTracker, tracker);

    manager->deleteConnection(connId);
    auto nullTracker = manager->getConnection(connId);
    ValidateTracker(nullTracker, false);
}

void ConnectionManagerUnittest::TestEventHandling() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();

    struct conn_ctrl_event_t connectEvent = {};
    connectEvent.conn_id.fd = connId.fd;
    connectEvent.conn_id.tgid = connId.tgid;
    connectEvent.conn_id.start = connId.start;
    connectEvent.type = EventConnect;
    connectEvent.ts = 1;

    manager->AcceptNetCtrlEvent(&connectEvent);
    auto tracker = manager->getConnection(connId);
    ValidateTracker(tracker, true);

    struct conn_data_event_t dataEvent = {};
    dataEvent.conn_id = connectEvent.conn_id;
    dataEvent.protocol = support_proto_e::ProtoHTTP;
    dataEvent.role = support_role_e::IsClient;

    manager->AcceptNetDataEvent(&dataEvent);
    ValidateTracker(tracker, true, support_role_e::IsClient);

    struct conn_stats_event_t statsEvent = {};
    statsEvent.conn_id = connectEvent.conn_id;
    statsEvent.si.family = AF_INET;
    statsEvent.si.netns = 12345;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsUnknown;
    statsEvent.ts = 2;

    manager->AcceptNetStatsEvent(&statsEvent);
    tracker = manager->getConnection(connId);
    ValidateTracker(tracker, true, support_role_e::IsClient);

    struct conn_ctrl_event_t closeEvent = connectEvent;
    closeEvent.type = EventClose;

    manager->AcceptNetCtrlEvent(&closeEvent);
    tracker = manager->getConnection(connId);
    EXPECT_TRUE(tracker->IsClose());
}

void ConnectionManagerUnittest::TestTimeoutMechanism() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();

    auto tracker = manager->getOrCreateConnection(connId);
    ValidateTracker(tracker, true);

    struct conn_ctrl_event_t closeEvent = {};
    closeEvent.conn_id.fd = connId.fd;
    closeEvent.conn_id.tgid = connId.tgid;
    closeEvent.conn_id.start = connId.start;
    closeEvent.type = EventClose;

    manager->AcceptNetCtrlEvent(&closeEvent);

    for (size_t i = 0; i < 12; i++) {
        manager->Iterations();
    }

    auto nullTracker = manager->getConnection(connId);
    ValidateTracker(nullTracker, false);
}

void ConnectionManagerUnittest::TestMetadataHandling() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();
    auto tracker = manager->getOrCreateConnection(connId);

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

    auto updatedTracker = manager->getConnection(connId);
    EXPECT_EQ(updatedTracker->GetSourceIp(), "127.0.0.1");
    EXPECT_EQ(updatedTracker->GetRemoteIp(), "192.168.1.1");
}

void ConnectionManagerUnittest::TestProtocolDetection() {
    auto manager = CreateManager();
    auto connId = CreateTestConnId();
    auto tracker = manager->getOrCreateConnection(connId);

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

    auto& attrs = tracker->GetConnTrackerAttrs();
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kIp.Name())], "127.0.0.1");
}

void ConnectionManagerUnittest::TestResourceManagement() {
    auto manager = CreateManager();
    std::vector<ConnId> connIds;
    const int connectionCount = 100;

    for (int i = 0; i < connectionCount; ++i) {
        auto connId = CreateTestConnId(i);
        connIds.push_back(connId);
        auto tracker = manager->getOrCreateConnection(connId);
        ValidateTracker(tracker, true);
    }

    for (int i = 0; i < connectionCount / 2; ++i) {
        struct conn_ctrl_event_t closeEvent = {};
        closeEvent.conn_id.fd = connIds[i].fd;
        closeEvent.conn_id.tgid = connIds[i].tgid;
        closeEvent.conn_id.start = connIds[i].start;
        closeEvent.type = EventClose;

        manager->AcceptNetCtrlEvent(&closeEvent);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (size_t i = 0; i < 12; i++) {
        manager->Iterations();
    }

    for (int i = 0; i < connectionCount; ++i) {
        auto tracker = manager->getConnection(connIds[i]);
        if (i < connectionCount / 2) {
            ValidateTracker(tracker, false);
        } else {
            ValidateTracker(tracker, true);
        }
    }
}

void ConnectionManagerUnittest::TestErrorHandling() {
    auto manager = CreateManager();

    ConnId invalidConnId(-1, 0, 0);
    auto nullTracker = manager->getConnection(invalidConnId);
    ValidateTracker(nullTracker, false);

    auto connId = CreateTestConnId();
    manager->deleteConnection(connId);
    // re-delete
    manager->deleteConnection(connId);
}

UNIT_TEST_CASE(ConnectionManagerUnittest, TestBasicOperations);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestEventHandling);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestTimeoutMechanism);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestMetadataHandling);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestProtocolDetection);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestResourceManagement);
UNIT_TEST_CASE(ConnectionManagerUnittest, TestErrorHandling);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
