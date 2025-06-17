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
#include <iostream>
#include <memory>
#include <thread>

#include "StringTools.h"
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
#include "plugin/network_observer/Connection.h"
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
    void TestSaeScenario();
    void TestPeriodicalTask();
    void BenchmarkConsumeTask();

protected:
    void SetUp() override {
        Timer::GetInstance()->Init();
        AsynCurlRunner::GetInstance()->Stop();
        mThreadPool = std::make_unique<ThreadPool>(1);
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mEBPFAdapter->Init();
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(
            mEBPFAdapter, "test_host", "/", mEventQueue, nullptr, nullptr, nullptr, nullptr);
        ProtocolParserManager::GetInstance().AddParser(support_proto_e::ProtoHTTP);
        mManager
            = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mThreadPool, mEventQueue, nullptr);
        EBPFServer::GetInstance()->UpdatePluginManager(PluginType::NETWORK_OBSERVE, mManager);
    }

    void TearDown() override {
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
        mManager->Destroy();
        EBPFServer::GetInstance()->UpdatePluginManager(PluginType::NETWORK_OBSERVE, nullptr);
    }

private:
    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mThreadPool, mEventQueue, nullptr);
    }

    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    std::unique_ptr<ThreadPool> mThreadPool;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    std::shared_ptr<NetworkObserverManager> mManager;
};

void NetworkObserverManagerUnittest::TestInitialization() {
    // auto mManager = CreateManager();
    EXPECT_NE(mManager, nullptr);

    ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP", "MySQL", "Redis"};
    // options.mEnableCids = {"container1", "container2"};
    // options.mDisableCids = {"container3"};

    int result = mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mManager->GetPluginType(), PluginType::NETWORK_OBSERVE);
}

void NetworkObserverManagerUnittest::TestEventHandling() {
    // auto mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, nullptr);
    EXPECT_NE(mManager, nullptr);
    ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP"};
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
    // ObserverNetworkOption options;
    // mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    // mManager->Destroy();

    // auto statsEvent = CreateConnStatsEvent();
    // mManager->AcceptNetStatsEvent(&statsEvent);

    // auto* dataEvent = CreateHttpDataEvent();
    // // TODO @qianlu.kk
    // mManager->AcceptDataEvent(dataEvent);
    // free(dataEvent);

    // std::vector<std::shared_ptr<AbstractRecord>> items(10, nullptr);
    // size_t count = mManager->mRollbackQueue.wait_dequeue_bulk_timed(items.data(), 1024,
    // std::chrono::milliseconds(200)); APSARA_TEST_EQUAL(count, 1UL); APSARA_TEST_TRUE(items[0] != nullptr);

    // AbstractAppRecord* record = static_cast<AbstractAppRecord*>(items[0].get());
    // APSARA_TEST_TRUE(record != nullptr);
    // auto conn = record->GetConnection();
    // APSARA_TEST_TRUE(conn != nullptr);

    // APSARA_TEST_TRUE(mManager->mConnectionManager->getConnection(conn->GetConnId()) != nullptr);

    // // destroy connection
    // conn->MarkClose();
    // for (size_t i = 0; i < 12; i++) {
    //     mManager->mConnectionManager->Iterations();
    // }

    // // connection that record holds still available
    // APSARA_TEST_TRUE(mManager->mConnectionManager->getConnection(conn->GetConnId()) == nullptr);

    // // verify attributes
    // HttpRecord* httpRecord = static_cast<HttpRecord*>(record);
    // // http attrs
    // APSARA_TEST_EQUAL(httpRecord->GetPath(), "/index.html");
    // APSARA_TEST_EQUAL(httpRecord->GetSpanName(), "/index.html");
    // APSARA_TEST_EQUAL(httpRecord->GetStatusCode(), 200);
    // APSARA_TEST_EQUAL(httpRecord->GetStartTimeStamp(), 1UL);
    // APSARA_TEST_EQUAL(httpRecord->GetEndTimeStamp(), 2UL);

    // auto& attrs = httpRecord->GetConnection()->GetConnTrackerAttrs();
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kLocalAddr.Name())], "127.0.0.1:8080");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRemoteAddr.Name())], "192.168.1.1:80");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kRpcType.Name())], "25");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallKind.Name())], "http_client");
    // APSARA_TEST_EQUAL(attrs[kConnTrackerTable.ColIndex(kCallType.Name())], "http_client");
}

