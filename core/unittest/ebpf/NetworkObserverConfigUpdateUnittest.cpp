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
#include <string>
#include <unordered_set>
#include <vector>

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

namespace logtail::ebpf {

class NetworkObserverConfigUpdateUnittest : public ::testing::Test {
public:
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
        auto retryableEventCacheSize = mRef.CreateIntGauge(METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE);
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
        mManager = NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue);
        EBPFServer::GetInstance()->updatePluginState(
            PluginType::NETWORK_OBSERVE, "pipeline", "project", PluginStateOperation::kAddPipeline, mManager);
    }
    void TearDown() override {
        AsynCurlRunner::GetInstance()->Stop();
        mManager->Destroy();
        EBPFServer::GetInstance()->updatePluginState(
            PluginType::NETWORK_OBSERVE, "", "", PluginStateOperation::kRemoveAll, nullptr);
    }
    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(mProcessCacheManager, mEBPFAdapter, mEventQueue);
    }
    // 生成 workload key
    size_t GenWorkloadKey(const std::string& ns, const std::string& kind, const std::string& name) {
        return GenerateWorkloadKey(ns, kind, name);
    }
    // 生成 container key
    size_t GenContainerKey(const std::string& cid) { return GenerateContainerKey(cid); }
    // 生成 podInfo 并写入 K8sMetadata cache
    void AddPodInfo(const std::string& ns,
                    const std::string& kind,
                    const std::string& name,
                    const std::vector<std::string>& cids) {
        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mPodIp = ns + "-ip";
        podInfo->mPodName = name + "-pod";
        podInfo->mNamespace = ns;
        podInfo->mWorkloadKind = kind;
        podInfo->mWorkloadName = name;
        for (const auto& cid : cids) {
            podInfo->mContainerIds.push_back(cid);
            K8sMetadata::GetInstance().mContainerCache.insert(cid, podInfo);
        }
    }
    // 1. config 增
    void AddConfig();
    // 2. config 改
    void UpdateConfig();
    // 3. config 删
    void RemoveConfig();
    // 4. container 增
    void AddContainer();
    // 5. container 删
    void RemoveContainer();
    // 6. config/容器正交性
    void ConfigAndContainerOrthogonal();
    // 7. MultiConfigContainerIndependence：多个 config/workload/container 交叉，删除一个 config 只影响其下 container。
    void MultiConfigContainerIndependence();
    // 8. ContainerMigrationBetweenConfigs：container 先后被不同 config/workload 关联，归属和字段正确。
    void ContainerMigrationBetweenConfigs();
    // 9. SelectorChangeAffectsContainer：config selector 变更导致 container 归属变化，字段和归属正确。
    void SelectorChangeAffectsContainer();
    // 10. IdempotentAddRemove：重复 add/remove config/container，最终状态一致。
    void IdempotentAddRemove();
    // 11. EmptySelectorAndContainer：空 selector/空 container/空 config 情况，系统状态合理。
    void EmptySelectorAndContainer();
    // 12. PartialFieldUpdate：config 更新时只变更部分字段，container 反映最新字段。
    void PartialFieldUpdate();
    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    MetricsRecordRef mRef;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    std::shared_ptr<NetworkObserverManager> mManager;
    RetryableEventCache mRetryableEventCache;
};

