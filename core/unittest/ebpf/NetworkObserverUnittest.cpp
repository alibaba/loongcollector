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
#include <memory>
#include <thread>

#include "common/TimeUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/EBPFServer.h"
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
    void TestRollbackProcessing();
    void TestConfigUpdate();
    void TestErrorHandling();
    void TestPluginLifecycle();
    void TestHandleHostMetadataUpdate();
    void TestPeriodicalTask();
    void BenchmarkConsumeTask();

protected:
    void SetUp() override {
        Timer::GetInstance()->Init();
        AsynCurlRunner::GetInstance()->Stop();
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mEBPFAdapter->Init();
        DynamicMetricLabels dynamicLabels;
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mRef,
            MetricCategory::METRIC_CATEGORY_RUNNER,
            {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
            std::move(dynamicLabels));
        auto pollProcessEventsTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
        auto lossProcessEventsTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
        auto processCacheMissTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
        auto processCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
        auto processDataMapSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
        auto retryableEventCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE);
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(mEBPFAdapter,
                                                                     "test_host",
                                                                     "/",
                                                                     mEventQueue,
                                                                     pollProcessEventsTotal,
                                                                     lossProcessEventsTotal,
                                                                     processCacheMissTotal,
                                                                     processCacheSize,
                                                                     processDataMapSize,
                                                                     retryableEventCacheSize);
        ProtocolParserManager::GetInstance().AddParser(support_proto_e::ProtoHTTP);
        mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, nullptr);
        EBPFServer::GetInstance()->updatePluginState(PluginType::NETWORK_OBSERVE, "pipeline", "project", mManager);
    }

    void TearDown() override {
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
        mManager->Destroy();
        EBPFServer::GetInstance()->updatePluginState(PluginType::NETWORK_OBSERVE, "", "", nullptr);
    }

private:
    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, nullptr);
    }

    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    MetricsRecordRef mRef;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    std::shared_ptr<NetworkObserverManager> mManager;
};

void NetworkObserverManagerUnittest::TestInitialization() {
    // auto mManager = CreateManager();
    EXPECT_NE(mManager, nullptr);

    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP", "MySQL", "Redis"};
    options.mEnableCids = {"container1", "container2"};
    options.mDisableCids = {"container3"};

    int result = mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mManager->GetPluginType(), PluginType::NETWORK_OBSERVE);
}

void NetworkObserverManagerUnittest::TestEventHandling() {
    // auto mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, nullptr);
    EXPECT_NE(mManager, nullptr);
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    struct conn_ctrl_event_t connectEvent = {};
    connectEvent.conn_id.fd = 1;
    connectEvent.conn_id.tgid = 1000;
    connectEvent.conn_id.start = 123456;
    connectEvent.type = EventConnect;
    mManager->AcceptNetCtrlEvent(&connectEvent);

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
    mManager->AcceptNetStatsEvent(&statsEvent);

    struct conn_ctrl_event_t closeEvent = connectEvent;
    closeEvent.type = EventClose;
    mManager->AcceptNetCtrlEvent(&closeEvent);

    mManager->RecordEventLost(callback_type_e::CTRL_HAND, 1);
    mManager->RecordEventLost(callback_type_e::INFO_HANDLE, 2);
    mManager->RecordEventLost(callback_type_e::STAT_HAND, 3);
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
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    mManager->Destroy();

    auto statsEvent = CreateConnStatsEvent();
    mManager->AcceptNetStatsEvent(&statsEvent);

    auto* dataEvent = CreateHttpDataEvent();
    // TODO @qianlu.kk
    mManager->AcceptDataEvent(dataEvent);
    free(dataEvent);

    std::vector<std::shared_ptr<AbstractRecord>> items(10, nullptr);
    size_t count
        = mManager->mRollbackQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
    APSARA_TEST_EQUAL(count, 1UL);
    APSARA_TEST_TRUE(items[0] != nullptr);

    AbstractAppRecord* record = static_cast<AbstractAppRecord*>(items[0].get());
    APSARA_TEST_TRUE(record != nullptr);
    auto conn = record->GetConnection();
    APSARA_TEST_TRUE(conn != nullptr);

    APSARA_TEST_TRUE(mManager->mConnectionManager->getConnection(conn->GetConnId()) != nullptr);

    // destroy connection
    conn->MarkClose();
    for (size_t i = 0; i < 12; i++) {
        mManager->mConnectionManager->Iterations();
    }

    // connection that record holds still available
    APSARA_TEST_TRUE(mManager->mConnectionManager->getConnection(conn->GetConnId()) == nullptr);

    // verify attributes
    HttpRecord* httpRecord = static_cast<HttpRecord*>(record);
    // http attrs
    APSARA_TEST_EQUAL(httpRecord->GetPath(), "/index.html");
    APSARA_TEST_EQUAL(httpRecord->GetSpanName(), "/index.html");
    APSARA_TEST_EQUAL(httpRecord->GetStatusCode(), 200);
    APSARA_TEST_EQUAL(httpRecord->GetStartTimeStamp(), 1UL);
    APSARA_TEST_EQUAL(httpRecord->GetEndTimeStamp(), 2UL);

    auto& attrs = httpRecord->GetConnection()->GetConnTrackerAttrs();
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kLocalAddr.Name())], "127.0.0.1:8080");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())], "192.168.1.1:80");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
}

