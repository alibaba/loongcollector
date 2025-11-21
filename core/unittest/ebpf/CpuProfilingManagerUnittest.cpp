// Copyright 2025 LoongCollector Authors
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

#include <cstring>
#include <gtest/gtest.h>

#include <memory>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "common/JsonUtil.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/cpu_profiling/CpuProfilingManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

class CpuProfilingManagerUnittest : public ::testing::Test {
public:
    void TestConstructor();
    void TestAddOrUpdateConfig();
    void TestHandleProcessDiscoveryEvent();
    void TestHandleCpuProfilingEventSingleConfig();
    void TestHandleCpuProfilingEventMultiConfig();

protected:
    void SetUp() override {
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mEventQueue = std::make_unique<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>>();

        WriteMetrics::GetInstance()->CreateMetricsRecordRef(mMetricRef,
                                                            MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE,
                                                            {{METRIC_LABEL_KEY_PLUGIN_ID, "test_plugin"}},
                                                            DynamicMetricLabels{});
        mPluginMetricPtr = std::make_shared<PluginMetricManager>(
            mMetricRef.GetLabels(),
            std::unordered_map<std::string, MetricType>{
                {METRIC_PLUGIN_IN_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
                {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER}},
            MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
        WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricRef);

        mManager = std::make_shared<CpuProfilingManager>(mWrapper.mProcessCacheManager,
                                                         mEBPFAdapter,
                                                         *mEventQueue,
                                                         &mEventPool);
    }

    void TearDown() override {
        mWrapper.Clear();
        mRetryableEventCache.Clear();
    }

private:
    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    ProcessCacheManagerWrapper mWrapper;
    std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>> mEventQueue;
    EventPool mEventPool = EventPool(true);
    std::shared_ptr<CpuProfilingManager> mManager;
    MetricsRecordRef mMetricRef;
    PluginMetricManagerPtr mPluginMetricPtr;
    RetryableEventCache mRetryableEventCache;
};

void CpuProfilingManagerUnittest::TestConstructor() {
    APSARA_TEST_TRUE(mManager != nullptr);
    APSARA_TEST_EQUAL(mManager->GetPluginType(), PluginType::CPU_PROFILING);
}

void CpuProfilingManagerUnittest::TestAddOrUpdateConfig() {
    std::string configStr = R"(
        {
            "Type": "input_cpu_profiling",
            "CommandLines": ["]["]
        }
    )"; // wrong regex is intentional
    std::string errorMsg;
    Json::Value configJson, optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-config");

    CpuProfilingOption option;
    option.Init(configJson, &ctx, "test-config");

    PluginMetricManagerPtr metricMgr;
    APSARA_TEST_EQUAL(0, mManager->AddOrUpdateConfig(&ctx, 0, metricMgr, &option));
    APSARA_TEST_EQUAL(1, mManager->mConfigNameToKey.count("test-config"));
    APSARA_TEST_EQUAL(1, mManager->mConfigInfoMap.count(mManager->mConfigNameToKey["test-config"]));

    APSARA_TEST_EQUAL(0, mManager->RemoveConfig("test-config"));
    APSARA_TEST_EQUAL(0, mManager->mConfigNameToKey.size());
    APSARA_TEST_EQUAL(0, mManager->mConfigInfoMap.size());
}

using DiscoverResult = ProcessDiscoveryManager::DiscoverResult;
void AddMockDiscoverResult(
    DiscoverResult& result, size_t configKey,
    std::set<uint32_t> matchedPids) {
    result.emplace_back(configKey, std::move(matchedPids));
}

