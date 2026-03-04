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

    // test CpuProfilingManager::addContentToEvent
    void TestAddContentToEventBasic();
    void TestAddContentToEventEmptyStack();
    void TestAddContentToEventEmptyNameAndComm();

    // test CpuProfilingManager::parseStackCnt
    void TestParseStackCnt();
    void TestParseStackCntEmpty();
    void TestParseStackCntInvalidFormat();
    void TestParseStackCntMultipleLines();
    void TestParseStackCntWithNullTraceId();
    void TestParseStackCntWithEmptyStack();
    void TestParseStackCntEdgeCases();

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

        mManager = std::make_shared<CpuProfilingManager>(
            mWrapper.mProcessCacheManager, mEBPFAdapter, *mEventQueue, &mEventPool);
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
void AddMockDiscoverResult(DiscoverResult& result, size_t configKey, std::set<uint32_t> matchedPids) {
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
    ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key, 0, ctx);
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
    ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key1, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("test-multi-1");
    // queue 2
    ctx.SetConfigName("test-multi-2");
    ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key2, 0, ctx);
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

void CpuProfilingManagerUnittest::TestAddContentToEventBasic() {
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto* event = eventGroup.AddLogEvent();
    CpuProfilingManager::addContentToEvent(event, {"func0", "func1", "func2"}, "testApp", "testComm", 10000000);
    APSARA_TEST_EQUAL(event->GetContent("name"), "func2");
    APSARA_TEST_EQUAL(event->GetContent("stack"), "func1\nfunc0");
    APSARA_TEST_TRUE(event->HasContent("stackID"));
    APSARA_TEST_EQUAL(event->GetContent("type"), "profile_cpu");
    APSARA_TEST_EQUAL(event->GetContent("type_cn"), "");
    APSARA_TEST_EQUAL(event->GetContent("units"), "nanoseconds");
    APSARA_TEST_EQUAL(event->GetContent("val"), "10000000");
    APSARA_TEST_EQUAL(event->GetContent("valueTypes"), "cpu");
    APSARA_TEST_EQUAL(event->GetContent("valueTypes_cn"), "");
    APSARA_TEST_EQUAL(event->GetContent("labels"), R"({"__name__": "testApp", "thread": "testComm"})");
}

void CpuProfilingManagerUnittest::TestAddContentToEventEmptyStack() {
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto* event = eventGroup.AddLogEvent();
    CpuProfilingManager::addContentToEvent(event, {}, "testApp", "testComm", 10000000);
    APSARA_TEST_TRUE(!event->HasContent("name"));
    APSARA_TEST_TRUE(!event->HasContent("stack"));
}

void CpuProfilingManagerUnittest::TestAddContentToEventEmptyNameAndComm() {
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto* event = eventGroup.AddLogEvent();
    CpuProfilingManager::addContentToEvent(event, {"func0", "func1", "func2"}, "", "", 10000000);
    APSARA_TEST_EQUAL(event->GetContent("name"), "func2");
    APSARA_TEST_EQUAL(event->GetContent("stack"), "func1\nfunc0");
    APSARA_TEST_TRUE(event->HasContent("stackID"));
    APSARA_TEST_EQUAL(event->GetContent("type"), "profile_cpu");
    APSARA_TEST_EQUAL(event->GetContent("type_cn"), "");
    APSARA_TEST_EQUAL(event->GetContent("units"), "nanoseconds");
    APSARA_TEST_EQUAL(event->GetContent("val"), "10000000");
    APSARA_TEST_EQUAL(event->GetContent("valueTypes"), "cpu");
    APSARA_TEST_EQUAL(event->GetContent("valueTypes_cn"), "");
    APSARA_TEST_EQUAL(event->GetContent("labels"), R"({"__name__": "", "thread": ""})");
}