void NetworkObserverConfigUpdateUnittest::AddConfig() {
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.size(), 1u);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["test-config-1"].count(key), 1u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key), 1u);
    const auto& wlConfig = mManager->mWorkloadConfigs[key];
    APSARA_TEST_TRUE(wlConfig.config != nullptr);
    APSARA_TEST_EQUAL(wlConfig.config->mAppId, "id1");
    APSARA_TEST_EQUAL(wlConfig.config->mAppName, "app1");
    APSARA_TEST_EQUAL(wlConfig.config->mWorkspace, "w1");
    APSARA_TEST_EQUAL(wlConfig.config->mServiceId, "sid1");
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL4, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL7, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableLog, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableSpan, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableMetric, true);
    APSARA_TEST_EQUAL(wlConfig.config->mSampleRate, 1.0);
    APSARA_TEST_EQUAL(wlConfig.config->mConfigName, "test-config-1");
    APSARA_TEST_EQUAL(wlConfig.config->mQueueKey, context.GetProcessQueueKey());
    APSARA_TEST_EQUAL(wlConfig.config->mPluginIndex, 0);
    APSARA_TEST_TRUE(wlConfig.config->mSampler != nullptr);
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 0u);
}
void NetworkObserverConfigUpdateUnittest::UpdateConfig() {
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    options.mApmConfig.mAppName = "app1-mod";
    options.mL7Config.mEnableLog = false;
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    const auto& wlConfig = mManager->mWorkloadConfigs[key];
    APSARA_TEST_EQUAL(wlConfig.config->mAppName, "app1-mod");
    APSARA_TEST_EQUAL(wlConfig.config->mEnableLog, false);
    APSARA_TEST_EQUAL(wlConfig.config->mAppId, "id1");
    APSARA_TEST_EQUAL(wlConfig.config->mWorkspace, "w1");
    APSARA_TEST_EQUAL(wlConfig.config->mServiceId, "sid1");
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL4, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableL7, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableSpan, true);
    APSARA_TEST_EQUAL(wlConfig.config->mEnableMetric, true);
    APSARA_TEST_EQUAL(wlConfig.config->mSampleRate, 1.0);
    APSARA_TEST_EQUAL(wlConfig.config->mConfigName, "test-config-1");
    APSARA_TEST_EQUAL(wlConfig.config->mQueueKey, context.GetProcessQueueKey());
    APSARA_TEST_EQUAL(wlConfig.config->mPluginIndex, 0);
    APSARA_TEST_TRUE(wlConfig.config->mSampler != nullptr);
}
void NetworkObserverConfigUpdateUnittest::RemoveConfig() {
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    // 验证 container 已经被正确关联
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cidKey));
    }
    // 移除 config
    mManager->RemoveConfig("test-config-1");
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.count("test-config-1"), 0u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key), 0u);
    // 验证 container 也被清理
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cidKey), 0u);
    }
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0u);
}
void NetworkObserverConfigUpdateUnittest::AddContainer() {
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    const auto& wlConfig = mManager->mWorkloadConfigs[key];
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 2u);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 2u);
    for (const auto& cid : cids) {
        APSARA_TEST_EQUAL(wlConfig.containerIds.count(cid), 1u);
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.find(cidKey) != mManager->mContainerConfigs.end());
        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_TRUE(appConfig != nullptr);
        APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
        APSARA_TEST_EQUAL(appConfig->mAppName, "app1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
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
    }
    // 正交性验证：变更 config 后，container 绑定的 config 字段应同步变化
    options.mApmConfig.mAppName = "app1-new";
    options.mL7Config.mEnableLog = false;
    options.mL7Config.mSampleRate = 0.5;
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.find(cidKey) != mManager->mContainerConfigs.end());
        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_TRUE(appConfig != nullptr);
        APSARA_TEST_EQUAL(appConfig->mAppName, "app1-new");
        APSARA_TEST_EQUAL(appConfig->mEnableLog, false);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 0.5);
        // 其它字段也应保持一致
        APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, true);
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "test-config-1");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, context.GetProcessQueueKey());
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 0);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
    }
}
void NetworkObserverConfigUpdateUnittest::RemoveContainer() {
    CollectionPipelineContext context;
    context.SetConfigName("test-config-1");
    context.SetProcessQueueKey(1);
    ObserverNetworkOption options;
    options.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    options.mL4Config.mEnable = true;
    options.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    options.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&context, 0, nullptr, &options);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    std::vector<std::string> left = {"cid2"};
    mManager->HandleHostMetadataUpdate(left);
    const auto& wlConfig = mManager->mWorkloadConfigs[key];
    APSARA_TEST_EQUAL(wlConfig.containerIds.size(), 1u);
    size_t cid1Key = GenContainerKey("cid1");
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cid1Key), 0u);
    size_t cid2Key = GenContainerKey("cid2");
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cid2Key), 1u);
    const auto& appConfig = mManager->mContainerConfigs[cid2Key];
    APSARA_TEST_TRUE(appConfig != nullptr);
    APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
    APSARA_TEST_EQUAL(appConfig->mAppName, "app1");
    APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
    APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
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
}
void NetworkObserverConfigUpdateUnittest::ConfigAndContainerOrthogonal() {
    // 添加 config1
    CollectionPipelineContext ctx1;
    ctx1.SetConfigName("config1");
    ctx1.SetProcessQueueKey(1);
    ObserverNetworkOption opt1;
    opt1.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt1.mL4Config.mEnable = true;
    opt1.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt1.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&ctx1, 0, nullptr, &opt1);
    size_t key1 = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids1 = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids1);
    mManager->HandleHostMetadataUpdate(cids1);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key1].containerIds.size(), 2u);

    // 添加 config2
    CollectionPipelineContext ctx2;
    ctx2.SetConfigName("config2");
    ctx2.SetProcessQueueKey(2);
    ObserverNetworkOption opt2;
    opt2.mApmConfig = {.mWorkspace = "w2", .mAppName = "app2", .mAppId = "id2", .mServiceId = "sid2"};
    opt2.mL4Config.mEnable = false;
    opt2.mL7Config
        = {.mEnable = true, .mEnableSpan = false, .mEnableMetric = true, .mEnableLog = false, .mSampleRate = 0.5};
    opt2.mSelectors = {{"workload2", "kind2", "ns2"}};
    mManager->AddOrUpdateConfig(&ctx2, 1, nullptr, &opt2);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key1].containerIds.size(), 2u);
    size_t key2 = GenWorkloadKey("ns2", "kind2", "workload2");
    std::vector<std::string> cids2 = {"cid3"};
    AddPodInfo("ns2", "kind2", "workload2", cids2);
    mManager->HandleHostMetadataUpdate({"cid2", "cid1", "cid3"});
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key1].containerIds.size(), 2u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key2].containerIds.size(), 1u);
    mManager->RemoveConfig("config1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key1), 0u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key2), 1u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key2].containerIds.size(), 1u);
    const auto& appConfig = mManager->mContainerConfigs[GenContainerKey("cid3")];
    APSARA_TEST_TRUE(appConfig != nullptr);
    APSARA_TEST_EQUAL(appConfig->mAppId, "id2");
    APSARA_TEST_EQUAL(appConfig->mAppName, "app2");
    APSARA_TEST_EQUAL(appConfig->mWorkspace, "w2");
    APSARA_TEST_EQUAL(appConfig->mServiceId, "sid2");
    APSARA_TEST_EQUAL(appConfig->mEnableL4, false);
    APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
    APSARA_TEST_EQUAL(appConfig->mEnableLog, false);
    APSARA_TEST_EQUAL(appConfig->mEnableSpan, false);
    APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
    APSARA_TEST_EQUAL(appConfig->mSampleRate, 0.5);
    APSARA_TEST_EQUAL(appConfig->mConfigName, "config2");
    APSARA_TEST_EQUAL(appConfig->mQueueKey, ctx2.GetProcessQueueKey());
    APSARA_TEST_EQUAL(appConfig->mPluginIndex, 1);
    APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
    mManager->RemoveConfig("config2");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key2), 0u);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(GenContainerKey("cid3")), 0u);
}

