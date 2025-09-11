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

#include "common/StringTools.h"
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
    void TestWhitelistManagement();
    void TestPerfBufferOperations();
    void TestRecordProcessing();
    void TestConfigUpdate();
    void TestErrorHandling();
    void TestPluginLifecycle();
    void TestHandleHostMetadataUpdate();
    void TestPeriodicalTask();
    void TestSaeScenario();
    void BenchmarkConsumeTask();
    void TestReportAgentInfo();
    void TestConverge();

protected:
    void SetUp() override {
        AsynCurlRunner::GetInstance()->Stop();
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mEBPFAdapter->Init();
        DynamicMetricLabels dynamicLabels;
        WriteMetrics::GetInstance()->CreateMetricsRecordRef(
            mRef,
            MetricCategory::METRIC_CATEGORY_RUNNER,
            {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
            std::move(dynamicLabels));
        auto pollProcessEventsTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
        auto lossProcessEventsTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
        auto processCacheMissTotal = mRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
        auto processCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
        auto processDataMapSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
        WriteMetrics::GetInstance()->CommitMetricsRecordRef(mRef);
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(mEBPFAdapter,
                                                                     "test_host",
                                                                     "/",
                                                                     mEventQueue,
                                                                     pollProcessEventsTotal,
                                                                     lossProcessEventsTotal,
                                                                     processCacheMissTotal,
                                                                     processCacheSize,
                                                                     processDataMapSize,
                                                                     mRetryableEventCache);
        ProtocolParserManager::GetInstance().AddParser(support_proto_e::ProtoHTTP);
        mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, &mEventPool);
        mManager->Init();
        EBPFServer::GetInstance()->updatePluginState(
            PluginType::NETWORK_OBSERVE, "pipeline", "project", PluginStateOperation::kAddPipeline, mManager);
    }

    void TearDown() override {
        AsynCurlRunner::GetInstance()->Stop();
        mManager->Destroy();
        EBPFServer::GetInstance()->updatePluginState(
            PluginType::NETWORK_OBSERVE, "", "", PluginStateOperation::kRemoveAll, nullptr);
        mRetryableEventCache.Clear();
    }

private:
    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, &mEventPool);
    }

    int HandleEvents() {
        std::array<std::shared_ptr<CommonEvent>, 4096> items;
        size_t count = mEventQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
        LOG_WARNING(sLogger, ("count", count));
        for (size_t i = 0; i < count; i++) {
            if (items[i] == nullptr) {
                LOG_WARNING(sLogger, ("event is null", ""));
                continue;
            }
            mManager->HandleEvent(items[i]);
        }
        return count;
    }

    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    MetricsRecordRef mRef;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    EventPool mEventPool = EventPool(true);
    std::shared_ptr<NetworkObserverManager> mManager;
    RetryableEventCache mRetryableEventCache;
};

void NetworkObserverManagerUnittest::TestInitialization() {
    // auto mManager = CreateManager();
    EXPECT_NE(mManager, nullptr);

    ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP", "MySQL", "Redis"};
    // options.mEnableCids = {"container1", "container2"};
    // options.mDisableCids = {"container3"};

    int result = mManager->Init();
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mManager->GetPluginType(), PluginType::NETWORK_OBSERVE);
}

void NetworkObserverManagerUnittest::TestEventHandling() {
    // auto mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue, nullptr);
    EXPECT_NE(mManager, nullptr);
    ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP"};
    mManager->Init();

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

void NetworkObserverManagerUnittest::TestWhitelistManagement() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    mManager->Init();

    std::vector<std::pair<std::string, uint64_t>> enableCids = {{"container1", 1}, {"container2", 2}};
    std::vector<std::string> disableCids;
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    enableCids.clear();
    disableCids = {{"container3", 3}, {"container4", 4}};
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));

    enableCids = {{"container5", 5}};
    disableCids = {{"container6", 6}};
    mManager->UpdateWhitelists(std::move(enableCids), std::move(disableCids));
}

