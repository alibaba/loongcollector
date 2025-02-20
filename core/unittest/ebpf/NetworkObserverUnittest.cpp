// Copyright 2023 iLogtail Authors
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

#include <memory>
#include <thread>

#include "common/TimeUtil.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/SourceManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/plugin/network_observer/NetworkObserverManager.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "metadata/K8sMetadata.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class NetworkObserverManagerUnittest : public ::testing::Test {
public:
    void TestInitialization();
    void TestEventHandling();
    void TestDataEventProcessing();
    void TestWhitelistManagement();
    void TestPerfBufferOperations();
    void TestRecordProcessing();
    void TestAggregation();
    void TestProtocolParsing();
    void TestErrorHandling();
    void TestPluginLifecycle();

protected:
    void SetUp() override {
        mTimer = std::make_shared<Timer>();
        mTimer->Init();
        mSourceManager = std::make_shared<SourceManager>();
        mSourceManager->Init();
        mBaseManager = std::make_shared<ProcessCacheManager>(mSourceManager, "test_host", "/", mEventQueue);
        ProtocolParserManager::GetInstance().AddParser(ProtocolType::HTTP);
    }

    void TearDown() override { mTimer->Stop(); }

private:
    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(mBaseManager, mSourceManager, mEventQueue, mTimer);
    }

    void ValidateNetDataEvent(const std::unique_ptr<NetDataEvent>& event,
                              const ConnId& expectedConnId,
                              ProtocolType expectedProtocol,
                              support_role_e expectedRole) {
        EXPECT_NE(event, nullptr);
        if (event) {
            APSARA_TEST_EQUAL(event->mConnection->GetConnId(), expectedConnId);
            APSARA_TEST_EQUAL(event->mConnection->GetProtocol(), expectedProtocol);
            APSARA_TEST_EQUAL(event->mConnection->GetRole(), expectedRole);
        }
    }

    std::shared_ptr<Timer> mTimer;
    std::shared_ptr<SourceManager> mSourceManager;
    std::shared_ptr<ProcessCacheManager> mBaseManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
};

void NetworkObserverManagerUnittest::TestInitialization() {
    auto manager = CreateManager();
    EXPECT_NE(manager, nullptr);

    // 测试正常初始化
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP", "MySQL", "Redis"};
    options.mEnableCids = {"container1", "container2"};
    options.mDisableCids = {"container3"};

    int result = manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);
    EXPECT_EQ(manager->GetPluginType(), PluginType::NETWORK_OBSERVE);

    // 测试销毁
    result = manager->Destroy();
    EXPECT_EQ(result, 0);
}

void NetworkObserverManagerUnittest::TestEventHandling() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 测试连接建立事件
    struct conn_ctrl_event_t connectEvent = {};
    connectEvent.conn_id.fd = 1;
    connectEvent.conn_id.tgid = 1000;
    connectEvent.conn_id.start = 123456;
    connectEvent.type = EventConnect;
    manager->AcceptNetCtrlEvent(&connectEvent);

    // 测试连接统计事件
    struct conn_stats_event_t statsEvent = {};
    statsEvent.conn_id = connectEvent.conn_id;
    statsEvent.protocol = support_proto_e::ProtoHTTP;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.si.family = AF_INET;
    statsEvent.si.netns = 12345;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    manager->AcceptNetStatsEvent(&statsEvent);

    // 测试连接关闭事件
    struct conn_ctrl_event_t closeEvent = connectEvent;
    closeEvent.type = EventClose;
    manager->AcceptNetCtrlEvent(&closeEvent);

    // 测试事件丢失处理
    manager->RecordEventLost(callback_type_e::CTRL_HAND, 1);
    manager->RecordEventLost(callback_type_e::INFO_HANDLE, 2);
    manager->RecordEventLost(callback_type_e::STAT_HAND, 3);
}

std::shared_ptr<Connection> CreateTestTracker() {
    ConnId connId(1, 1000, 123456);
    return std::make_shared<Connection>(connId);
}

