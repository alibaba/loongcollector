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

#include <memory>

#include "ProcessCacheValue.h"
#include "ebpf/plugin/ProcessCache.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessCacheValueUnittest : public ::testing::Test {
protected:
    void TestCloneContents();
    void TestCloneContentsExcessive();
    void TestSetContent();
};

void ProcessCacheValueUnittest::TestCloneContents() {
    ProcessCacheValue v1;
    v1.mPPid = 1;
    v1.mPKtime = 2;
    v1.mRefCount = 3;
    v1.SetContent<ebpf::kArguments>(StringView("arg1 arg2 arg3"));
    std::unique_ptr<ProcessCacheValue> v2(v1.CloneContents());
    APSARA_TEST_EQUAL_FATAL(0U, v2->mPPid);
    APSARA_TEST_EQUAL_FATAL(0UL, v2->mPKtime);
    APSARA_TEST_EQUAL_FATAL(0, v2->mRefCount);
    APSARA_TEST_EQUAL_FATAL(StringView("arg1 arg2 arg3"), v2->Get<ebpf::kArguments>());
}

void ProcessCacheValueUnittest::TestCloneContentsExcessive() {
    ProcessCacheValue v1;
    v1.mPPid = 1;
    v1.mPKtime = 2;
    v1.mRefCount = 3;
    v1.SetContent<ebpf::kArguments>(StringView("arg1 arg2 arg3"));
    std::array<std::unique_ptr<ProcessCacheValue>, 10000> clones;
    for (size_t i = 0; i < clones.size(); ++i) {
        auto& v2 = clones[i];
        if (i == 0) {
            v2.reset(v1.CloneContents());
        } else {
            auto& parent = clones[i - 1];
            v2.reset(parent->CloneContents());
        }
        APSARA_TEST_EQUAL_FATAL(0U, v2->mPPid);
        APSARA_TEST_EQUAL_FATAL(0UL, v2->mPKtime);
        APSARA_TEST_EQUAL_FATAL(0, v2->mRefCount);
        APSARA_TEST_EQUAL_FATAL(StringView("arg1 arg2 arg3"), v2->Get<ebpf::kArguments>());
    }
    APSARA_TEST_NOT_EQUAL_FATAL(v1.GetSourceBuffer().get(), clones.back()->GetSourceBuffer().get());
}

void ProcessCacheValueUnittest::TestSetContent() {
    ProcessCacheValue v;
    v.SetContent<ebpf::kProcessId>(uint32_t(1000U));
    v.SetContent<ebpf::kKtime>(uint64_t(1000000000UL));
    APSARA_TEST_EQUAL_FATAL(StringView("1000"), v.Get<ebpf::kProcessId>());
    APSARA_TEST_EQUAL_FATAL(StringView("1000000000"), v.Get<ebpf::kKtime>());
}

UNIT_TEST_CASE(ProcessCacheValueUnittest, TestCloneContents);
UNIT_TEST_CASE(ProcessCacheValueUnittest, TestCloneContentsExcessive);
UNIT_TEST_CASE(ProcessCacheValueUnittest, TestSetContent);

class ProcessCacheUnittest : public ::testing::Test {
    ProcessCacheUnittest() : mProcParser("/"), mProcessCache(16, mProcParser) {}

protected:
    void TestAddCache();
    void TestRefCount();
    void TestClearExpiredCache();
    void TestForceShrink();
    void TestPrintDebugInfo();
    void TestCacheSize();
    void TestCacheContains();
    void TestClearCache();

private:
    ProcParser mProcParser;
    ProcessCache mProcessCache;
};

void ProcessCacheUnittest::TestAddCache() {
    data_event_id key{12345, 1234567890};
    std::shared_ptr<ProcessCacheValue> cacheValue = mProcessCache.Lookup(key);
    APSARA_TEST_TRUE(cacheValue == nullptr);

    cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("5678"));
    cacheValue->SetContent<kUid>(StringView("1000"));
    cacheValue->SetContent<kBinary>(StringView("test_binary"));

    // 测试缓存更新
    mProcessCache.AddCache(key, cacheValue);

    APSARA_TEST_TRUE(mProcessCache.Contains(key));

    // 测试缓存查找
    cacheValue = mProcessCache.Lookup(key);
    APSARA_TEST_TRUE(cacheValue != nullptr);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>(), StringView("1234"));
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>(), StringView("5678"));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>(), StringView("1000"));
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>(), StringView("test_binary"));
}