void NetworkObserverManagerUnittest::TestPerfBufferOperations() {
    // auto mManager = CreateManager();
    ObserverNetworkOption options;
    // options.mEnableProtocols = {"HTTP"};
    mManager->Init();

    int result = mManager->PollPerfBuffer(kDefaultMaxWaitTimeMS);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 5; i++) {
        result = mManager->PollPerfBuffer(kDefaultMaxWaitTimeMS);
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
    options.mApmConfig.mLanguage = "php";

    options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

    mManager->Init();

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

    APSARA_TEST_EQUAL(HandleEvents(), 100);
    // verify
    // auto now = std::chrono::steady_clock::now();
    LOG_INFO(sLogger, ("====== consume span ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeSpanAggregateTree());
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups[0].GetEvents().size(), 100UL);
    auto tags = mManager->mSpanEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump span tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 11UL);
    APSARA_TEST_EQUAL(tags["service.name"], "test-app-name");
    APSARA_TEST_EQUAL(tags["arms.appId"], "test-app-id");
    APSARA_TEST_EQUAL(tags["host.ip"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host.name"], "test-pod-name");
    APSARA_TEST_EQUAL(tags["arms.app.type"], "apm");
    APSARA_TEST_EQUAL(tags["language"], "php");
    APSARA_TEST_EQUAL(tags["data_type"], "trace"); // used for route

    LOG_INFO(sLogger, ("====== consume metric ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeMetricAggregateTree());
    APSARA_TEST_EQUAL(mManager->mMetricEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mMetricEventGroups[0].GetEvents().size(),
                      301UL); // 100 record => 300 RED metric and 1 tag metric
    tags = mManager->mMetricEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump metric tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 10UL);
    APSARA_TEST_EQUAL(tags["service"], "test-app-name");
    APSARA_TEST_EQUAL(tags["pid"], "test-app-id");
    APSARA_TEST_EQUAL(tags["serverIp"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host"], "test-pod-name");
    APSARA_TEST_EQUAL(tags["source"], "apm");
    APSARA_TEST_EQUAL(tags["technology"], "ebpf");
    APSARA_TEST_EQUAL(tags["language"], "php");
    APSARA_TEST_EQUAL(tags["data_type"], "metric"); // used for route
    StringView prefix = "/index.html/";
    for (const auto& evt : mManager->mMetricEventGroups[0].GetEvents()) {
        APSARA_TEST_TRUE(evt.Is<MetricEvent>());
        auto* metricEvent = evt.Get<MetricEvent>();
        auto view = metricEvent->GetTag(kRpc.MetricKey());
        std::string rpc(view.data(), view.size());
        LOG_INFO(sLogger, ("rpc", rpc)("name", metricEvent->GetName()));
        if (metricEvent->GetName() == "arms_tag_entity") {
            continue;
        }
        APSARA_TEST_TRUE(StartWith(rpc, prefix)); // used for route
    }

    LOG_INFO(sLogger, ("====== consume log ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeLogAggregateTree());
    APSARA_TEST_EQUAL(mManager->mLogEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mLogEventGroups[0].GetEvents().size(), 100UL);
    tags = mManager->mLogEventGroups[0].GetTags();
    APSARA_TEST_EQUAL(tags.size(), 1UL);
}

size_t GenerateContainerIdHash(const std::string& cid) {
    std::hash<std::string> hasher;
    size_t key = 0;
    AttrHashCombine(key, hasher(cid));
    return key;
}

void NetworkObserverManagerUnittest::TestConfigUpdate() {
    auto mManager = CreateManager();
    mManager->Init();
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetCreateTime(12345);

    // 准备测试配置
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "test-workspace-1",
                          .mAppName = "test-app-name-1",
                          .mAppId = "test-app-id-1",
                          .mServiceId = "test-service-id-1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"test-workload-name-1", "test-workload-kind-1", "test-namespace-1"}};

    // 预先生成 workload keys
    size_t keys[5];
    for (int i = 0; i < 5; i++) {
        keys[i] = GenerateWorkloadKey("test-namespace-" + std::to_string(i),
                                      "test-workload-kind-" + std::to_string(i),
                                      "test-workload-name-" + std::to_string(i));
    }

    /******************** 测试用例1: 添加初始配置 ********************/
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t workload1Key = keys[1];

    // 详细验证配置添加
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.size(), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.count("test-config-1"), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].size(), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].count(workload1Key), 1);

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 1);
    APSARA_TEST_TRUE(mManager->mWorkloadConfigs.find(workload1Key) != mManager->mWorkloadConfigs.end());

    const auto& wlConfig = mManager->mWorkloadConfigs[workload1Key];
    APSARA_TEST_TRUE(wlConfig.config != nullptr);

    // 验证所有配置字段
    APSARA_TEST_EQUAL(wlConfig.config->mAppId, "test-app-id-1");
    APSARA_TEST_EQUAL(wlConfig.config->mAppName, "test-app-name-1");
    APSARA_TEST_EQUAL(wlConfig.config->mWorkspace, "test-workspace-1");
    APSARA_TEST_EQUAL(wlConfig.config->mServiceId, "test-service-id-1");
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL4, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL7, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableLog, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableSpan, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableMetric, true);
    APSARA_TEST_EQUAL(wlConfig.config->mSampleRate, 1.0);
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 0); // 初始无容器

    /******************** 准备容器数据 ********************/
    for (int i = 0; i <= 4; i++) {
        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mPodIp = "test-pod-ip-" + std::to_string(i);
        podInfo->mPodName = "test-pod-name-" + std::to_string(i);
        podInfo->mNamespace = "test-namespace-" + std::to_string(i);
        podInfo->mWorkloadKind = "test-workload-kind-" + std::to_string(i);
        podInfo->mWorkloadName = "test-workload-name-" + std::to_string(i);

        for (int j = 0; j < 3; j++) { // 每个pod 3个容器
            std::string cid = "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a446131" + std::to_string(i)
                + std::to_string(j);
            podInfo->mContainerIds.push_back(cid);
            K8sMetadata::GetInstance().mContainerCache.insert(cid, podInfo);
        }
    }

    /******************** 测试用例2: 添加容器关联 ********************/
    std::vector<std::string> cids = {
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613110", // workload1
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111" // workload1
    };
    mManager->HandleHostMetadataUpdate(cids);

    // 验证容器关联
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 2);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2);

    // 详细验证每个容器的配置字段
    for (const auto& cid : cids) {
        APSARA_TEST_EQUAL(wlConfig.containerIds.count(cid), 1);

        size_t cidKey = GenerateContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.find(cidKey) != mManager->mContainerConfigs.end());

        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_TRUE(appConfig != nullptr);

        // 验证所有字段
        APSARA_TEST_EQUAL(appConfig->mAppId, "test-app-id-1");
        APSARA_TEST_EQUAL(appConfig->mAppName, "test-app-name-1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "test-workspace-1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "test-service-id-1");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableLog, true);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, true);
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 1.0);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "test-config-1");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, context.GetProcessQueueKey());
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 0);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
        // APSARA_TEST_EQUAL(appConfig->mSampler->GetSampleRate(), 1.0);
    }

    /******************** 测试用例3: 添加新容器 ********************/
    std::vector<std::string> newCids = {
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112", // workload1
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111" // workload1 (已存在)
    };
    mManager->HandleHostMetadataUpdate(newCids);

    // 验证容器更新
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 2);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2);

    // 验证新容器配置
    size_t newCidKey = GenerateContainerKey("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.find(newCidKey) != mManager->mContainerConfigs.end());
    const auto& newContainerConfig = mManager->mContainerConfigs[newCidKey];
    APSARA_TEST_EQUAL(newContainerConfig->mAppId, "test-app-id-1");

    // 验证旧容器不再存在
    size_t removedCidKey = GenerateContainerKey("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613110");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.find(removedCidKey) == mManager->mContainerConfigs.end());

    /******************** 测试用例4: 移除容器 ********************/
    std::vector<std::string> removeCids = {
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613122", // workload2
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111" // workload1
    };
    mManager->HandleHostMetadataUpdate(removeCids);

    // 验证容器移除
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 1);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 1);

    // 验证保留的容器配置
    size_t keptCidKey = GenerateContainerKey("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613111");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.find(keptCidKey) != mManager->mContainerConfigs.end());
    const auto& keptContainerConfig = mManager->mContainerConfigs[keptCidKey];
    APSARA_TEST_EQUAL(keptContainerConfig->mAppName, "test-app-name-1");

    // 验证移除的容器不再存在
    size_t missingCidKey = GenerateContainerKey("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613112");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.find(missingCidKey) == mManager->mContainerConfigs.end());

    /******************** 测试用例5: 更新配置 ********************/
    options.mSelectors = {{"test-workload-name-2", "test-workload-kind-2", "test-namespace-2"},
                          {"test-workload-name-3", "test-workload-kind-3", "test-namespace-3"}};
    options.mL4Config.mEnable = false;
    options.mL7Config.mEnableLog = false;
    options.mL7Config.mSampleRate = 0.5; // 修改采样率

    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);

    // 验证配置更新
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].size(), 2);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].count(keys[2]), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].count(keys[3]), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].count(keys[1]), 0);

    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 2);

    // 验证workload2配置
    const auto& wlConfig2 = mManager->mWorkloadConfigs[keys[2]];
    APSARA_TEST_EQUAL(wlConfig2.config->mEnableL4, false);
    APSARA_TEST_EQUAL(wlConfig2.config->mEnableLog, false);
    APSARA_TEST_EQUAL(wlConfig2.config->mSampleRate, 0.5);
    APSARA_TEST_TRUE(wlConfig2.config->mSampler != nullptr);
    // APSARA_TEST_EQUAL(wlConfig2.config->mSampler->GetSampleRate(), 0.5);

    // 验证workload3配置
    const auto& wlConfig3 = mManager->mWorkloadConfigs[keys[3]];
    APSARA_TEST_EQUAL(wlConfig3.config->mServiceId, "test-service-id-1");

    // 验证旧容器配置已被清除
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(workload1Key), 0);
    // APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 0); // 确保旧workload的容器被清除

    /******************** 测试用例6: 添加新配置 ********************/
    CollectionPipelineContext context2;
    context2.SetConfigName("test-config-2");
    context2.SetProcessQueueKey(54321); // 设置不同的队列key

    ObserverNetworkOption options2;
    options2.mApmConfig = {
        .mWorkspace = "test-workspace-2",
        .mAppName = "test-app-name-2",
        .mAppId = "test-app-id-2",
        .mServiceId = "test-service-id-2",
    };
    options2.mL4Config.mEnable = true;
    options2.mL7Config
        = {.mEnable = true, .mEnableSpan = false, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 0.1};
    options2.mSelectors = {{"test-workload-name-4", "test-workload-kind-4", "test-namespace-4"}};

    mManager->AddOrUpdateConfig(&context2, 1, nullptr, &options2); // 使用不同的index

    // 验证新配置添加
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.size(), 2);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-2"].size(), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-2"].count(keys[4]), 1);

    // 验证新workload配置
    const auto& wlConfig4 = mManager->mWorkloadConfigs[keys[4]];
    APSARA_TEST_EQUAL(wlConfig4.config->mAppId, "test-app-id-2");
    APSARA_TEST_EQUAL(wlConfig4.config->mEnableSpan, false);
    APSARA_TEST_EQUAL(wlConfig4.config->mQueueKey, 54321);
    APSARA_TEST_EQUAL(wlConfig4.config->mPluginIndex, 1);

    /******************** 测试用例7: 删除配置 ********************/
    mManager->RemoveConfig("test-config-1");

    // 验证配置删除
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.size(), 1);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.count("test-config-1"), 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(keys[4]), 1);

    // 验证容器配置为空
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0);

    /******************** 测试用例8: 添加新容器到配置2 ********************/
    std::vector<std::string> workload4Cids = {
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613140", // workload4
        "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613141" // workload4
    };
    mManager->HandleHostMetadataUpdate(workload4Cids);

    // 验证新容器配置
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[keys[4]].containerIds.size(), 2);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2);

    for (const auto& cid : workload4Cids) {
        size_t cidKey = GenerateContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.find(cidKey) != mManager->mContainerConfigs.end());

        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_EQUAL(appConfig->mAppId, "test-app-id-2");
        APSARA_TEST_EQUAL(appConfig->mAppName, "test-app-name-2");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "test-workspace-2");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "test-service-id-2");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableLog, true);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, false); // 验证关闭span
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 0.1);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "test-config-2");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, 54321);
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 1);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
        // APSARA_TEST_EQUAL(appConfig->mSampler->GetSampleRate(), 0.1);
    }
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

    mManager->Init();

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

    mManager->Init();

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
    free(dataEvent);
}