conn_data_event_t* CreateHttpDataEvent() {
    const std::string resp = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 13\r\n"
                             "\r\n"
                             "Hello, World!";
    const std::string req = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
                            "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string msg = req + resp;
    conn_data_event_t* evt = (conn_data_event_t*)malloc(offsetof(conn_data_event_t, msg) + msg.size());
    memcpy(evt->msg, msg.data(), msg.size());
    evt->conn_id.fd = 0;
    evt->conn_id.start = 1;
    evt->conn_id.tgid = 2;
    evt->role = support_role_e::IsClient;
    evt->request_len = req.size();
    evt->response_len = resp.size();
    evt->protocol = support_proto_e::ProtoHTTP;
    evt->start_ts = 1;
    evt->end_ts = 2;
    return evt;
}

conn_data_event_t* CreateHttpDataEvent(int i) {
    const std::string resp = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 13\r\n"
                             "\r\n"
                             "Hello, World!";
    const std::string req = "GET /index.html/" + std::to_string(i)
        + " HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
          "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string msg = req + resp;
    conn_data_event_t* evt = (conn_data_event_t*)malloc(offsetof(conn_data_event_t, msg) + msg.size());
    memcpy(evt->msg, msg.data(), msg.size());
    evt->conn_id.fd = 0;
    evt->conn_id.start = 1;
    evt->conn_id.tgid = 2;
    evt->role = support_role_e::IsClient;
    evt->request_len = req.size();
    evt->response_len = resp.size();
    evt->protocol = support_proto_e::ProtoHTTP;
    evt->start_ts = 1;
    evt->end_ts = 2;
    return evt;
}

conn_stats_event_t CreateConnStatsEvent() {
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
    statsEvent.conn_id.fd = 0;
    statsEvent.conn_id.start = 1;
    statsEvent.conn_id.tgid = 2;
    // docker id
    std::string testCid
        = "/machine.slice/libpod-80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106.scope";
    memcpy(statsEvent.docker_id, testCid.c_str(), testCid.size());
    return statsEvent;
}

void NetworkObserverManagerUnittest::TestDataEventProcessing() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    manager->Stop();

    auto statsEvent = CreateConnStatsEvent();
    manager->AcceptNetStatsEvent(&statsEvent);

    auto* dataEvent = CreateHttpDataEvent();
    manager->AcceptDataEvent(dataEvent);
    free(dataEvent);

    std::vector<std::shared_ptr<AbstractRecord>> items(10, nullptr);
    size_t count = manager->mRecordQueue.wait_dequeue_bulk_timed(items.data(), 1024, std::chrono::milliseconds(200));
    APSARA_TEST_EQUAL(count, 1);
    APSARA_TEST_TRUE(items[0] != nullptr);

    AbstractAppRecord* record = static_cast<AbstractAppRecord*>(items[0].get());
    APSARA_TEST_TRUE(record != nullptr);
    auto conn = record->GetConnection();
    APSARA_TEST_TRUE(conn != nullptr);

    APSARA_TEST_TRUE(manager->mConnectionManager->GetConnection(conn->GetConnId()) != nullptr);

    // destroy connection
    conn->UnsafeMarkClose();
    for (size_t i = 0; i < 12; i++) {
        manager->mConnectionManager->Iterations(i);
    }

    // connection that record holds still available
    APSARA_TEST_TRUE(manager->mConnectionManager->GetConnection(conn->GetConnId()) == nullptr);

    // verify attributes
    HttpRecord* httpRecord = static_cast<HttpRecord*>(record);
    // http attrs
    APSARA_TEST_EQUAL(httpRecord->GetPath(), "/index.html");
    APSARA_TEST_EQUAL(httpRecord->GetSpanName(), "/index.html");
    APSARA_TEST_EQUAL(httpRecord->GetStatusCode(), 200);
    APSARA_TEST_EQUAL(httpRecord->GetStartTimeStamp(), 1);
    APSARA_TEST_EQUAL(httpRecord->GetEndTimeStamp(), 2);

    auto attrs = httpRecord->GetConnection()->GetConnTrackerAttrs();
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kLocalAddr.Name())], "127.0.0.1:8080");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())], "192.168.1.1:80");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
}