void ProcessCacheUnittest::TestRefCount() {
    data_event_id key{12345, 1234567890};
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("5678"));
    cacheValue->SetContent<kUid>(StringView("1000"));
    cacheValue->SetContent<kBinary>(StringView("test_binary"));
    mProcessCache.AddCache(key, cacheValue);
    cacheValue = mProcessCache.Lookup(key);
    APSARA_TEST_TRUE(cacheValue != nullptr);

    APSARA_TEST_EQUAL(1, cacheValue->mRefCount);
    mProcessCache.IncRef(key, cacheValue);
    APSARA_TEST_EQUAL(2, cacheValue->mRefCount);
    mProcessCache.DecRef(key, cacheValue);
    APSARA_TEST_EQUAL(1, cacheValue->mRefCount);
    mProcessCache.DecRef(key, cacheValue);
    APSARA_TEST_EQUAL(0, cacheValue->mRefCount);
}

void ProcessCacheUnittest::TestClearExpiredCache() {
    data_event_id key{12345, 1234567890};
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("5678"));
    cacheValue->SetContent<kUid>(StringView("1000"));
    cacheValue->SetContent<kBinary>(StringView("test_binary"));
    mProcessCache.AddCache(key, cacheValue);

    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kInUse, cacheValue->LifeStage());
    mProcessCache.DecRef(key, cacheValue);
    cacheValue = mProcessCache.Lookup(key);
    APSARA_TEST_TRUE(cacheValue != nullptr);
    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kDeletePending, cacheValue->LifeStage());
    APSARA_TEST_EQUAL(0, cacheValue->mRefCount);

    mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kDeleteReady, cacheValue->LifeStage());

    mProcessCache.IncRef(key, cacheValue);
    mProcessCache.DecRef(key, cacheValue);
    cacheValue = mProcessCache.Lookup(key);
    APSARA_TEST_TRUE(cacheValue != nullptr);
    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kDeletePending, cacheValue->LifeStage());
    APSARA_TEST_EQUAL(0, cacheValue->mRefCount);

    mProcessCache.ClearExpiredCache();
    mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kDeleted, cacheValue->LifeStage());
    APSARA_TEST_TRUE(mProcessCache.Lookup(key) == nullptr);

    mProcessCache.IncRef(key, cacheValue);
    mProcessCache.DecRef(key, cacheValue);
    APSARA_TEST_EQUAL(ProcessCacheValue::LifeStage::kDeleted, cacheValue->LifeStage());
}

void ProcessCacheUnittest::TestForceShrink() {
    for (int i = 0; i < 5; i++) {
        data_event_id key{static_cast<uint32_t>(i + 1000), static_cast<uint64_t>(i + 1000000)};
        auto cacheValue = std::make_shared<ProcessCacheValue>();
        cacheValue->SetContent<kProcessId>(StringView("1234"));
        cacheValue->SetContent<kKtime>(StringView("5678"));
        cacheValue->SetContent<kUid>(StringView("1000"));
        cacheValue->SetContent<kBinary>(StringView("test_binary"));
        mProcessCache.AddCache(key, cacheValue);
    }

    // mLastForceShrinkTimeSec is 0
    APSARA_TEST_EQUAL(5UL, mProcessCache.Size());
    mProcessCache.ForceShrink();
    APSARA_TEST_EQUAL(0UL, mProcessCache.Size());

    // interval within 2min
    for (int i = 0; i < 5; i++) {
        data_event_id key{static_cast<uint32_t>(i + 1000), static_cast<uint64_t>(i + 1000000)};
        auto cacheValue = std::make_shared<ProcessCacheValue>();
        cacheValue->SetContent<kProcessId>(StringView("1234"));
        cacheValue->SetContent<kKtime>(StringView("5678"));
        cacheValue->SetContent<kUid>(StringView("1000"));
        cacheValue->SetContent<kBinary>(StringView("test_binary"));
        mProcessCache.AddCache(key, cacheValue);
    }
    APSARA_TEST_EQUAL(5UL, mProcessCache.Size());
    mProcessCache.mLastForceShrinkTimeSec = TimeKeeper::GetInstance()->NowSec() - 110;
    mProcessCache.ForceShrink();
    APSARA_TEST_EQUAL(5UL, mProcessCache.Size());

    // interval exceeding 2 minutes
    mProcessCache.mLastForceShrinkTimeSec = TimeKeeper::GetInstance()->NowSec() - 130;
    mProcessCache.ForceShrink();

    APSARA_TEST_EQUAL(0UL, mProcessCache.Size());
}