void NetworkObserverConfigUpdateUnittest::MultiConfigContainerIndependence() {
    // config1/workload1/cid1,cid2; config2/workload2/cid3
    CollectionPipelineContext ctx1, ctx2;
    ctx1.SetConfigName("config1");
    ctx1.SetProcessQueueKey(1);
    ctx2.SetConfigName("config2");
    ctx2.SetProcessQueueKey(2);
    ObserverNetworkOption opt1, opt2;
    opt1.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt1.mL4Config.mEnable = true;
    opt1.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt1.mSelectors = {{"workload1", "kind1", "ns1"}};
    opt2.mApmConfig = {.mWorkspace = "w2", .mAppName = "app2", .mAppId = "id2", .mServiceId = "sid2"};
    opt2.mL4Config.mEnable = false;
    opt2.mL7Config
        = {.mEnable = true, .mEnableSpan = false, .mEnableMetric = true, .mEnableLog = false, .mSampleRate = 0.5};
    opt2.mSelectors = {{"workload2", "kind2", "ns2"}};
    mManager->AddOrUpdateConfig(&ctx1, 0, nullptr, &opt1);
    mManager->AddOrUpdateConfig(&ctx2, 1, nullptr, &opt2);
    size_t key1 = GenWorkloadKey("ns1", "kind1", "workload1");
    size_t key2 = GenWorkloadKey("ns2", "kind2", "workload2");
    std::vector<std::string> cids1 = {"cid1", "cid2"};
    std::vector<std::string> cids2 = {"cid3"};
    AddPodInfo("ns1", "kind1", "workload1", cids1);
    AddPodInfo("ns2", "kind2", "workload2", cids2);
    mManager->HandleHostMetadataUpdate({"cid2", "cid1", "cid3"});
    // 验证 config1/workload1/cid1,cid2
    const auto& wlConfig1 = mManager->mWorkloadConfigs[key1];
    APSARA_TEST_EQUAL(wlConfig1.containerIds.size(), 2u);
    for (const auto& cid : cids1) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cidKey));
        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
        APSARA_TEST_EQUAL(appConfig->mAppName, "app1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableLog, true);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, true);
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 1.0);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "config1");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, ctx1.GetProcessQueueKey());
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 0);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
    }
    // 验证 config2/workload2/cid3
    const auto& wlConfig2 = mManager->mWorkloadConfigs[key2];
    APSARA_TEST_EQUAL(wlConfig2.containerIds.size(), 1u);
    size_t cid3Key = GenContainerKey("cid3");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cid3Key));
    const auto& appConfig2 = mManager->mContainerConfigs[cid3Key];
    APSARA_TEST_EQUAL(appConfig2->mAppId, "id2");
    APSARA_TEST_EQUAL(appConfig2->mAppName, "app2");
    APSARA_TEST_EQUAL(appConfig2->mWorkspace, "w2");
    APSARA_TEST_EQUAL(appConfig2->mServiceId, "sid2");
    APSARA_TEST_EQUAL(appConfig2->mEnableL4, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableL7, true);
    APSARA_TEST_EQUAL(appConfig2->mEnableLog, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableSpan, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableMetric, true);
    APSARA_TEST_EQUAL(appConfig2->mSampleRate, 0.5);
    APSARA_TEST_EQUAL(appConfig2->mConfigName, "config2");
    APSARA_TEST_EQUAL(appConfig2->mQueueKey, ctx2.GetProcessQueueKey());
    APSARA_TEST_EQUAL(appConfig2->mPluginIndex, 1);
    APSARA_TEST_TRUE(appConfig2->mSampler != nullptr);
    // 删除 config1，只影响其下 container
    mManager->RemoveConfig("config1");
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key1), 0u);
    for (const auto& cid : cids1) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cidKey), 0u);
    }
    // config2/workload2/cid3 不受影响
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key2), 1u);
    APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cid3Key));
}