void NetworkObserverManagerUnittest::BenchmarkConsumeTask() {
}

void NetworkObserverManagerUnittest::TestReportAgentInfo() {
    // 测试无配置时调用 ReportAgentInfo
    mManager->ReportAgentInfo();
    APSARA_TEST_EQUAL(mManager->mAgentInfoEventGroups.size(), 0UL);

    // 测试有全局配置时调用 ReportAgentInfo
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-config");
    ctx.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "test-workspace",
                          .mAppName = "test-app",
                          .mAppId = "test-app-id",
                          .mServiceId = "test-service-id"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &options);

    mManager->ReportAgentInfo();
    APSARA_TEST_EQUAL(mManager->mAgentInfoEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mAgentInfoEventGroups[0].GetEvents().size(), 1UL);
    APSARA_TEST_TRUE(mManager->mAgentInfoEventGroups[0].GetEvents()[0].Is<LogEvent>());
    auto* logEvent = mManager->mAgentInfoEventGroups[0].GetEvents()[0].Get<LogEvent>();
    APSARA_TEST_EQUAL(logEvent->GetContent("pid"), "test-app-id");
    APSARA_TEST_EQUAL(logEvent->GetContent("acs_cms_workspace"), "test-workspace");
    APSARA_TEST_EQUAL(logEvent->GetContent("acs_arms_service_id"), "test-service-id");
    APSARA_TEST_EQUAL(logEvent->GetContent("appName"), "test-app");
    APSARA_TEST_EQUAL(logEvent->GetContent("agentEnv"), "ACSK8S");
    APSARA_TEST_TRUE(logEvent->GetContent("timestamp") != "");
    APSARA_TEST_TRUE(logEvent->GetContent("startTimestamp") != "");

    // 测试有 workload 配置和 container 时调用 ReportAgentInfo
    options.mSelectors = {{"test-workload", "deployment", "test-namespace"}};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &options);

    // 添加 container 信息
    auto podInfo = std::make_shared<K8sPodInfo>();
    podInfo->mContainerIds = {"test-container-id"};
    podInfo->mPodIp = "192.168.1.100";
    podInfo->mPodName = "test-pod";
    podInfo->mNamespace = "test-namespace";
    podInfo->mWorkloadKind = "deployment";
    podInfo->mWorkloadName = "test-workload";
    podInfo->mStartTime = time(nullptr);
    K8sMetadata::GetInstance().mContainerCache.insert("test-container-id", podInfo);
    mManager->HandleHostMetadataUpdate({"test-container-id"});
    mManager->ReportAgentInfo();
    APSARA_TEST_EQUAL(mManager->mAgentInfoEventGroups.size(), 2UL);
    APSARA_TEST_EQUAL(mManager->mAgentInfoEventGroups[1].GetEvents().size(), 1UL);


    // 验证 workload 配置已正确设置
    size_t workloadKey = GenerateWorkloadKey("test-namespace", "deployment", "test-workload");
    APSARA_TEST_TRUE(mManager->mWorkloadConfigs.count(workloadKey) > 0);
    const auto& workloadConfig = mManager->mWorkloadConfigs[workloadKey];
    APSARA_TEST_EQUAL(workloadConfig.containerIds.count("test-container-id"), 1u);
    APSARA_TEST_TRUE(workloadConfig.config != nullptr);
    APSARA_TEST_EQUAL(workloadConfig.config->mAppId, "test-app-id");
    APSARA_TEST_EQUAL(workloadConfig.config->mAppName, "test-app");
    APSARA_TEST_EQUAL(workloadConfig.config->mWorkspace, "test-workspace");
    APSARA_TEST_EQUAL(workloadConfig.config->mServiceId, "test-service-id");
}