void NetworkObserverManagerUnittest::TestWhitelistManagement() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    std::vector<std::string> enableCids = {"container1", "container2"};
    std::vector<std::string> disableCids;
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    enableCids.clear();
    disableCids = {"container3", "container4"};
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    enableCids = {"container5"};
    disableCids = {"container6"};
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));
}

void NetworkObserverManagerUnittest::TestPerfBufferOperations() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    int result = mManager->PollPerfBuffer();
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 5; i++) {
        result = mManager->PollPerfBuffer();
        EXPECT_EQ(result, 0);
    }
}

void NetworkObserverManagerUnittest::TestRecordProcessing() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    options.mEnableLog = true;
    options.mEnableMetric = true;
    options.mEnableSpan = true;
    options.mSampleRate = 1;
    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {"1", "2"};
    podInfo->mAppName = "test-app-name";
    podInfo->mAppId = "test-app-id";
    podInfo->mPodIp = "test-pod-ip";
    podInfo->mPodName = "test-pod-name";
    podInfo->mNamespace = "test-namespace";
    podInfo->mWorkloadKind = "Deployment";
    podInfo->mWorkloadName = "test-workloadname";

    LOG_INFO(sLogger, ("step", "0-0"));
    K8sMetadata::GetInstance().mContainerCache.insert(
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106", podInfo);

    auto peerPodInfo = std::make_shared<K8sPodInfo>();
    peerPodInfo->mContainerIds = {"3", "4"};
    peerPodInfo->mPodIp = "peer-pod-ip";
    peerPodInfo->mPodName = "peer-pod-name";
    peerPodInfo->mNamespace = "peer-namespace";
    K8sMetadata::GetInstance().mIpCache.insert("192.168.1.1", peerPodInfo);

    auto statsEvent = CreateConnStatsEvent();
    mManager->AcceptNetStatsEvent(&statsEvent);
    auto cnn = mManager->mConnectionManager->getConnection({0, 2, 1});
    APSARA_TEST_TRUE(cnn != nullptr);
    APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
    APSARA_TEST_TRUE(cnn->IsPeerMetaAttachReady());
    APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
    APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());

    APSARA_TEST_TRUE(cnn->IsMetaAttachReadyForAppRecord());

    // Generate 10 records
    for (size_t i = 0; i < 100; i++) {
        auto* dataEvent = CreateHttpDataEvent(i);
        mManager->AcceptDataEvent(dataEvent);
        free(dataEvent);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    // verify
    auto now = std::chrono::steady_clock::now();
    LOG_INFO(sLogger, ("====== consume span ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeSpanAggregateTree(now));
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups[0].GetEvents().size(), 100UL);
    auto tags = mManager->mSpanEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump span tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 6UL);
    APSARA_TEST_EQUAL(tags["service.name"], "test-app-name");
    APSARA_TEST_EQUAL(tags["arms.appId"], "test-app-id");
    APSARA_TEST_EQUAL(tags["host.ip"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host.name"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["arms.app.type"], "ebpf");
    APSARA_TEST_EQUAL(tags["data_type"], "trace"); // used for route

    LOG_INFO(sLogger, ("====== consume metric ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeMetricAggregateTree(now));
    APSARA_TEST_EQUAL(mManager->mMetricEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mMetricEventGroups[0].GetEvents().size(), 301UL);
    tags = mManager->mMetricEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump metric tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 6UL);
    APSARA_TEST_EQUAL(tags["service"], "test-app-name");
    APSARA_TEST_EQUAL(tags["pid"], "test-app-id");
    APSARA_TEST_EQUAL(tags["serverIp"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["source"], "ebpf");
    APSARA_TEST_EQUAL(tags["data_type"], "metric"); // used for route
    LOG_INFO(sLogger, ("====== consume log ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeLogAggregateTree(now));
    APSARA_TEST_EQUAL(mManager->mLogEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mLogEventGroups[0].GetEvents().size(), 100UL);
    tags = mManager->mLogEventGroups[0].GetTags();
    APSARA_TEST_EQUAL(tags.size(), 1UL);
}

// TEST RollBack mechanism
void NetworkObserverManagerUnittest::TestRollbackProcessing() {
    // case1. caused by conn stats event comes later than data event ...
    {
        // auto mManager = CreateManager();
        ObserverNetworkOption options;
        options.mEnableProtocols = {"HTTP"};
        options.mEnableLog = true;
        options.mEnableMetric = true;
        options.mEnableSpan = true;
        mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mContainerIds = {"1", "2"};
        podInfo->mAppName = "test-app-name";
        podInfo->mAppId = "test-app-id";
        podInfo->mPodIp = "test-pod-ip";
        podInfo->mPodName = "test-pod-name";
        podInfo->mNamespace = "test-namespace";
        podInfo->mWorkloadKind = "Deployment";
        podInfo->mWorkloadName = "test-workloadname";

        LOG_INFO(sLogger, ("step", "0-0"));
        K8sMetadata::GetInstance().mContainerCache.insert(
            "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106", podInfo);

        auto peerPodInfo = std::make_shared<K8sPodInfo>();
        peerPodInfo->mContainerIds = {"3", "4"};
        peerPodInfo->mPodIp = "peer-pod-ip";
        peerPodInfo->mPodName = "peer-pod-name";
        peerPodInfo->mNamespace = "peer-namespace";
        K8sMetadata::GetInstance().mIpCache.insert("192.168.1.1", peerPodInfo);

        // Generate 10 records
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }
        auto cnn = mManager->mConnectionManager->getConnection({0, 2, 1});
        APSARA_TEST_FALSE(cnn->IsMetaAttachReadyForAppRecord());

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // conn stats arrive
        auto statsEvent = CreateConnStatsEvent();
        mManager->AcceptNetStatsEvent(&statsEvent);
        APSARA_TEST_TRUE(cnn != nullptr);
        APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsPeerMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());

        APSARA_TEST_TRUE(cnn->IsMetaAttachReadyForAppRecord());
        APSARA_TEST_EQUAL(mManager->mDropRecordTotal, 0);
        APSARA_TEST_EQUAL(mManager->mRollbackRecordTotal, 100);

        std::this_thread::sleep_for(std::chrono::seconds(5));
        APSARA_TEST_EQUAL(mManager->mDropRecordTotal, 0);
        APSARA_TEST_EQUAL(mManager->mRollbackRecordTotal, 100);

        // Generate 10 records
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        APSARA_TEST_EQUAL(mManager->mDropRecordTotal, 0);
        APSARA_TEST_EQUAL(mManager->mRollbackRecordTotal, 100);
    }

    // case2. caused by fetch metadata from server ...
    {
        // mock data event
        // mock conn stats event

        // mock iterations

        // mock async fetch metadata

        // verify
    }

    // case3. caused by no conn stats received ...
    // conn stats data may loss
    {}
}

void NetworkObserverManagerUnittest::TestConfigUpdate() {
    // for protocol update
    {
        // auto mManager = CreateManager();
        ObserverNetworkOption options;
        options.mEnableProtocols = {"http"};
        std::cout << magic_enum::enum_name(support_proto_e::ProtoHTTP) << std::endl;
        mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_TRUE(mManager->mPreviousOpt != nullptr);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableProtocols.size(), 1UL);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableProtocols[0], "http");
        // only http
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 1UL);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers.count(support_proto_e::ProtoHTTP) > 0);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers[support_proto_e::ProtoHTTP] != nullptr);

        options.mEnableProtocols = {"MySQL", "Redis", "Dubbo"};
        // std::vector<std::string> protocols = {"MySQL", "Redis", "Dubbo"};
        int result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 0UL);

        // protocols = {"HTTP", "MySQL"};
        options.mEnableProtocols = {"HTTP", "MySQL"};
        result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 1UL);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers.count(support_proto_e::ProtoHTTP) > 0);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers[support_proto_e::ProtoHTTP] != nullptr);

        // protocols.clear();
        options.mEnableProtocols = {};
        result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 0UL);
        mManager->Destroy();
    }

    // for enable log
    // for protocol update
    {
        ObserverNetworkOption options;
        options.mEnableProtocols = {"http"};
        options.mEnableLog = false;
        options.mEnableMetric = true;
        options.mEnableSpan = true;
        mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_TRUE(mManager->mPreviousOpt != nullptr);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableProtocols.size(), 1UL);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableProtocols[0], "http");
        // only http
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 1UL);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers.count(support_proto_e::ProtoHTTP) > 0);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers[support_proto_e::ProtoHTTP] != nullptr);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableLog, false);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableSpan, true);

        options.mEnableProtocols = {"MySQL", "Redis", "Dubbo"};
        options.mEnableLog = true;
        options.mEnableMetric = false;
        options.mEnableSpan = false;
        // std::vector<std::string> protocols = {"MySQL", "Redis", "Dubbo"};
        int result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 0UL);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableMetric, false);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableSpan, false);

        // protocols = {"HTTP", "MySQL"};
        options.mEnableProtocols = {"HTTP", "MySQL"};
        options.mEnableLog = true;
        options.mEnableMetric = true;
        options.mEnableSpan = false;
        result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 1UL);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers.count(support_proto_e::ProtoHTTP) > 0);
        APSARA_TEST_TRUE(ProtocolParserManager::GetInstance().mParsers[support_proto_e::ProtoHTTP] != nullptr);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableSpan, false);

        // protocols.clear();
        options.mEnableProtocols = {};
        result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(ProtocolParserManager::GetInstance().mParsers.size(), 0UL);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mPreviousOpt->mEnableSpan, false);
    }
}

