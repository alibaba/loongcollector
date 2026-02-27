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
#include "ebpf/plugin/ProcessSyncRetryableEvent.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcFsStub.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessSyncRetryableEventUnittest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override { mWrapper.Clear(); }

    void TestProcToProcessCacheValue();

private:
    ProcessCacheManagerWrapper mWrapper;
};

void ProcessSyncRetryableEventUnittest::TestProcToProcessCacheValue() {
    { // kernel thread
        Proc proc = CreateStubProc();
        FillKernelThreadProc(proc);
        std::unique_ptr<ProcessSyncRetryableEvent> processSyncRetryableEvent(
            mWrapper.mProcessCacheManager->CreateProcessSyncRetryableEvent(proc));
        auto cacheValuePtr = processSyncRetryableEvent->procToProcessCacheValue(proc);
        auto& cacheValue = *cacheValuePtr;
        APSARA_TEST_EQUAL(cacheValue.mPPid, proc.ppid);
        APSARA_TEST_EQUAL(cacheValue.mPKtime, proc.ktime);
        APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(proc.pid));
        APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(0U));
        APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
        APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(proc.ktime));
        APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), proc.cwd);
        APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), proc.comm);
    }
    { // cwd is root and invalid ppid
        Proc proc = CreateStubProc();
        FillRootCwdProc(proc);
        std::unique_ptr<ProcessSyncRetryableEvent> processSyncRetryableEvent(
            mWrapper.mProcessCacheManager->CreateProcessSyncRetryableEvent(proc));
        auto cacheValuePtr = processSyncRetryableEvent->procToProcessCacheValue(proc);
        auto& cacheValue = *cacheValuePtr;
        APSARA_TEST_EQUAL(cacheValue.mPPid, proc.ppid);
        APSARA_TEST_EQUAL(cacheValue.mPKtime, proc.ktime);
        APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(proc.pid));
        APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(0U));
        APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
        APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(proc.ktime));
        APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), proc.cwd);
        APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), proc.exe);
        APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), "/etc/host.conf /etc/resolv.conf");
        APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string());
        APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(), std::string());
        APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string());
        APSARA_TEST_EQUAL(cacheValue.Get<kContainerId>().to_string(), std::string());
    }
}

UNIT_TEST_CASE(ProcessSyncRetryableEventUnittest, TestProcToProcessCacheValue);

UNIT_TEST_MAIN