void NetworkObserverManagerUnittest::TestWhitelistManagement() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
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
    // options.mEnableProtocols = {"HTTP"};
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
    options.mL7Config.mEnable = true;
    options.mL7Config.mEnableLog = true;
    options.mL7Config.mEnableMetric = true;
    options.mL7Config.mEnableSpan = true;
    options.mL7Config.mSampleRate = 1.0;

    options.mApmConfig.mAppId = "test-app-id";
    options.mApmConfig.mAppName = "test-app-name";
    options.mApmConfig.mWorkspace = "test-workspace";
    options.mApmConfig.mServiceId = "test-service-id";

    options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-config-networkobserver");
    ctx.SetProcessQueueKey(1);
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // options.mEnableProtocols = {"HTTP"};
    // options.mEnableLog = true;
    // options.mEnableMetric = true;
    // options.mEnableSpan = true;
    // options.mSampleRate = 1;
    // mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {"1", "2"};
    // podInfo->mAppName = "test-app-name";
    // podInfo->mAppId = "test-app-id";
    podInfo->mPodIp = "test-pod-ip";
    podInfo->mPodName = "test-pod-name";
    podInfo->mNamespace = "test-namespace";
    podInfo->mWorkloadKind = "Deployment";
    podInfo->mWorkloadName = "test-workloadname";

    LOG_INFO(sLogger, ("step", "0-0"));
    K8sMetadata::GetInstance().mContainerCache.insert(
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106", podInfo);

    mManager->HandleHostMetadataUpdate({"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106"});

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

    // copy current
    mManager->mContainerConfigsReplica = mManager->mContainerConfigs;

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
    APSARA_TEST_EQUAL(tags.size(), 9UL);
    APSARA_TEST_EQUAL(tags["service.name"], "test-app-name");
    APSARA_TEST_EQUAL(tags["arms.appId"], "test-app-id");
    APSARA_TEST_EQUAL(tags["host.ip"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host.name"], "test-pod-name");
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
    APSARA_TEST_EQUAL(tags.size(), 8UL);
    APSARA_TEST_EQUAL(tags["service"], "test-app-name");
    APSARA_TEST_EQUAL(tags["pid"], "test-app-id");
    APSARA_TEST_EQUAL(tags["serverIp"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host"], "test-pod-name");
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
    {
        ObserverNetworkOption options;
        options.mL7Config.mEnable = true;
        options.mL7Config.mEnableLog = true;
        options.mL7Config.mEnableMetric = true;
        options.mL7Config.mEnableSpan = true;

        options.mApmConfig.mAppId = "test-app-id";
        options.mApmConfig.mAppName = "test-app-name";
        options.mApmConfig.mWorkspace = "test-workspace";
        options.mApmConfig.mServiceId = "test-service-id";

        options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

        mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
        CollectionPipelineContext ctx;
        ctx.SetConfigName("test-config-networkobserver");
        ctx.SetProcessQueueKey(1);
        mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mContainerIds = {"1", "2"};
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

        // copy current
        mManager->mContainerConfigsReplica = mManager->mContainerConfigs;

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
}

size_t GenerateContainerIdHash(const std::string& cid) {
    std::hash<std::string> hasher;
    size_t key = 0;
    AttrHashCombine(key, hasher(cid));
    return key;
}

// TODO @qianlu.kk
void NetworkObserverManagerUnittest::TestConfigUpdate() {
    auto mManager = CreateManager();
    std::cout << "begin" << std::endl;
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetCreateTime(12345);
    ObserverNetworkOption options;
    options.mApmConfig.mAppId = "test-app-id-1";
    options.mApmConfig.mAppName = "test-app-name-1";
    options.mApmConfig.mServiceId = "test-service-id-1";
    options.mApmConfig.mWorkspace = "test-workspace-1";

    options.mL4Config.mEnable = true;

    options.mL7Config.mEnable = true;
    options.mL7Config.mEnableLog = true;
    options.mL7Config.mEnableMetric = true;
    options.mL7Config.mEnableSpan = true;
    options.mL7Config.mSampleRate = 1.0;

    options.mSelectors = {{"test-workload-name-1", "test-workload-kind-1", "test-namespace-1"}};

    size_t workload0Key = GenerateWorkloadKey("test-namespace-0", "test-workload-kind-0", "test-workload-name-0");
    size_t workload2Key = GenerateWorkloadKey("test-namespace-2", "test-workload-kind-2", "test-workload-name-2");
    size_t workload3Key = GenerateWorkloadKey("test-namespace-3", "test-workload-kind-3", "test-workload-name-3");
    size_t workload4Key = GenerateWorkloadKey("test-namespace-4", "test-workload-kind-4", "test-workload-name-4");

    // case1. add config for test-config-1 which select workload1
    mManager->AddOrUpdateConfig(&context, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    size_t workload1Key = GenerateWorkloadKey("test-namespace-1", "test-workload-kind-1", "test-workload-name-1");
    // step1. validate every member
    APSARA_TEST_EQUAL(mManager->mWorkloads.size(), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads.count("test-config-1"), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].size(), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].count(workload1Key), 1); // "test-config-1"

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload1Key), 1);

    APSARA_TEST_TRUE(mManager->mWorkloadConfigs[workload1Key] != nullptr);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mAppId, "test-app-id-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mAppName, "test-app-name-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mWorkspace, "test-workspace-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mServiceId, "test-service-id-1");

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mEnableL4, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mEnableL7, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mEnableMetric, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mEnableSpan, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mEnableLog, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload1Key]->mSampleRate, 1.0);

    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 0);

    // step2. mock host metadata update ...
    {
        for (int i = 0; i <= 9; i++) {
            auto podInfo = std::make_shared<K8sPodInfo>();
            podInfo->mPodIp = "test-pod-ip-" + std::to_string(i);
            podInfo->mPodName = "test-pod-name-" + std::to_string(i);
            podInfo->mNamespace = "test-namespace-" + std::to_string(i);
            podInfo->mWorkloadKind = "test-workload-kind-" + std::to_string(i);
            podInfo->mWorkloadName = "test-workload-name-" + std::to_string(i);

            LOG_INFO(sLogger, ("step", "0-0"));
            for (int j = 0; j < 5; j++) {
                std::string containerId = "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a446131"
                    + std::to_string(i) + std::to_string(j);
                podInfo->mContainerIds.push_back(containerId);
                K8sMetadata::GetInstance().mContainerCache.insert(containerId, podInfo);
            }
        }
    }

    mManager->HandleHostMetadataUpdate(
        {"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613110",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"}); // "test-workload-name-1",
                                                                               // "test-workload-kind-1",
                                                                               // "test-namespace-1"
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2); // 2 containers
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;

    for (int i = 0; i < 2; i++) {
        std::string cid = "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a4461311" + std::to_string(i);
        size_t key = GenerateContainerIdHash(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs[key] != nullptr);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppId, "test-app-id-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppName, "test-app-name-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mWorkspace, "test-workspace-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mServiceId, "test-service-id-1");

        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL4, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL7, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableSpan, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mSampleRate, 1.0);
    }

    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.count(workload1Key), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].size(), 2); // 2 containers
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613110"),
                      1);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"),
                      1);

    // add pod
    mManager->HandleHostMetadataUpdate(
        {"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"}); // "test-workload-name-1",
                                                                               // "test-workload-kind-1",
                                                                               // "test-namespace-1"
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2); // 2 containers
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;
    for (int i = 1; i <= 2; i++) {
        std::string cid = "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a4461311" + std::to_string(i);
        size_t key = GenerateContainerIdHash(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs[key] != nullptr);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppId, "test-app-id-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppName, "test-app-name-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mWorkspace, "test-workspace-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mServiceId, "test-service-id-1");

        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL4, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL7, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableSpan, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mSampleRate, 1.0);
    }

    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.count(workload1Key), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].size(), 2); // 2 containers
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112"),
                      1);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"),
                      1);

    // remove 1 pod
    mManager->HandleHostMetadataUpdate(
        {"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613122",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"}); // "test-workload-name-1",
                                                                               // "test-workload-kind-1",
                                                                               // "test-namespace-1"
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 1); // 1 containers
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;
    for (int i = 1; i <= 1; i++) {
        std::string cid = "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a4461311" + std::to_string(i);
        size_t key = GenerateContainerIdHash(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs[key] != nullptr);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppId, "test-app-id-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mAppName, "test-app-name-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mWorkspace, "test-workspace-1");
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mServiceId, "test-service-id-1");

        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL4, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableL7, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableMetric, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableSpan, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mEnableLog, true);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs[key]->mSampleRate, 1.0);
    }

    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.count(workload1Key), 1); // 1 workload
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].size(), 1); // 2 containers
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112"),
                      0);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers[workload1Key].count(
                          "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"),
                      1);

    // ========
    // case2. update config1 and select both workload2 and workload3
    options.mSelectors = {{"test-workload-name-2", "test-workload-kind-2", "test-namespace-2"},
                          {"test-workload-name-3", "test-workload-kind-3", "test-namespace-3"}};
    options.mL4Config.mEnable = false;
    options.mL7Config.mEnableLog = false;
    mManager->AddOrUpdateConfig(&context, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // step1. validate every member
    APSARA_TEST_EQUAL(mManager->mWorkloads.size(), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloads.count("test-config-1"), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].size(), 2);
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].count(workload2Key), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].count(workload3Key), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].count(workload1Key), 0); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-1"].count(workload0Key), 0); // "test-config-1"

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 2);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload1Key), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload2Key), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload3Key), 1);

    APSARA_TEST_TRUE(mManager->mWorkloadConfigs[workload2Key] != nullptr);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mAppId, "test-app-id-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mAppName, "test-app-name-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mWorkspace, "test-workspace-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mServiceId, "test-service-id-1");

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mEnableL4, false);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mEnableL7, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mEnableMetric, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mEnableSpan, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mEnableLog, false);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload2Key]->mSampleRate, 1.0);

    APSARA_TEST_TRUE(mManager->mWorkloadConfigs[workload3Key] != nullptr);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mAppId, "test-app-id-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mAppName, "test-app-name-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mWorkspace, "test-workspace-1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mServiceId, "test-service-id-1");

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mEnableL4, false);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mEnableL7, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mEnableMetric, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mEnableSpan, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mEnableLog, false);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload3Key]->mSampleRate, 1.0);

    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0); // will clean when hostMetaUpdate is called ...
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 0);

    mManager->HandleHostMetadataUpdate(
        {"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613102",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111"}); // workload0 workload1
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0);
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 0);

    mManager->HandleHostMetadataUpdate(
        {"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613102",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613122",
         "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613123"}); // workload2
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2);
    for (const auto& it : mManager->mContainerConfigs) {
        std::cout << it.first << std::endl;
    }
    std::cout << "===" << std::endl;
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 1);


    // case3. add config2 which select workload2
    CollectionPipelineContext context2;
    context2.SetConfigName("test-config-2");
    context2.SetCreateTime(12345);
    ObserverNetworkOption options2;
    options2.mApmConfig.mAppId = "test-app-id-2";
    options2.mApmConfig.mAppName = "test-app-name-2";
    options2.mApmConfig.mServiceId = "test-service-id-2";
    options2.mApmConfig.mWorkspace = "test-workspace-2";

    options2.mL4Config.mEnable = true;

    options2.mL7Config.mEnable = true;
    options2.mL7Config.mEnableLog = true;
    options2.mL7Config.mEnableMetric = true;
    options2.mL7Config.mEnableSpan = true;
    options2.mL7Config.mSampleRate = 1.0;

    options2.mSelectors = {{"test-workload-name-4", "test-workload-kind-4", "test-namespace-4"}};
    mManager->AddOrUpdateConfig(
        &context2, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options2));
    APSARA_TEST_EQUAL(mManager->mWorkloads.size(), 2); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads.count("test-config-2"), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-2"].size(), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-2"].count(workload4Key), 1); // "test-config-1"

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 3);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload4Key), 1);

    APSARA_TEST_TRUE(mManager->mWorkloadConfigs[workload4Key] != nullptr);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mAppId, "test-app-id-2");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mAppName, "test-app-name-2");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mWorkspace, "test-workspace-2");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mServiceId, "test-service-id-2");

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mEnableL4, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mEnableL7, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mEnableMetric, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mEnableSpan, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mEnableLog, true);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[workload4Key]->mSampleRate, 1.0);

    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 1);

    // case4. remove config1
    mManager->RemoveConfig("test-config-1");
    APSARA_TEST_EQUAL(mManager->mWorkloads.size(), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads.count("test-config-2"), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-2"].size(), 1); // "test-config-1"
    APSARA_TEST_EQUAL(mManager->mWorkloads["test-config-2"].count(workload4Key), 1); // "test-config-1"

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload4Key), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload2Key), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload3Key), 0);

    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadContainers.size(), 0);


    std::cout << "end" << std::endl;
}

