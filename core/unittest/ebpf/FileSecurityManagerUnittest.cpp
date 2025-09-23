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

#include <chrono>
#include <memory>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "coolbpf/security/type.h"
#include "ebpf/plugin/ProcessCacheValue.h"
#include "ebpf/plugin/file_security/FileSecurityManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ManagerUnittestBase.h"

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
    memcpy(event.path, "abcd", 4);
    strncat(event.path, path, sizeof(event.path) - 5);
    return event;
}

class FileSecurityManagerUnittest : public SecurityManagerUnittestBase {
public:
    void TestConstructor();
    void TestCreateFileRetryableEvent();
    void TestRecordFileEvent();
    void TestHandleEvent();
    void TestSendEvents();
    void TestFileSecurityManagerEventHandling();
    void TestFileSecurityManagerAggregation();
    void TestFileSecurityManagerErrorHandling();

protected:
    std::shared_ptr<AbstractManager> createManagerInstance() override {
        return std::make_shared<FileSecurityManager>(
            mProcessCacheManager, mMockEBPFAdapter, *mEventQueue, mEventPool.get(), *mRetryableEventCache);
    }
};

void FileSecurityManagerUnittest::TestConstructor() {
    auto manager = createAndInitManagerInstance();
    APSARA_TEST_TRUE(manager != nullptr);
    APSARA_TEST_EQUAL(manager->GetPluginType(), PluginType::FILE_SECURITY);
}

void FileSecurityManagerUnittest::TestCreateFileRetryableEvent() {
    auto manager = createAndInitManagerInstance();
    file_data_t event = CreateMockFileEvent();
    auto* retryEvent = static_cast<FileSecurityManager*>(manager.get())->CreateFileRetryableEvent(&event);
    APSARA_TEST_TRUE(retryEvent != nullptr);

    delete retryEvent;
}

void FileSecurityManagerUnittest::TestRecordFileEvent() {
    auto manager = createAndInitManagerInstance();

    // event is null
    static_cast<FileSecurityManager*>(manager.get())->RecordFileEvent(nullptr);
    APSARA_TEST_EQUAL(0UL, static_cast<FileSecurityManager*>(manager.get())->EventCache().Size());

    // ProcessCacheManager is null
    auto nullManager = std::make_shared<FileSecurityManager>(nullptr, // ProcessCacheManager
                                                             mMockEBPFAdapter,
                                                             *mEventQueue,
                                                             mEventPool.get(),
                                                             *mRetryableEventCache);
    file_data_t event = CreateMockFileEvent();
    nullManager->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(0UL, nullManager->EventCache().Size());

    // success
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    static_cast<FileSecurityManager*>(manager.get())->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(0UL, static_cast<FileSecurityManager*>(manager.get())->EventCache().Size());

    // no cache
    mProcessCacheManager->mProcessCache.removeCache({event.key.pid, event.key.ktime});
    static_cast<FileSecurityManager*>(manager.get())->RecordFileEvent(&event);
    APSARA_TEST_EQUAL(1UL, static_cast<FileSecurityManager*>(manager.get())->EventCache().Size());
}

void FileSecurityManagerUnittest::TestHandleEvent() {
    auto manager = createAndInitManagerInstance();

    // test normal event
    auto fileEvent = std::make_shared<FileEvent>(
        1234, 123456789, KernelEventType::FILE_PERMISSION_EVENT, 1234567890123ULL, StringView("/etc/passwd"));
    int result = manager->HandleEvent(fileEvent);
    APSARA_TEST_EQUAL(0, result);

    // test null event
    auto nullEvent = std::shared_ptr<CommonEvent>(nullptr);
    result = manager->HandleEvent(nullEvent);
    APSARA_TEST_EQUAL(1, result);
}

void FileSecurityManagerUnittest::TestSendEvents() {
    auto manager = createAndInitManagerInstance();

    // not running
    int result = manager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // time interval is too short
    static_cast<FileSecurityManager*>(manager.get())->mInited = true;
    static_cast<FileSecurityManager*>(manager.get())->mLastSendTimeMs
        = TimeKeeper::GetInstance()->NowMs() - static_cast<FileSecurityManager*>(manager.get())->mSendIntervalMs + 10;
    result = manager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // empty node
    static_cast<FileSecurityManager*>(manager.get())->mLastSendTimeMs
        = TimeKeeper::GetInstance()->NowMs() - static_cast<FileSecurityManager*>(manager.get())->mSendIntervalMs - 10;
    result = manager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    auto fileEvent = std::make_shared<FileEvent>(
        1234, 123456789, KernelEventType::FILE_PERMISSION_EVENT, 1234567890123ULL, StringView("/etc/passwd"));

    // ProcessCacheManager is null
    auto nullManager = std::make_shared<FileSecurityManager>(nullptr, // ProcessCacheManager
                                                             mMockEBPFAdapter,
                                                             *mEventQueue,
                                                             mEventPool.get(),
                                                             *mRetryableEventCache);
    nullManager->mInited = true;
    nullManager->HandleEvent(fileEvent);
    result = nullManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // failed to finalize process tags
    auto cacheManager = createAndInitManagerInstance();
    static_cast<FileSecurityManager*>(cacheManager.get())->mInited = true;
    static_cast<FileSecurityManager*>(cacheManager.get())->HandleEvent(fileEvent);
    result = cacheManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // mPipelineCtx is nullptr
    static_cast<FileSecurityManager*>(cacheManager.get())->HandleEvent(fileEvent);

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    cacheValue->SetContent<kBinary>(StringView("/usr/bin/test"));
    mProcessCacheManager->mProcessCache.AddCache({1234, 123456789}, cacheValue);

    result = cacheManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // push queue failed
    static_cast<FileSecurityManager*>(cacheManager.get())->HandleEvent(fileEvent);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    // mManager->UpdateContext(&ctx, 123, 1);
    result = cacheManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);

    // success
    std::vector<KernelEventType> eventTypes = {KernelEventType::FILE_PATH_TRUNCATE,
                                               KernelEventType::FILE_MMAP,
                                               KernelEventType::FILE_PERMISSION_EVENT,
                                               KernelEventType::FILE_PERMISSION_EVENT_WRITE,
                                               KernelEventType::FILE_PERMISSION_EVENT_READ,
                                               KernelEventType::PROCESS_EXECVE_EVENT};
    for (auto eventType : eventTypes) {
        static_cast<FileSecurityManager*>(cacheManager.get())
            ->HandleEvent(
                std::make_shared<FileEvent>(1234, 123456789, eventType, 1234567890123ULL, StringView("/etc/passwd")));
    }

    QueueKey queueKey = QueueKeyManager::GetInstance()->GetKey("test_config");
    ctx.SetProcessQueueKey(queueKey);
    // mManager->UpdateContext(&ctx, queueKey, 1);
    ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(queueKey, 0, ctx);
    result = cacheManager->SendEvents();
    APSARA_TEST_EQUAL(0, result);
}