void CpuProfilingManagerUnittest::TestHandleProcessDiscoveryEvent() {
    DiscoverResult result1;
    AddMockDiscoverResult(result1, 1, {1, 2, 3});
    mManager->HandleProcessDiscoveryEvent(result1);
    APSARA_TEST_EQUAL(mManager->mRouter[1].size(), 1);
    APSARA_TEST_EQUAL(mManager->mRouter[2].size(), 1);
    APSARA_TEST_EQUAL(mManager->mRouter[3].size(), 1);

    DiscoverResult result2;
    AddMockDiscoverResult(result2, 1, {1});
    AddMockDiscoverResult(result2, 2, {1, 3});
    mManager->HandleProcessDiscoveryEvent(result2);
    APSARA_TEST_EQUAL(mManager->mRouter[1].size(), 2);
    APSARA_TEST_EQUAL(mManager->mRouter[2].size(), 0);
    APSARA_TEST_EQUAL(mManager->mRouter[3].size(), 1);
}

void CpuProfilingManagerUnittest::TestHandleCpuProfilingEventSingleConfig() {
    const size_t kConfigKey = 1;
    QueueKey key = QueueKeyManager::GetInstance()->GetKey("test-1");
    // create queue
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test-1");
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(key, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("test-1");

    // mock AddOrUpdateConfig
    CpuProfilingManager::ConfigInfo info{
        .mPipelineCtx = nullptr,
        .mQueueKey = key,
        .mPluginIndex = 0,
    };
    mManager->mConfigInfoMap[kConfigKey] = info;

    // handle process discovery event
    DiscoverResult result;
    AddMockDiscoverResult(result, kConfigKey, {1, 2, 3});
    mManager->HandleProcessDiscoveryEvent(result);

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    // handle profiling event and send it to queue
    mManager->HandleCpuProfilingEvent(3, "abc", "abc:3;a;b;c 10", 10);
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test-1", configName);

    // pid=4 not in router, so queue is empty
    mManager->HandleCpuProfilingEvent(4, "abc", "abc:4;a;b;c 10", 10);
    APSARA_TEST_FALSE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
}

void CpuProfilingManagerUnittest::TestHandleCpuProfilingEventMultiConfig() {
    const size_t kConfigKey1 = 1;
    const size_t kConfigKey2 = 2;
    QueueKey key1 = QueueKeyManager::GetInstance()->GetKey("test-multi-1");
    QueueKey key2 = QueueKeyManager::GetInstance()->GetKey("test-multi-2");

    // create queue
    CollectionPipelineContext ctx;
    // queue 1
    ctx.SetConfigName("test-multi-1");
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(key1, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("test-multi-1");
    // queue 2
    ctx.SetConfigName("test-multi-2");
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(key2, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("test-multi-2");

    // mock AddOrUpdateConfig
    // config 1
    mManager->mConfigInfoMap[kConfigKey1] = {
        .mPipelineCtx = nullptr,
        .mQueueKey = key1,
        .mPluginIndex = 0,
    };
    // config 2
    mManager->mConfigInfoMap[kConfigKey2] = {
        .mPipelineCtx = nullptr,
        .mQueueKey = key2,
        .mPluginIndex = 0,
    };

    // handle process discovery event
    DiscoverResult result;
    AddMockDiscoverResult(result, kConfigKey1, {1, 2});
    AddMockDiscoverResult(result, kConfigKey2, {2, 3});
    mManager->HandleProcessDiscoveryEvent(result);

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;

    // handle pid 1, queue 1 get item
    mManager->HandleCpuProfilingEvent(1, "abc", "abc:1;a;b;c 10", 10);
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test-multi-1", configName);
    APSARA_TEST_FALSE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));

    // handle pid 2, queue 1 and queue 2 get item
    mManager->HandleCpuProfilingEvent(2, "abc", "abc:2;a;b;c 10", 10);
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test-multi-1", configName);
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL("test-multi-2", configName);

    // handle pid 4, no queue get item
    mManager->HandleCpuProfilingEvent(4, "abc", "abc:4;a;b;c 10", 10);
    APSARA_TEST_FALSE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
}

UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestConstructor);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestAddOrUpdateConfig);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleProcessDiscoveryEvent);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleCpuProfilingEventSingleConfig);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleCpuProfilingEventMultiConfig);

UNIT_TEST_MAIN