void NetworkObserverManagerUnittest::TestPluginLifecycle() {
    // auto mManager = CreateManager();

    ObserverNetworkOption options;
    options.mEnableProtocols = {"HTTP"};
    int result = mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);

    // case1: udpate
    // suspend

    // update

    // destroy

    // case2: init and stop

    // case3: stop and re-run

    options.mEnableProtocols = {"HTTP", "MySQL"};
    result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);

    result = mManager->Destroy();
    EXPECT_EQ(result, 0);
}

std::shared_ptr<K8sPodInfo> CreatePodInfo(const std::string& cid) {
    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {cid};
    podInfo->mPodIp = "test-pod-ip";
    podInfo->mPodName = "test-pod-name";
    podInfo->mNamespace = "test-namespace";
    podInfo->mAppId = cid + "-test-app-id";
    podInfo->mAppName = cid + "-test-app-name";
    return podInfo;
}

void NetworkObserverManagerUnittest::TestHandleHostMetadataUpdate() {
    std::vector<std::string> cidLists0 = {"1", "2", "3", "4", "5"};
    for (auto cid : cidLists0) {
        K8sMetadata::GetInstance().mContainerCache.insert(cid, CreatePodInfo(cid));
    }
    mManager->HandleHostMetadataUpdate({"1", "2", "3", "4"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 4UL);
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 0UL);

    mManager->HandleHostMetadataUpdate({"2", "3", "4", "5"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 1UL); // only add "5"
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 1UL); // delete "1"

    mManager->HandleHostMetadataUpdate({"4", "5", "6"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 0UL);
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 2UL); // delete "2" "3"
}