void FileSecurityManagerUnittest::TestFileSecurityManagerEventHandling() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 测试文件权限事件
    auto permissionEvent = std::make_shared<FileEvent>(1234,
                                                       5678,
                                                       KernelEventType::FILE_PERMISSION_EVENT,
                                                       std::chrono::system_clock::now().time_since_epoch().count(),
                                                       "/test/file.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(permissionEvent), 0);

    // 测试文件内存映射事件
    auto mmapEvent = std::make_shared<FileEvent>(1234,
                                                 5678,
                                                 KernelEventType::FILE_MMAP,
                                                 std::chrono::system_clock::now().time_since_epoch().count(),
                                                 "/test/mmap.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(mmapEvent), 0);

    // 测试文件截断事件
    auto truncateEvent = std::make_shared<FileEvent>(1234,
                                                     5678,
                                                     KernelEventType::FILE_PATH_TRUNCATE,
                                                     std::chrono::system_clock::now().time_since_epoch().count(),
                                                     "/test/truncate.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(truncateEvent), 0);

    manager->Destroy();
}

void FileSecurityManagerUnittest::TestFileSecurityManagerErrorHandling() {
    auto manager = createAndInitManagerInstance();

    // 测试 null 事件处理
    APSARA_TEST_NOT_EQUAL(manager->HandleEvent(nullptr), 0);

    auto validEvent = std::make_shared<FileEvent>(1234,
                                                  5678,
                                                  KernelEventType::FILE_PERMISSION_EVENT,
                                                  std::chrono::system_clock::now().time_since_epoch().count(),
                                                  "/test/file.txt");

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    APSARA_TEST_EQUAL(manager->HandleEvent(validEvent), 0);

    // 测试无效的文件路径
    auto invalidPathEvent = std::make_shared<FileEvent>(1234,
                                                        5678,
                                                        KernelEventType::FILE_PERMISSION_EVENT,
                                                        std::chrono::system_clock::now().time_since_epoch().count(),
                                                        "");
    APSARA_TEST_EQUAL(manager->HandleEvent(invalidPathEvent), 0);

    // 测试不同类型的事件
    auto mmapEvent = std::make_shared<FileEvent>(1234,
                                                 5678,
                                                 KernelEventType::FILE_MMAP,
                                                 std::chrono::system_clock::now().time_since_epoch().count(),
                                                 "/test/mmap.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(mmapEvent), 0);

    manager->Destroy();
}

void FileSecurityManagerUnittest::TestFileSecurityManagerAggregation() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 创建多个相关的文件事件
    std::vector<std::shared_ptr<FileEvent>> events;

    // 同一连接的多个事件
    for (int i = 0; i < 3; ++i) {
        events.push_back(
            std::make_shared<FileEvent>(1234, // pid
                                        5678, // ktime
                                        KernelEventType::FILE_PATH_TRUNCATE, // type
                                        std::chrono::system_clock::now().time_since_epoch().count() + i, // timestamp
                                        "/test/" + std::to_string(i) // path
                                        ));
    }

    // 处理所有事件
    for (const auto& event : events) {
        APSARA_TEST_EQUAL(manager->HandleEvent(event), 0);
    }

    // add cache
    auto execveEvent = std::make_shared<ProcessCacheValue>();
    data_event_id key{1234, 5678};
    execveEvent->mPPid = 2345;
    execveEvent->mPKtime = 6789;

    // 测试缓存更新
    mProcessCacheManager->mProcessCache.AddCache(key, execveEvent);
    auto pExecveEvent = std::make_shared<ProcessCacheValue>();
    data_event_id pkey{2345, 6789};
    mProcessCacheManager->mProcessCache.AddCache(pkey, pExecveEvent);

    // 触发聚合
    APSARA_TEST_EQUAL(0, manager->SendEvents());

    manager->Destroy();
}

UNIT_TEST_CASE(FileSecurityManagerUnittest, TestConstructor);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestCreateFileRetryableEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestRecordFileEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestHandleEvent);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestSendEvents);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestFileSecurityManagerEventHandling);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestFileSecurityManagerAggregation);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestFileSecurityManagerErrorHandling);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestDifferentConfigNamesReplacement);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestSameConfigNameUpdate);
UNIT_TEST_CASE(FileSecurityManagerUnittest, TestBasicConfigUpdate);

UNIT_TEST_MAIN
