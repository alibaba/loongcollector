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

#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/type.h"
#include "ebpf/plugin/FileRetryableEvent.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;
file_data_t CreateStubFileEvent() {
    file_data_t event{};
    event.key.pid = 1234;
    event.key.ktime = 123456789;
    event.pkey.pid = 5678;
    event.pkey.ktime = 567891234;
    event.func = TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    event.timestamp = 1234567890123ULL;
    event.size = strlen("/etc/passwd");
    strcpy(event.path, "abcd/etc/passwd");
    return event;
}

class FileRetryableEventUnittest : public ::testing::Test {
public:
    void TestHandleMessageWithProcessFound();
    void TestHandleMessageWithProcessNotFound();
    void TestOnRetry();
    void TestOnDrop();
    void TestRetryLimit();
    void TestHandleMessageWithFlushFailure();
    void TestHandleMessageWithDifferentFunctions();
    void TestHandleMessageWithInvalidFunction();
    void TestHandleMessageWithZeroPidKtime();

protected:
    void SetUp() override {
        mCommonEventQueue = std::make_unique<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>>(1);
    }

    void TearDown() override { mWrapper.Clear(); }

private:
    ProcessCacheManagerWrapper mWrapper;
    std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>> mCommonEventQueue;
};

void FileRetryableEventUnittest::TestHandleMessageWithProcessFound() {
    file_data_t event = CreateStubFileEvent();

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    APSARA_TEST_TRUE(result);
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));
}

void FileRetryableEventUnittest::TestHandleMessageWithProcessNotFound() {
    file_data_t event = CreateStubFileEvent();

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    APSARA_TEST_FALSE(result);
    APSARA_TEST_FALSE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_FALSE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));
}

void FileRetryableEventUnittest::TestOnRetry() {
    file_data_t event = CreateStubFileEvent();

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    APSARA_TEST_FALSE(result);

    result = fileEvent.OnRetry();
    APSARA_TEST_FALSE(result);

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    result = fileEvent.OnRetry();
    APSARA_TEST_TRUE(result);
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));

    std::shared_ptr<CommonEvent> newEvent;
    bool newDequeueResult = mCommonEventQueue->try_dequeue(newEvent);
    APSARA_TEST_TRUE(newDequeueResult);
    APSARA_TEST_TRUE(newEvent != nullptr);

    auto fileEventPtr = std::dynamic_pointer_cast<FileEvent>(newEvent);
    APSARA_TEST_TRUE(fileEventPtr != nullptr);
    APSARA_TEST_EQUAL(fileEventPtr->mPid, static_cast<uint32_t>(event.key.pid));
    APSARA_TEST_EQUAL(fileEventPtr->mKtime, static_cast<uint64_t>(event.key.ktime));
    APSARA_TEST_EQUAL(fileEventPtr->mPath, "/etc/passwd");
}

void FileRetryableEventUnittest::TestOnDrop() {
    file_data_t event = CreateStubFileEvent();

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    APSARA_TEST_FALSE(result);

    fileEvent.OnDrop();
}

void FileRetryableEventUnittest::TestRetryLimit() {
    file_data_t event = CreateStubFileEvent();

    FileRetryableEvent fileEvent(0, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);
    fileEvent.HandleMessage();
    APSARA_TEST_FALSE(fileEvent.CanRetry());
}

void FileRetryableEventUnittest::TestHandleMessageWithFlushFailure() {
    file_data_t event = CreateStubFileEvent();

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    auto dummyEvent = std::make_shared<FileEvent>(999, 999, KernelEventType::FILE_PERMISSION_EVENT, 999, "/dummy");
    while (mCommonEventQueue->try_enqueue(dummyEvent)) {
    }

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);
    bool result = fileEvent.HandleMessage();
    APSARA_TEST_FALSE(result);
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_FALSE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));

    std::shared_ptr<CommonEvent> consumedEvent;
    bool dequeueResult = mCommonEventQueue->try_dequeue(consumedEvent);
    APSARA_TEST_TRUE(dequeueResult);

    while (mCommonEventQueue->try_dequeue(consumedEvent)) {
        if (mCommonEventQueue->try_enqueue(dummyEvent)) {
            break;
        }
    }
    bool retryResult = fileEvent.OnRetry();
    APSARA_TEST_TRUE(retryResult);
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));
}

void FileRetryableEventUnittest::TestHandleMessageWithDifferentFunctions() {
    std::vector<file_secure_func> functionTypes = {TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION,
                                                   TRACEPOINT_FUNC_SECURITY_MMAP_FILE,
                                                   TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE,
                                                   TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION_WRITE,
                                                   TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION_READ};

    for (auto funcType : functionTypes) {
        file_data_t event = CreateStubFileEvent();
        event.func = funcType;

        auto cacheValue = std::make_shared<ProcessCacheValue>();
        cacheValue->SetContent<kProcessId>(StringView("1234"));
        cacheValue->SetContent<kKtime>(StringView("123456789"));
        mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

        FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

        bool result = fileEvent.HandleMessage();
        APSARA_TEST_TRUE(result);
        APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
        APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));

        mWrapper.mProcessCacheManager->mProcessCache.Clear();
    }
}

void FileRetryableEventUnittest::TestHandleMessageWithInvalidFunction() {
    file_data_t event = CreateStubFileEvent();
    event.func = TRACEPOINT_FUNC_SYS_WRITE;

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("123456789"));
    mWrapper.mProcessCacheManager->mProcessCache.AddCache({event.key.pid, event.key.ktime}, cacheValue);

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    APSARA_TEST_FALSE(result);
}

void FileRetryableEventUnittest::TestHandleMessageWithZeroPidKtime() {
    file_data_t event = CreateStubFileEvent();
    event.key.pid = 0;
    event.key.ktime = 0;

    FileRetryableEvent fileEvent(3, event, mWrapper.mProcessCacheManager->mProcessCache, *mCommonEventQueue);

    bool result = fileEvent.HandleMessage();
    // return true for zero pid/ktime, but a warning log will be generated
    APSARA_TEST_TRUE(result);
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFindProcess));
    APSARA_TEST_TRUE(fileEvent.IsTaskCompleted(FileRetryableEvent::kFlushEvent));
}

UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithProcessFound);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithProcessNotFound);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestOnRetry);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestOnDrop);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestRetryLimit);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithFlushFailure);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithDifferentFunctions);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithInvalidFunction);
UNIT_TEST_CASE(FileRetryableEventUnittest, TestHandleMessageWithZeroPidKtime);

UNIT_TEST_MAIN