void NetworkObserverManagerUnittest::TestPeriodicalTask() {
    // manager init, will execute
    mManager->mInited = true;
    Timer::GetInstance()->Clear();
    EBPFServer::GetInstance()->updatePluginState(PluginType::NETWORK_OBSERVE, "pipeline", "project", mManager);

    auto now = std::chrono::steady_clock::now();
    std::shared_ptr<ScheduleConfig> metricConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(15), JobType::METRIC_AGG);
    std::shared_ptr<ScheduleConfig> spanConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(2), JobType::SPAN_AGG);
    std::shared_ptr<ScheduleConfig> logConfig
        = std::make_shared<NetworkObserverScheduleConfig>(std::chrono::seconds(2), JobType::LOG_AGG);
    mManager->ScheduleNext(now, metricConfig);
    mManager->ScheduleNext(now, spanConfig);
    mManager->ScheduleNext(now, logConfig);
    APSARA_TEST_EQUAL(mManager->mExecTimes, 4);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 6);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 8);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 10);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 12);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 14);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    APSARA_TEST_EQUAL(mManager->mExecTimes, 16);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    // execute 2 metric task
    APSARA_TEST_EQUAL(mManager->mExecTimes, 20);
}

void NetworkObserverManagerUnittest::BenchmarkConsumeTask() {
}

UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestInitialization);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestEventHandling);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestDataEventProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestWhitelistManagement);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPerfBufferOperations);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestRecordProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestRollbackProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestConfigUpdate);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPluginLifecycle);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestHandleHostMetadataUpdate);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPeriodicalTask);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, BenchmarkConsumeTask);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