void NetworkObserverConfigUpdateUnittest::ContainerMigrationBetweenConfigs() {
    // cidX 先属于 config1/workload1，后属于 config2/workload2
    CollectionPipelineContext ctx1, ctx2;
    ctx1.SetConfigName("config1");
    ctx1.SetProcessQueueKey(1);
    ctx2.SetConfigName("config2");
    ctx2.SetProcessQueueKey(2);
    ObserverNetworkOption opt1, opt2;
    opt1.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt1.mL4Config.mEnable = true;
    opt1.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt1.mSelectors = {{"workload1", "kind1", "ns1"}};
    opt2.mApmConfig = {.mWorkspace = "w2", .mAppName = "app2", .mAppId = "id2", .mServiceId = "sid2"};
    opt2.mL4Config.mEnable = false;
    opt2.mL7Config
        = {.mEnable = true, .mEnableSpan = false, .mEnableMetric = true, .mEnableLog = false, .mSampleRate = 0.5};
    opt2.mSelectors = {{"workload2", "kind2", "ns2"}};
    mManager->AddOrUpdateConfig(&ctx1, 0, nullptr, &opt1);
    size_t key1 = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cidX"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    // 验证归属 config1
    size_t cidKey = GenContainerKey("cidX");
    APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cidKey));
    const auto& appConfig1 = mManager->mContainerConfigs[cidKey];
    APSARA_TEST_TRUE(mManager->mWorkloadConfigs.count(key1) > 0);
    APSARA_TEST_EQUAL(appConfig1->mAppId, "id1");
    APSARA_TEST_EQUAL(appConfig1->mAppName, "app1");
    APSARA_TEST_EQUAL(appConfig1->mWorkspace, "w1");
    APSARA_TEST_EQUAL(appConfig1->mServiceId, "sid1");
    APSARA_TEST_EQUAL(appConfig1->mEnableL4, true);
    APSARA_TEST_EQUAL(appConfig1->mEnableL7, true);
    APSARA_TEST_EQUAL(appConfig1->mEnableLog, true);
    APSARA_TEST_EQUAL(appConfig1->mEnableSpan, true);
    APSARA_TEST_EQUAL(appConfig1->mEnableMetric, true);
    APSARA_TEST_EQUAL(appConfig1->mSampleRate, 1.0);
    APSARA_TEST_EQUAL(appConfig1->mConfigName, "config1");
    APSARA_TEST_EQUAL(appConfig1->mQueueKey, ctx1.GetProcessQueueKey());
    APSARA_TEST_EQUAL(appConfig1->mPluginIndex, 0);
    APSARA_TEST_TRUE(appConfig1->mSampler != nullptr);
    // 迁移到 config2/workload2
    mManager->AddOrUpdateConfig(&ctx2, 1, nullptr, &opt2);
    AddPodInfo("ns2", "kind2", "workload2", cids);
    mManager->HandleHostMetadataUpdate(cids);
    size_t key2 = GenWorkloadKey("ns2", "kind2", "workload2");
    APSARA_TEST_TRUE(mManager->mWorkloadConfigs.count(key2) > 0);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key1].containerIds.count("cidX"), 0u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key2].containerIds.count("cidX"), 1u);
    const auto& appConfig2 = mManager->mContainerConfigs[cidKey];
    APSARA_TEST_TRUE(appConfig2 != nullptr);
    APSARA_TEST_EQUAL(appConfig2->mAppId, "id2");
    APSARA_TEST_EQUAL(appConfig2->mAppName, "app2");
    APSARA_TEST_EQUAL(appConfig2->mWorkspace, "w2");
    APSARA_TEST_EQUAL(appConfig2->mServiceId, "sid2");
    APSARA_TEST_EQUAL(appConfig2->mEnableL4, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableL7, true);
    APSARA_TEST_EQUAL(appConfig2->mEnableLog, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableSpan, false);
    APSARA_TEST_EQUAL(appConfig2->mEnableMetric, true);
    APSARA_TEST_EQUAL(appConfig2->mSampleRate, 0.5);
    APSARA_TEST_EQUAL(appConfig2->mConfigName, "config2");
    APSARA_TEST_EQUAL(appConfig2->mQueueKey, ctx2.GetProcessQueueKey());
    APSARA_TEST_EQUAL(appConfig2->mPluginIndex, 1);
    APSARA_TEST_TRUE(appConfig2->mSampler != nullptr);
    // config1/workload1 不再包含 cidX
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key1].containerIds.count("cidX"), 0u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs[key2].containerIds.count("cidX"), 1u);
}