// TODO @qianlu.kk
void NetworkObserverManagerUnittest::TestPluginLifecycle() {
    // auto mManager = CreateManager();

    // ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP"};
    // int result = mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    // EXPECT_EQ(result, 0);

    // case1: udpate
    // suspend

    // update

    // destroy

    // case2: init and stop

    // case3: stop and re-run

    // options.mEnableProtocols = {"HTTP", "MySQL"};
    // result = mManager->Update(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    // EXPECT_EQ(result, 0);

    // result = mManager->Destroy();
    // EXPECT_EQ(result, 0);
}

std::shared_ptr<K8sPodInfo> CreatePodInfo(const std::string& cid) {
    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {cid};
    podInfo->mPodIp = "test-pod-ip";
    podInfo->mPodName = "test-pod-name";
    podInfo->mNamespace = "test-namespace";
    podInfo->mWorkloadKind = "Deployment";
    podInfo->mWorkloadName = "test-workloadname";
    podInfo->mAppId = cid + "-test-app-id";
    podInfo->mAppName = cid + "-test-app-name";
    return podInfo;
}

void NetworkObserverManagerUnittest::TestHandleHostMetadataUpdate() {
    std::vector<std::string> cidLists0 = {"1", "2", "3", "4", "5"};
    for (auto cid : cidLists0) {
        K8sMetadata::GetInstance().mContainerCache.insert(cid, CreatePodInfo(cid));
    }

    ObserverNetworkOption options;
    options.mL7Config.mEnable = true;
    options.mL7Config.mEnableLog = true;
    options.mL7Config.mEnableMetric = true;
    options.mL7Config.mEnableSpan = true;
    options.mL7Config.mSampleRate = 1.0;

    options.mApmConfig.mAppId = "test-app-id";
    options.mApmConfig.mAppName = "test-app-name";
    options.mApmConfig.mWorkspace = "test-workspace";
    options.mApmConfig.mServiceId = "test-service-id";

    options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-config-networkobserver");
    ctx.SetProcessQueueKey(1);
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    mManager->HandleHostMetadataUpdate({"1", "2", "3", "4"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 4);
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 0);

    mManager->HandleHostMetadataUpdate({"2", "3", "4", "5"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 1); // only add "5"
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 1); // delete "1"

    mManager->HandleHostMetadataUpdate({"4", "5", "6"});
    APSARA_TEST_EQUAL(mManager->mEnableCids.size(), 0);
    APSARA_TEST_EQUAL(mManager->mDisableCids.size(), 2); // delete "2" "3"
}

