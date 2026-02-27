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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ProcParser.h"
#include "ebpf/plugin/ProcessCloneRetryableEvent.h"
#include "security/bpf_process_event_type.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessCloneRetryableEventUnittest : public ::testing::Test {
public:
    void SetUp() override {}

    void TearDown() override { mWrapper.Clear(); }

    void TestMsgCloneEventToProcessCacheValue();
    void TestMsgCloneEventToProcessCacheValueParentNotFound();

private:
    ProcessCacheManagerWrapper mWrapper;
};

void ProcessCloneRetryableEventUnittest::TestMsgCloneEventToProcessCacheValue() {
    // 测试缓存操作
    data_event_id parentkey{12345, 123456789};
    auto parentExecId = GenerateExecId(mWrapper.mProcessCacheManager->mHostName, parentkey.pid, parentkey.time);
    auto parentCacheValue = std::make_shared<ProcessCacheValue>();
    parentCacheValue->SetContent<kProcessId>(StringView("1234"));
    parentCacheValue->SetContent<kKtime>(StringView("123456789"));
    parentCacheValue->SetContent<kExecId>(parentExecId);
    parentCacheValue->SetContent<kUid>(StringView("1000"));
    parentCacheValue->SetContent<kBinary>(StringView("test_binary"));

    // 测试缓存更新
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(parentkey, parentCacheValue);

    msg_clone_event event{};
    event.tgid = 5678;
    event.ktime = 123456790;
    event.parent.pid = parentkey.pid;
    event.parent.ktime = parentkey.time;
    auto execId = GenerateExecId(mWrapper.mProcessCacheManager->mHostName, event.tgid, event.ktime);
    std::unique_ptr<ProcessCloneRetryableEvent> processCloneRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessCloneRetryableEvent(&event));
    std::shared_ptr<ProcessCacheValue> cacheValue(processCloneRetryableEvent->cloneProcessCacheValue(event));
    APSARA_TEST_TRUE(cacheValue != nullptr);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL((*cacheValue).Get<kProcessId>().to_string(), std::to_string(event.tgid));
    APSARA_TEST_EQUAL((*cacheValue).Get<kKtime>().to_string(), std::to_string(event.ktime));
    APSARA_TEST_EQUAL((*cacheValue).Get<kExecId>().to_string(), execId);
    APSARA_TEST_EQUAL((*cacheValue).Get<kUid>().to_string(), "1000");
    APSARA_TEST_EQUAL((*cacheValue).Get<kBinary>().to_string(), "test_binary");
}

void ProcessCloneRetryableEventUnittest::TestMsgCloneEventToProcessCacheValueParentNotFound() {
    msg_clone_event event{};
    event.tgid = 5678;
    event.ktime = 123456790;
    event.parent.pid = 1234;
    event.parent.pid = 123456789;
    std::unique_ptr<ProcessCloneRetryableEvent> processCloneRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessCloneRetryableEvent(&event));
    std::shared_ptr<ProcessCacheValue> cacheValue(processCloneRetryableEvent->cloneProcessCacheValue(event));
    APSARA_TEST_TRUE(cacheValue == nullptr);
}

UNIT_TEST_CASE(ProcessCloneRetryableEventUnittest, TestMsgCloneEventToProcessCacheValue);
UNIT_TEST_CASE(ProcessCloneRetryableEventUnittest, TestMsgCloneEventToProcessCacheValueParentNotFound);


UNIT_TEST_MAIN