void CpuProfilingManagerUnittest::TestParseStackCnt() {
    const char* symbol = "bash:1234;func1;func2;func3 10 trace123\n";
    std::vector<CpuProfilingManager::StackCnt> result;

    CpuProfilingManager::parseStackCnt(symbol, result);

    APSARA_TEST_EQUAL(1U, result.size());
    
    auto& [stack, cnt, traceId] = result[0];
    APSARA_TEST_EQUAL(3U, stack.size());
    APSARA_TEST_EQUAL("func1", stack[0]);
    APSARA_TEST_EQUAL("func2", stack[1]);
    APSARA_TEST_EQUAL("func3", stack[2]);
    APSARA_TEST_EQUAL(10U, cnt);
    APSARA_TEST_EQUAL("trace123", traceId);
}

void CpuProfilingManagerUnittest::TestParseStackCntEmpty() {
    const char* symbol = "";
    std::vector<CpuProfilingManager::StackCnt> result;

    CpuProfilingManager::parseStackCnt(symbol, result);

    APSARA_TEST_EQUAL(0U, result.size());
}

void CpuProfilingManagerUnittest::TestParseStackCntInvalidFormat() {
    std::vector<CpuProfilingManager::StackCnt> result;

    const char* noSemicolon = "bash:1234 func1 10 trace123\n";
    CpuProfilingManager::parseStackCnt(noSemicolon, result);
    APSARA_TEST_EQUAL(0U, result.size());

    result.clear();
    const char* noFirstSpace = "bash:1234;func1;func2;func3\n";
    CpuProfilingManager::parseStackCnt(noFirstSpace, result);
    APSARA_TEST_EQUAL(0U, result.size());

    result.clear();
    const char* noSecondSpace = "bash:1234;func1;func2;func3 10\n";
    CpuProfilingManager::parseStackCnt(noSecondSpace, result);
    APSARA_TEST_EQUAL(0U, result.size());

    result.clear();
    const char* wrongOrder = "bash:1234;10 func1;func2 trace123\n";
    CpuProfilingManager::parseStackCnt(wrongOrder, result);
    APSARA_TEST_EQUAL(0U, result.size());

    result.clear();
    const char* invalidCount = "bash:1234;func1;func2;func3 abc trace123\n";
    CpuProfilingManager::parseStackCnt(invalidCount, result);
    APSARA_TEST_EQUAL(0U, result.size());
}

void CpuProfilingManagerUnittest::TestParseStackCntMultipleLines() {
    const char* symbol = 
        "bash:1234;func1;func2;func3 10 trace123\n"
        "nginx:5678;handle_request;process_data 20 trace456\n"
        "python:9012;main;run;execute 15 trace789\n";
    
    std::vector<CpuProfilingManager::StackCnt> result;
    CpuProfilingManager::parseStackCnt(symbol, result);

    APSARA_TEST_EQUAL(3U, result.size());
    
    auto& [stack1, cnt1, traceId1] = result[0];
    APSARA_TEST_EQUAL(3U, stack1.size());
    APSARA_TEST_EQUAL("func1", stack1[0]);
    APSARA_TEST_EQUAL(10U, cnt1);
    APSARA_TEST_EQUAL("trace123", traceId1);
    
    auto& [stack2, cnt2, traceId2] = result[1];
    APSARA_TEST_EQUAL(2U, stack2.size());
    APSARA_TEST_EQUAL("handle_request", stack2[0]);
    APSARA_TEST_EQUAL("process_data", stack2[1]);
    APSARA_TEST_EQUAL(20U, cnt2);
    APSARA_TEST_EQUAL("trace456", traceId2);
    
    auto& [stack3, cnt3, traceId3] = result[2];
    APSARA_TEST_EQUAL(3U, stack3.size());
    APSARA_TEST_EQUAL(15U, cnt3);
    APSARA_TEST_EQUAL("trace789", traceId3);
}

void CpuProfilingManagerUnittest::TestParseStackCntWithNullTraceId() {
    const char* symbol = "bash:1234;func1;func2 10 null\n";
    std::vector<CpuProfilingManager::StackCnt> result;

    CpuProfilingManager::parseStackCnt(symbol, result);

    APSARA_TEST_EQUAL(1U, result.size());
    
    auto& [stack, cnt, traceId] = result[0];
    APSARA_TEST_EQUAL(2U, stack.size());
    APSARA_TEST_EQUAL(10U, cnt);
    APSARA_TEST_EQUAL("", traceId); // null convert to empty string
}