void NetworkObserverConfigUpdateUnittest::SelectorChangeAffectsContainer() {
    // config selector 变更导致 container 归属变化
    CollectionPipelineContext ctx;
    ctx.SetConfigName("config1");
    ctx.SetProcessQueueKey(1);
    ObserverNetworkOption opt;
    opt.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt.mL4Config.mEnable = true;
    opt.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    size_t key1 = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids1 = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids1);
    mManager->HandleHostMetadataUpdate(cids1);
    // selector 变更，workload1->workload2
    opt.mSelectors = {{"workload2", "kind2", "ns2"}};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    size_t key2 = GenWorkloadKey("ns2", "kind2", "workload2");
    std::vector<std::string> cids2 = {"cid3"};
    AddPodInfo("ns2", "kind2", "workload2", cids2);
    mManager->HandleHostMetadataUpdate(cids2);
    // workload1 container 应被清理
    for (const auto& cid : cids1) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cidKey), 0u);
    }
    // workload2 container 应被新建
    for (const auto& cid : cids2) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cidKey));
        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
        APSARA_TEST_EQUAL(appConfig->mAppName, "app1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableLog, true);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, true);
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 1.0);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "config1");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, ctx.GetProcessQueueKey());
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 0);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
    }
}

void NetworkObserverConfigUpdateUnittest::IdempotentAddRemove() {
    // 重复 add/remove config/container，最终状态一致
    CollectionPipelineContext ctx;
    ctx.SetConfigName("config1");
    ctx.SetProcessQueueKey(1);
    ObserverNetworkOption opt;
    opt.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt.mL4Config.mEnable = true;
    opt.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cid1", "cid2"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    // 重复 add
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    mManager->HandleHostMetadataUpdate(cids);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key), 1u);
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_TRUE(mManager->mContainerConfigs.count(cidKey));
    }
    // 重复 remove
    mManager->RemoveConfig("config1");
    mManager->RemoveConfig("config1");
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        APSARA_TEST_EQUAL(mManager->mContainerConfigs.count(cidKey), 0u);
    }
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.count(key), 0u);
}