void NetworkObserverManagerUnittest::TestWhitelistManagement() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 测试添加白名单
    std::vector<std::string> enableCids = {"container1", "container2"};
    std::vector<std::string> disableCids;
    manager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    // 测试添加黑名单
    enableCids.clear();
    disableCids = {"container3", "container4"};
    manager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    // 测试同时更新白名单和黑名单
    enableCids = {"container5"};
    disableCids = {"container6"};
    manager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));
}

void NetworkObserverManagerUnittest::TestPerfBufferOperations() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 测试正常轮询
    int result = manager->PollPerfBuffer();
    EXPECT_EQ(result, 0);

    // 测试连续轮询
    for (int i = 0; i < 5; i++) {
        result = manager->PollPerfBuffer();
        EXPECT_EQ(result, 0);
    }
}

void NetworkObserverManagerUnittest::TestRecordProcessing() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    options.mEnableLog = true;
    options.mEnableMetric = true;
    options.mEnableSpan = true;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    auto podInfo = std::make_shared<k8sContainerInfo>();
    podInfo->containerIds = {"1", "2"};
    podInfo->podIp = "test-pod-ip";
    podInfo->podName = "test-pod-name";
    podInfo->k8sNamespace = "test-namespace";

    LOG_INFO(sLogger, ("step", "0-0"));
    K8sMetadata::GetInstance().containerCache.insert("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106",
                                                     podInfo);

    auto peerPodInfo = std::make_shared<k8sContainerInfo>();
    peerPodInfo->containerIds = {"3", "4"};
    peerPodInfo->podIp = "peer-pod-ip";
    peerPodInfo->podName = "peer-pod-name";
    peerPodInfo->k8sNamespace = "peer-namespace";
    K8sMetadata::GetInstance().ipCache.insert("192.168.1.1", peerPodInfo);

    auto statsEvent = CreateConnStatsEvent();
    manager->AcceptNetStatsEvent(&statsEvent);

    // Generate 10 records
    for (size_t i = 0; i < 100; i++) {
        auto* dataEvent = CreateHttpDataEvent(i);
        manager->AcceptDataEvent(dataEvent);
        free(dataEvent);
    }
    // verify
    auto now = std::chrono::steady_clock::now();
    LOG_INFO(sLogger, ("====== consume span ======", ""));
    APSARA_TEST_TRUE(manager->ConsumeSpanAggregateTree(now));
    LOG_INFO(sLogger, ("====== consume metric ======", ""));
    APSARA_TEST_TRUE(manager->ConsumeMetricAggregateTree(now));
    LOG_INFO(sLogger, ("====== consume log ======", ""));
    APSARA_TEST_TRUE(manager->ConsumeLogAggregateTree(now));
}

void NetworkObserverManagerUnittest::TestAggregation() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    auto now = std::chrono::steady_clock::now();

    // 测试日志聚合
    bool result = manager->ConsumeLogAggregateTree(now);
    EXPECT_TRUE(result);

    // 测试指标聚合
    result = manager->ConsumeMetricAggregateTree(now);
    EXPECT_TRUE(result);

    // 测试追踪聚合
    result = manager->ConsumeSpanAggregateTree(now);
    EXPECT_TRUE(result);
}

void NetworkObserverManagerUnittest::TestProtocolParsing() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 测试添加多个协议解析器
    std::vector<std::string> protocols = {"HTTP", "MySQL", "Redis", "Dubbo"};
    bool result = manager->UpdateParsers(protocols);
    EXPECT_TRUE(result);

    // 测试更新协议解析器
    protocols = {"HTTP", "MySQL"};
    result = manager->UpdateParsers(protocols);
    EXPECT_TRUE(result);

    // 测试清空协议解析器
    protocols.clear();
    result = manager->UpdateParsers(protocols);
    EXPECT_TRUE(result);
}

void NetworkObserverManagerUnittest::TestPluginLifecycle() {
    auto manager = CreateManager();

    // 测试初始化
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    int result = manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);

    // 测试更新配置
    options.mEnableProtocols = {"HTTP", "MySQL"};
    result = manager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);

    // 测试销毁
    result = manager->Destroy();
    EXPECT_EQ(result, 0);
}

UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestInitialization);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestEventHandling);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestDataEventProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestWhitelistManagement);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPerfBufferOperations);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestRecordProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestAggregation);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestProtocolParsing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPluginLifecycle);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