void ProcessCacheUnittest::TestPrintDebugInfo() {
    data_event_id key{12345, 1234567890};
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    cacheValue->SetContent<kKtime>(StringView("5678"));
    cacheValue->SetContent<kUid>(StringView("1000"));
    cacheValue->SetContent<kBinary>(StringView("test_binary"));
    mProcessCache.AddCache(key, cacheValue);

    cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("4321"));
    cacheValue->SetContent<kKtime>(StringView("8765"));
    cacheValue->SetContent<kUid>(StringView("1001"));
    cacheValue->SetContent<kBinary>(StringView("dec_binary"));
    mProcessCache.AddCache(key, cacheValue);
    mProcessCache.DecRef(key, cacheValue);

    mProcessCache.PrintDebugInfo();
}

void ProcessCacheUnittest::TestCacheSize() {
    APSARA_TEST_EQUAL(0UL, mProcessCache.Size());

    data_event_id key{12345, 1234567890};
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    mProcessCache.AddCache(key, cacheValue);

    APSARA_TEST_EQUAL(1UL, mProcessCache.Size());

    data_event_id key2{12346, 1234567891};
    auto cacheValue2 = std::make_shared<ProcessCacheValue>();
    cacheValue2->SetContent<kProcessId>(StringView("1235"));
    mProcessCache.AddCache(key2, cacheValue2);

    APSARA_TEST_EQUAL(2UL, mProcessCache.Size());
}

void ProcessCacheUnittest::TestCacheContains() {
    data_event_id key{12345, 1234567890};
    APSARA_TEST_FALSE(mProcessCache.Contains(key));

    auto cacheValue = std::make_shared<ProcessCacheValue>();
    cacheValue->SetContent<kProcessId>(StringView("1234"));
    mProcessCache.AddCache(key, cacheValue);

    APSARA_TEST_TRUE(mProcessCache.Contains(key));

    data_event_id nonExistentKey{99999, 9999999999};
    APSARA_TEST_FALSE(mProcessCache.Contains(nonExistentKey));
}

void ProcessCacheUnittest::TestClearCache() {
    for (int i = 0; i < 3; i++) {
        data_event_id key{static_cast<uint32_t>(i + 1000), static_cast<uint64_t>(i + 1000000)};
        auto cacheValue = std::make_shared<ProcessCacheValue>();
        cacheValue->SetContent<kProcessId>(StringView("1234"));
        mProcessCache.AddCache(key, cacheValue);
    }

    APSARA_TEST_EQUAL(3UL, mProcessCache.Size());

    mProcessCache.Clear();

    APSARA_TEST_EQUAL(0UL, mProcessCache.Size());
}

UNIT_TEST_CASE(ProcessCacheUnittest, TestAddCache);
UNIT_TEST_CASE(ProcessCacheUnittest, TestRefCount);
UNIT_TEST_CASE(ProcessCacheUnittest, TestClearExpiredCache);
UNIT_TEST_CASE(ProcessCacheUnittest, TestForceShrink);
UNIT_TEST_CASE(ProcessCacheUnittest, TestPrintDebugInfo);
UNIT_TEST_CASE(ProcessCacheUnittest, TestCacheSize);
UNIT_TEST_CASE(ProcessCacheUnittest, TestCacheContains);
UNIT_TEST_CASE(ProcessCacheUnittest, TestClearCache);

UNIT_TEST_MAIN