void CpuProfilingManagerUnittest::TestParseStackCntWithEmptyStack() {
    // only comm:pid，no stack frame
    const char* symbol = "bash:1234; 10 trace123\n";
    std::vector<CpuProfilingManager::StackCnt> result;

    CpuProfilingManager::parseStackCnt(symbol, result);

    APSARA_TEST_EQUAL(1U, result.size());
    
    auto& [stack, cnt, traceId] = result[0];
    APSARA_TEST_EQUAL(0U, stack.size()); // stack is empty
    APSARA_TEST_EQUAL(10U, cnt);
    APSARA_TEST_EQUAL("trace123", traceId);
}

void CpuProfilingManagerUnittest::TestParseStackCntEdgeCases() {
    std::vector<CpuProfilingManager::StackCnt> result;

    // count is 0
    const char* zeroCount = "bash:1234;func1 0 trace123\n";
    CpuProfilingManager::parseStackCnt(zeroCount, result);
    APSARA_TEST_EQUAL(1U, result.size());
    APSARA_TEST_EQUAL(0U, std::get<1>(result[0]));

    // count is max
    result.clear();
    const char* maxCount = "bash:1234;func1 4294967295 trace123\n"; // UINT32_MAX
    CpuProfilingManager::parseStackCnt(maxCount, result);
    APSARA_TEST_EQUAL(1U, result.size());
    APSARA_TEST_EQUAL(4294967295U, std::get<1>(result[0]));

    // very deep
    result.clear();
    std::string deepStack = "bash:1234;";
    for (int i = 0; i < 100; i++) {
        deepStack += "func" + std::to_string(i);
        if (i < 99) deepStack += ";";
    }
    deepStack += " 10 trace123\n";
    CpuProfilingManager::parseStackCnt(deepStack.c_str(), result);
    APSARA_TEST_EQUAL(1U, result.size());
    APSARA_TEST_EQUAL(100U, std::get<0>(result[0]).size());

    // stack with space
    result.clear();
    const char* spaceInTraceId = "bash:1234;func1;stack with space;func2 10 trace123\n";
    CpuProfilingManager::parseStackCnt(spaceInTraceId, result);
    APSARA_TEST_EQUAL(1U, result.size());
    auto& [stack, cnt, traceId] = result[0];
    APSARA_TEST_EQUAL(3U, stack.size());
    APSARA_TEST_EQUAL("func1", stack[0]);
    APSARA_TEST_EQUAL("stack with space", stack[1]);
    APSARA_TEST_EQUAL("func2", stack[2]);

    // multiple newline
    result.clear();
    const char* multipleNewlines = "bash:1234;func1 10 trace123\n\n\n";
    CpuProfilingManager::parseStackCnt(multipleNewlines, result);
    APSARA_TEST_EQUAL(1U, result.size()); // only one line

    // valid and invalid
    result.clear();
    const char* mixed = 
        "bash:1234;func1 10 trace123\n"
        "invalid line without format\n"
        "nginx:5678;func2 20 trace456\n";
    CpuProfilingManager::parseStackCnt(mixed, result);
    APSARA_TEST_EQUAL(2U, result.size()); // only two line
}



UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestConstructor);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestAddOrUpdateConfig);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleProcessDiscoveryEvent);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleCpuProfilingEventSingleConfig);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestHandleCpuProfilingEventMultiConfig);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestAddContentToEventBasic);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestAddContentToEventEmptyStack);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestAddContentToEventEmptyNameAndComm);
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCnt)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntEmpty)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntInvalidFormat)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntMultipleLines)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntWithNullTraceId)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntWithEmptyStack)
UNIT_TEST_CASE(CpuProfilingManagerUnittest, TestParseStackCntEdgeCases)

UNIT_TEST_MAIN