void NetworkObserverManagerUnittest::TestConverge() {
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
    options.mApmConfig.mLanguage = "php";

    options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

    mManager->Init();

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
    for (size_t aa = 0; aa < 20; aa++) {
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i + aa * 100);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }
        APSARA_TEST_EQUAL(HandleEvents(), 100);
    }

    StringView prefix = "/index.html/";


    LOG_INFO(sLogger, ("====== test span ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeSpanAggregateTree());
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups.size(), 1UL);
    APSARA_TEST_EQUAL(mManager->mSpanEventGroups[0].GetEvents().size(), 2000UL);
    auto tags = mManager->mSpanEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump span tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 11UL);
    APSARA_TEST_EQUAL(tags["service.name"], "test-app-name");
    APSARA_TEST_EQUAL(tags["arms.appId"], "test-app-id");
    APSARA_TEST_EQUAL(tags["host.ip"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host.name"], "test-pod-name");
    APSARA_TEST_EQUAL(tags["arms.app.type"], "apm");
    APSARA_TEST_EQUAL(tags["language"], "php");
    APSARA_TEST_EQUAL(tags["data_type"], "trace"); // used for route
    int spanCnt = 0;
    for (const auto& evt : mManager->mSpanEventGroups[0].GetEvents()) {
        APSARA_TEST_TRUE(evt.Is<SpanEvent>());
        auto* spanEvent = evt.Get<SpanEvent>();
        auto rpcView = spanEvent->GetTag(kRpc.SpanKey());
        std::string rpc(rpcView.data(), rpcView.size());
        auto spanNameView = spanEvent->GetName();
        std::string spanName(spanNameView.data(), spanNameView.size());
        APSARA_TEST_TRUE(StartWith(spanName, prefix));
        if (!StartWith(rpc, prefix)) {
            spanCnt++;
            APSARA_TEST_EQUAL(rpc, "{DEFAULT}");
        }
    }
    APSARA_TEST_TRUE(spanCnt > 0);

    LOG_INFO(sLogger, ("====== test metric ======", ""));
    APSARA_TEST_TRUE(mManager->ConsumeMetricAggregateTree());
    APSARA_TEST_EQUAL(mManager->mMetricEventGroups.size(), 1UL);
    tags = mManager->mMetricEventGroups[0].GetTags();
    for (const auto& tag : tags) {
        LOG_INFO(sLogger, ("dump metric tags", "")(std::string(tag.first), std::string(tag.second)));
    }
    APSARA_TEST_EQUAL(tags.size(), 10UL);
    APSARA_TEST_EQUAL(tags["service"], "test-app-name");
    APSARA_TEST_EQUAL(tags["pid"], "test-app-id");
    APSARA_TEST_EQUAL(tags["serverIp"], "127.0.0.1");
    APSARA_TEST_EQUAL(tags["host"], "test-pod-name");
    APSARA_TEST_EQUAL(tags["source"], "apm");
    APSARA_TEST_EQUAL(tags["technology"], "ebpf");
    APSARA_TEST_EQUAL(tags["language"], "php");
    APSARA_TEST_EQUAL(tags["data_type"], "metric"); // used for route

    int cnt = 0;
    for (const auto& evt : mManager->mMetricEventGroups[0].GetEvents()) {
        APSARA_TEST_TRUE(evt.Is<MetricEvent>());
        auto* metricEvent = evt.Get<MetricEvent>();
        auto view = metricEvent->GetTag(kRpc.MetricKey());
        std::string rpc(view.data(), view.size());
        LOG_INFO(sLogger, ("rpc", rpc)("name", metricEvent->GetName()));
        if (metricEvent->GetName() == "arms_tag_entity") {
            continue;
        }
        if (!StartWith(rpc, prefix)) {
            APSARA_TEST_EQUAL(rpc, "{DEFAULT}");
            cnt++;
        }
    }
    APSARA_TEST_TRUE(cnt > 0);
}

UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestInitialization);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestEventHandling);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestWhitelistManagement);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestPerfBufferOperations);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestRecordProcessing);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestConfigUpdate);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestHandleHostMetadataUpdate);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestSaeScenario);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, BenchmarkConsumeTask);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestReportAgentInfo);
UNIT_TEST_CASE(NetworkObserverManagerUnittest, TestConverge);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
