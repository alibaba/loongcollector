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
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/plugin/network_observer/NetworkObserverManager.h"
#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/type/NetworkObserverEvent.h"
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
    void TestConcurrency();
    void TestErrorHandling();
    void TestPluginLifecycle();

protected:
    void SetUp() override {
        mTimer = std::make_shared<Timer>();
        mTimer->Init();
        mSourceManager = std::make_shared<SourceManager>();
        mSourceManager->Init();
        mBaseManager = std::make_shared<BaseManager>(mSourceManager, "test_host", "/", mEventQueue);
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
            EXPECT_EQ(event->conn_id, expectedConnId);
            EXPECT_EQ(event->protocol, expectedProtocol);
            EXPECT_EQ(event->role, expectedRole);
        }
    }

    std::shared_ptr<Timer> mTimer;
    std::shared_ptr<SourceManager> mSourceManager;
    std::shared_ptr<BaseManager> mBaseManager;
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

void NetworkObserverManagerUnittest::TestDataEventProcessing() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    auto httpEvent = std::make_unique<NetDataEvent>(1, 1000, 123456);
    httpEvent->protocol = ProtocolType::HTTP;
    httpEvent->role = support_role_e::IsClient;
    httpEvent->req_msg = "GET /api/v1/test HTTP/1.1\r\nHost: example.com\r\nContent-Length: 0\r\n\r\n";
    httpEvent->resp_msg = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}";
    httpEvent->start_ts = 1;
    httpEvent->end_ts = 2;
    manager->EnqueueDataEvent(std::move(httpEvent));
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
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 创建不同类型的记录
    std::vector<std::shared_ptr<AbstractRecord>> records;

    // HTTP 记录
    auto httpRecord = std::make_shared<HttpRecord>(ConnId(1, 1000, 123456));
    httpRecord->SetPath("/api/v1/test");
    httpRecord->SetMethod("GET");
    httpRecord->SetStatusCode("200");
    records.push_back(httpRecord);

    // 连接统计记录
    auto statsRecord = std::make_shared<ConnStatsRecord>(ConnId(2, 2000, 234567));
    records.push_back(statsRecord);

    // 测试不同类型的记录处理
    manager->ConsumeRecordsAsEvent(records, records.size());
    manager->ConsumeRecordsAsMetric(records, records.size());
    manager->ConsumeRecordsAsTrace(records, records.size());
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

void NetworkObserverManagerUnittest::TestConcurrency() {
    auto manager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    const int threadCount = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&manager, i]() {
            // 模拟完整的连接生命周期
            struct conn_ctrl_event_t connectEvent = {};
            connectEvent.conn_id.fd = i;
            connectEvent.conn_id.tgid = 1000 + i;
            connectEvent.conn_id.start = 123456 + i;
            connectEvent.type = EventConnect;
            manager->AcceptNetCtrlEvent(&connectEvent);

            // 统计事件
            struct conn_stats_event_t statsEvent = {};
            statsEvent.conn_id = connectEvent.conn_id;
            statsEvent.protocol = support_proto_e::ProtoHTTP;
            statsEvent.role = support_role_e::IsClient;
            statsEvent.si.family = AF_INET;
            statsEvent.si.netns = 12345 + i;
            manager->AcceptNetStatsEvent(&statsEvent);

            // 请求数据
            auto reqEvent = std::make_unique<NetDataEvent>(i, 1000 + i, 123456 + i);
            reqEvent->protocol = ProtocolType::HTTP;
            reqEvent->role = support_role_e::IsClient;
            reqEvent->req_msg = "GET /api/v1/test HTTP/1.1\r\nHost: example.com\r\n\r\n";
            manager->EnqueueDataEvent(std::move(reqEvent));

            // 响应数据
            auto respEvent = std::make_unique<NetDataEvent>(i, 1000 + i, 123456 + i);
            respEvent->protocol = ProtocolType::HTTP;
            respEvent->role = support_role_e::IsServer;
            respEvent->resp_msg = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
            manager->EnqueueDataEvent(std::move(respEvent));

            // 关闭连接
            struct conn_ctrl_event_t closeEvent = connectEvent;
            closeEvent.type = EventClose;
            manager->AcceptNetCtrlEvent(&closeEvent);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
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
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestConcurrency);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPluginLifecycle);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