void NetworkObserverManagerUnittest::TestSaeScenario() {
    K8sMetadata::GetInstance().mEnable = false;

    ObserverNetworkOption options;
    options.mL7Config.mEnable = true;
    options.mL7Config.mEnableLog = true;
    options.mL7Config.mEnableMetric = true;
    options.mL7Config.mEnableSpan = true;
    options.mL7Config.mSampleRate = 1.0;

    options.mApmConfig.mAppId = "test-app-id";
    options.mApmConfig.mAppName = "test-app-name";
    options.mApmConfig.mWorkspace = "test-workspace";
    options.mApmConfig.mServiceId = "test-service-id";

    mManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-config-networkobserver");
    ctx.SetProcessQueueKey(1);
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // only 0
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 1);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(0), 1);

    // copy current
    mManager->mContainerConfigsReplica = mManager->mContainerConfigs;

    auto* dataEvent = CreateHttpDataEvent(1);
    const auto conn = mManager->mConnectionManager->AcceptNetDataEvent(dataEvent);
    const auto& appInfo = mManager->getAppConfigFromReplica(conn);
    APSARA_TEST_TRUE(appInfo != nullptr);
    APSARA_TEST_EQUAL(appInfo->mAppId, "test-app-id");
    APSARA_TEST_EQUAL(appInfo->mServiceId, "test-service-id");
    APSARA_TEST_EQUAL(appInfo->mAppName, "test-app-name");
    APSARA_TEST_EQUAL(appInfo->mWorkspace, "test-workspace");

    K8sMetadata::GetInstance().mEnable = true;
}

void NetworkObserverManagerUnittest::TestPeriodicalTask() {
    // manager init, will execute
    mManager->mFlag = true;
    Timer::GetInstance()->Clear();
    mThreadPool->Start();
    EBPFServer::GetInstance()->UpdatePluginManager(PluginType::NETWORK_OBSERVE, mManager);

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
    // std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // APSARA_TEST_EQUAL(mManager->mExecTimes, 4);
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
    mThreadPool->Stop();
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
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestSaeScenario);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPeriodicalTask);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, BenchmarkConsumeTask);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
