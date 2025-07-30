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
#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/type.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/file_security/FileSecurityManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

file_data_t CreateMockFileEvent(uint32_t pid = 1234,
                                uint64_t ktime = 123456789,
                                file_secure_func func = TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION,
                                const char* path = "/etc/passwd") {
    file_data_t event{};
    event.key.pid = pid;
    event.key.ktime = ktime;
    event.pkey.pid = 5678;
    event.pkey.ktime = 567891234;
    event.func = func;
    event.timestamp = 1234567890123ULL;
    event.size = strlen(path);
    strcpy(event.path, "abcd");
    strcat(event.path, path);
    return event;
}

class FileSecurityManagerUnittest : public ::testing::Test {
public:
    void TestConstructor();
    void TestCreateFileRetryableEvent();
    void TestRecordFileEvent();
    void TestHandleEvent();
    void TestSendEvents();

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

        mManager = std::make_shared<FileSecurityManager>(mWrapper.mProcessCacheManager,
                                                         mEBPFAdapter, // EBPFAdapter
                                                         *mEventQueue,
                                                         mRetryableEventCache);
    }

    void TearDown() override {
        mWrapper.Clear();
        mRetryableEventCache.Clear();
    }

private:
    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    ProcessCacheManagerWrapper mWrapper;
    std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>> mEventQueue;
    std::shared_ptr<FileSecurityManager> mManager;
    MetricsRecordRef mMetricRef;
    PluginMetricManagerPtr mPluginMetricPtr;
    RetryableEventCache mRetryableEventCache;
};

void FileSecurityManagerUnittest::TestConstructor() {
    APSARA_TEST_TRUE(mManager != nullptr);
    APSARA_TEST_EQUAL(mManager->GetPluginType(), PluginType::FILE_SECURITY);
}

void FileSecurityManagerUnittest::TestCreateFileRetryableEvent() {
    file_data_t event = CreateMockFileEvent();
    auto* retryEvent = mManager->CreateFileRetryableEvent(&event);
    APSARA_TEST_TRUE(retryEvent != nullptr);

    delete retryEvent;
}

void FileSecurityManagerUnittest::TestRecordFileEvent() {
    // event is null
    mManager->RecordFileEvent(nullptr);
    APSARA_TEST_EQUAL(0UL, mManager->EventCache().Size());


    // ProcessCacheManager is null
    file_data_t event = CreateMockFileEvent();
    mManager = std::make_shared<FileSecurityManager>(nullptr, // ProcessCacheManager
                                                     mEBPFAdapter, // EBPFAdapter
                                                     *mEventQueue,
                                                     mRetryableEventCache);
    mManager->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(0UL, mManager->EventCache().Size());

    // success
    mManager = std::make_shared<FileSecurityManager>(mWrapper.mProcessCacheManager, // ProcessCacheManager
                                                     mEBPFAdapter, // EBPFAdapter
                                                     *mEventQueue,
                                                     mRetryableEventCache);
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    mManager->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(0UL, mManager->EventCache().Size());

    // no cache
    mWrapper.mProcessCacheManager->mProcessCache.removeCache({event.key.pid, event.key.ktime});
    mManager->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(1UL, mManager->EventCache().Size());
}

void FileSecurityManagerUnittest::TestHandleEvent() {
    // test normal event
    auto fileEvent = std::make_shared<FileEvent>(
        1234, 123456789, KernelEventType::FILE_PERMISSION_EVENT, 1234567890123ULL, StringView("/etc/passwd"));
    int result = mManager->HandleEvent(fileEvent);
    APSARA_TEST_EQUAL(0, result);

    // test null event
    auto nullEvent = std::shared_ptr<CommonEvent>(nullptr);
    result = mManager->HandleEvent(nullEvent);
    APSARA_TEST_EQUAL(1, result);
}

void FileSecurityManagerUnittest::TestSendEvents() {
    // not running
    int result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // time interval is too short
    mManager->mInited = true;
    mManager->mLastSendTimeMs = TimeKeeper::GetInstance()->NowMs() - mManager->mSendIntervalMs + 10;
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // empty node
    mManager->mLastSendTimeMs = TimeKeeper::GetInstance()->NowMs() - mManager->mSendIntervalMs - 10;
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    auto fileEvent = std::make_shared<FileEvent>(
        1234, 123456789, KernelEventType::FILE_PERMISSION_EVENT, 1234567890123ULL, StringView("/etc/passwd"));
    // ProcessCacheManager is null
    mManager = std::make_shared<FileSecurityManager>(nullptr, // ProcessCacheManager
                                                     mEBPFAdapter, // EBPFAdapter
                                                     *mEventQueue,
                                                     mRetryableEventCache);
    mManager->mInited = true;
    mManager->HandleEvent(fileEvent);
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // failed to finalize process tags
    mManager = std::make_shared<FileSecurityManager>(mWrapper.mProcessCacheManager, // ProcessCacheManager
                                                     mEBPFAdapter, // EBPFAdapter
                                                     *mEventQueue,
                                                     mRetryableEventCache);
    mManager->mInited = true;
    mManager->HandleEvent(fileEvent);
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // mPipelineCtx is nullptr
    mManager->HandleEvent(fileEvent);

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    cacheValue->SetContent<kBinary>(StringView("/usr/bin/test"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({1234, 123456789}, cacheValue);

    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // push queue failed
    mManager->HandleEvent(fileEvent);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    // mManager->UpdateContext(&ctx, 123, 1);
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // success
    std::vector<KernelEventType> eventTypes = {KernelEventType::FILE_PATH_TRUNCATE,
                                               KernelEventType::FILE_MMAP,
                                               KernelEventType::FILE_PERMISSION_EVENT,
                                               KernelEventType::FILE_PERMISSION_EVENT_WRITE,
                                               KernelEventType::FILE_PERMISSION_EVENT_READ,
                                               KernelEventType::PROCESS_EXECVE_EVENT};
    for (auto eventType : eventTypes) {
        mManager->HandleEvent(
            std::make_shared<FileEvent>(1234, 123456789, eventType, 1234567890123ULL, StringView("/etc/passwd")));
    }

    QueueKey queueKey = QueueKeyManager::GetInstance()->GetKey("test_config");
    ctx.SetProcessQueueKey(queueKey);
    // mManager->UpdateContext(&ctx, queueKey, 1);
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(queueKey, 0, ctx);
    result = mManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);
}

UNIT_TEST_CASE(FileSecurityManagerUnittest, TestConstructor);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestCreateFileRetryableEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestRecordFileEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestHandleEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestSendEvents);

UNIT_TEST_MAIN