void NetworkObserverConfigUpdateUnittest::EmptySelectorAndContainer() {
    // 空 selector/空 container/空 config
    CollectionPipelineContext ctx;
    ctx.SetConfigName("config1");
    ctx.SetProcessQueueKey(1);
    ObserverNetworkOption opt;
    opt.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt.mL4Config.mEnable = true;
    opt.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt.mSelectors = {};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads["config1"].size(), 1u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 1u);
    // 空 container
    mManager->HandleHostMetadataUpdate({});
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 1u);
    // 空 config
    mManager->RemoveConfig("config1");
    APSARA_TEST_EQUAL(mManager->mConfigToWorkloads.count("config1"), 0u);
    APSARA_TEST_EQUAL(mManager->mWorkloadConfigs.size(), 0u);
    APSARA_TEST_EQUAL(mManager->mContainerConfigs.size(), 0u);
}

void NetworkObserverConfigUpdateUnittest::PartialFieldUpdate() {
    // config 更新时只变更部分字段
    CollectionPipelineContext ctx;
    ctx.SetConfigName("config1");
    ctx.SetProcessQueueKey(1);
    ObserverNetworkOption opt;
    opt.mApmConfig = {.mWorkspace = "w1", .mAppName = "app1", .mAppId = "id1", .mServiceId = "sid1"};
    opt.mL4Config.mEnable = true;
    opt.mL7Config
        = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
    opt.mSelectors = {{"workload1", "kind1", "ns1"}};
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    size_t key = GenWorkloadKey("ns1", "kind1", "workload1");
    std::vector<std::string> cids = {"cid1"};
    AddPodInfo("ns1", "kind1", "workload1", cids);
    mManager->HandleHostMetadataUpdate(cids);
    // 只变更部分字段
    opt.mL7Config.mEnableLog = false;
    opt.mL7Config.mSampleRate = 0.2;
    mManager->AddOrUpdateConfig(&ctx, 0, nullptr, &opt);
    for (const auto& cid : cids) {
        size_t cidKey = GenContainerKey(cid);
        const auto& appConfig = mManager->mContainerConfigs[cidKey];
        APSARA_TEST_EQUAL(appConfig->mAppId, "id1");
        APSARA_TEST_EQUAL(appConfig->mAppName, "app1");
        APSARA_TEST_EQUAL(appConfig->mWorkspace, "w1");
        APSARA_TEST_EQUAL(appConfig->mServiceId, "sid1");
        APSARA_TEST_EQUAL(appConfig->mEnableL4, true);
        APSARA_TEST_EQUAL(appConfig->mEnableL7, true);
        APSARA_TEST_EQUAL(appConfig->mEnableLog, false);
        APSARA_TEST_EQUAL(appConfig->mEnableSpan, true);
        APSARA_TEST_EQUAL(appConfig->mEnableMetric, true);
        APSARA_TEST_EQUAL(appConfig->mSampleRate, 0.2);
        APSARA_TEST_EQUAL(appConfig->mConfigName, "config1");
        APSARA_TEST_EQUAL(appConfig->mQueueKey, ctx.GetProcessQueueKey());
        APSARA_TEST_EQUAL(appConfig->mPluginIndex, 0);
        APSARA_TEST_TRUE(appConfig->mSampler != nullptr);
    }
}

UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, AddConfig);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, UpdateConfig);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, RemoveConfig);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, AddContainer);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, RemoveContainer);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, ConfigAndContainerOrthogonal);


UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, MultiConfigContainerIndependence);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, ContainerMigrationBetweenConfigs);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, SelectorChangeAffectsContainer);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, IdempotentAddRemove);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, EmptySelectorAndContainer);
UNIT_TEST_CASE(NetworkObserverConfigUpdateUnittest, PartialFieldUpdate);

} // namespace logtail::ebpf
UNIT_TEST_MAIN
