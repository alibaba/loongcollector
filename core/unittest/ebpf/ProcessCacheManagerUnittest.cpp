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

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "CommonDataEvent.h"
#include "Flags.h"
#include "ProcParser.h"
#include "common/memory/SourceBuffer.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/ProcessEvent.h"
#include "models/PipelineEventGroup.h"
#include "security/bpf_process_event_type.h"
#include "security/data_msg.h"
#include "security/msg_type.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/EBPFRawEventStub.h"
#include "unittest/ebpf/ProcFsStub.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

DECLARE_FLAG_INT32(ebpf_event_retry_limit);

class ProcessCacheManagerUnittest : public ::testing::Test {
public:
    void SetUp() override { INT32_FLAG(ebpf_event_retry_limit) = 2; }

    void TearDown() override { mWrapper.Clear(); }

    void TestListRunningProcs();
    // void TestWriteProcToBPFMap();

    void TestProcessEventCloneExecveExit();
    void TestProcessEventExecveExit();

    void TestProcessEventCloneExecveExitOutOfOrder();
    void TestProcessEventExecveExitOutOfOrder();

    void TestProcessEventCloneExecveExitOutOfOrder2();
    void TestProcessEventExecveExitOutOfOrder2();

    void TestProcessEventCloneExecveExitExitOutOfOrder();
    void TestProcessEventCloneExecveExitK8sMetaFail();

    void TestFinalizeProcessTags();

private:
    void testProcessEventCloneExecveExit(bool outOfOrder);
    void testProcessEventExecveExit(bool outOfOrder);
    ProcessCacheManagerWrapper mWrapper;
};

void ProcessCacheManagerUnittest::TestListRunningProcs() {
    ProcFsStub procFsStub(mWrapper.mProcDir);
    std::unordered_map<int, Proc> pidMap;
    for (uint32_t i = 1; i < 11; ++i) { // max i-1 is 9 so that container_id generated is ok
        Proc proc = CreateStubProc();
        proc.pid = i;
        proc.ppid = i - 1;
        proc.ktime = i * 1000000000UL;
        proc.pktime = (i - 1) * 1000000000UL;
        proc.auid = i + 500;
        proc.realUid = i + 500;
        proc.effectiveUid = i + 500;
        proc.savedUid = i + 500;
        proc.fsUid = i + 500;
        proc.realGid = i + 500;
        proc.effectiveGid = i + 500;
        proc.savedGid = i + 500;
        proc.fsGid = i + 500;
        proc.tid = proc.pid;
        proc.nspid = proc.pid;
        proc.flags = static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID);
        proc.cwd = "/home/user";
        proc.comm = "test program";
        proc.cmdline = proc.comm + '\0' + std::to_string(i) + '\0' + "arg2"; // \0 separated binary and args
        proc.container_id.clear();
        proc.container_id.resize(64, '0' + i - 1);
        proc.exe = "/usr/local/bin/test program";
        proc.effective = i;
        proc.inheritable = i + 1;
        proc.permitted = i + 2;
        proc.uts_ns = i + 400000000;
        proc.ipc_ns = i + 400000001;
        proc.mnt_ns = i + 400000002;
        proc.pid_ns = i + 400000003;
        proc.pid_for_children_ns = i + 400000004;
        proc.net_ns = i + 400000005;
        proc.time_ns = i + 400000006;
        proc.time_for_children_ns = i + 400000007;
        proc.cgroup_ns = i + 400000008;
        proc.user_ns = i + 400000009;
        pidMap[i] = proc;
    }
    { // kernal thread
        Proc proc = CreateStubProc();
        FillKernelThreadProc(proc);
        pidMap[proc.pid] = proc;
    }
    { // cwd is root and invalid ppid
        Proc proc = CreateStubProc();
        FillRootCwdProc(proc);
        pidMap[proc.pid] = proc;
    }
    for (auto& proc : pidMap) {
        procFsStub.CreatePidDir(proc.second);
    }
    auto procs = mWrapper.mProcessCacheManager->listRunningProcs();
    for (const auto& proc : procs) {
        const auto it = pidMap.find(proc->pid);
        APSARA_TEST_TRUE_FATAL(it != pidMap.end());
        const auto& expected = it->second;
        APSARA_TEST_EQUAL(proc->pid, expected.pid);
        APSARA_TEST_EQUAL(proc->ppid, expected.ppid);
        APSARA_TEST_EQUAL(proc->ktime, expected.ktime);
        APSARA_TEST_EQUAL(proc->auid, expected.auid);
        APSARA_TEST_EQUAL(proc->realUid, expected.realUid);
        APSARA_TEST_EQUAL(proc->effectiveUid, expected.effectiveUid);
        APSARA_TEST_EQUAL(proc->savedUid, expected.savedUid);
        APSARA_TEST_EQUAL(proc->fsUid, expected.fsUid);
        APSARA_TEST_EQUAL(proc->realGid, expected.realGid);
        APSARA_TEST_EQUAL(proc->effectiveGid, expected.effectiveGid);
        APSARA_TEST_EQUAL(proc->savedGid, expected.savedGid);
        APSARA_TEST_EQUAL(proc->fsGid, expected.fsGid);
        APSARA_TEST_EQUAL(proc->tid, expected.tid);
        APSARA_TEST_EQUAL(proc->nspid, expected.nspid);
        APSARA_TEST_EQUAL(proc->flags, expected.flags);
        APSARA_TEST_EQUAL(proc->cwd, expected.cwd);
        APSARA_TEST_EQUAL(proc->comm, expected.comm);
        APSARA_TEST_EQUAL(proc->cmdline, expected.cmdline);
        APSARA_TEST_EQUAL(proc->container_id, expected.container_id);
        APSARA_TEST_EQUAL(proc->exe, expected.exe);
        APSARA_TEST_EQUAL(proc->effective, expected.effective);
        APSARA_TEST_EQUAL(proc->inheritable, expected.inheritable);
        APSARA_TEST_EQUAL(proc->permitted, expected.permitted);
        APSARA_TEST_EQUAL(proc->uts_ns, expected.uts_ns);
        APSARA_TEST_EQUAL(proc->ipc_ns, expected.ipc_ns);
        APSARA_TEST_EQUAL(proc->mnt_ns, expected.mnt_ns);
        APSARA_TEST_EQUAL(proc->pid_ns, expected.pid_ns);
        APSARA_TEST_EQUAL(proc->pid_for_children_ns, expected.pid_for_children_ns);
        APSARA_TEST_EQUAL(proc->net_ns, expected.net_ns);
        APSARA_TEST_EQUAL(proc->time_ns, expected.time_ns);
        APSARA_TEST_EQUAL(proc->time_for_children_ns, expected.time_for_children_ns);
        APSARA_TEST_EQUAL(proc->cgroup_ns, expected.cgroup_ns);
        APSARA_TEST_EQUAL(proc->user_ns, expected.user_ns);
        const auto pit = pidMap.find(proc->ppid);
        if (pit != pidMap.end()) {
            const auto& pexpected = pit->second;
            APSARA_TEST_EQUAL(proc->ppid, pexpected.pid);
            APSARA_TEST_EQUAL(proc->pktime, pexpected.ktime);
            // APSARA_TEST_EQUAL(proc->pcmdline, pexpected.cmdline);
            // APSARA_TEST_EQUAL(proc->pexe, pexpected.exe);
            // APSARA_TEST_EQUAL(proc->pnspid, pexpected.nspid);
            // APSARA_TEST_EQUAL(proc->pflags, static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID);
        }
    }
}

namespace logtail::ebpf {
using EventVariant = std::variant<msg_exit, msg_execve_event, msg_clone_event>;
void HandleKernelProcessEvent(void* ctx, int cpu, void* data, uint32_t data_sz);
void ConsumeKernelProcessEvents(ProcessCacheManager* ctx, std::vector<EventVariant>& events) {
    ctx->EventCache().HandleEvents();
    for (auto& event : events) {
        std::visit([ctx](auto& e) { HandleKernelProcessEvent(ctx, 0, &e, 0U); }, event);
    }
    events.clear();
}

struct ProcessEventHash {
    size_t operator()(const ProcessEvent& event) const {
        return (uint64_t(event.mEventType) << 58) ^ (uint64_t(event.mPid) << 48) ^ event.mKtime;
    }
};

// 定义自定义相等比较函数
struct ProcessEventEqual {
    bool operator()(const ProcessEvent& lhs, const ProcessEvent& rhs) const {
        return lhs.mEventType == rhs.mEventType && lhs.mPid == rhs.mPid && lhs.mKtime == rhs.mKtime;
    }
};
} // namespace logtail::ebpf

/*
 * Before daemon exit
 * Lineage:      ┌------┐ ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       2             0                 2               0              1
 * After daemon exit
 * Lineage:      ┌------┐ ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       1             0                 1               0              1
 */
void ProcessCacheManagerUnittest::testProcessEventCloneExecveExit(bool outOfOrder) {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event daemonCloneEvent{};
    daemonCloneEvent.common.op = MSG_OP_CLONE;
    daemonCloneEvent.common.ktime = 30;
    daemonCloneEvent.tgid = 3;
    daemonCloneEvent.ktime = 30;
    daemonCloneEvent.parent.pid = shExecveEvent.process.pid;
    daemonCloneEvent.parent.ktime = shExecveEvent.process.ktime;

    msg_execve_event daemonExecveEvent = CreateStubExecveEvent();
    daemonExecveEvent.common.ktime = 31;
    daemonExecveEvent.process.pid = 3;
    daemonExecveEvent.process.ktime = 31;
    daemonExecveEvent.parent.pid = shExecveEvent.process.pid;
    daemonExecveEvent.parent.ktime = shExecveEvent.process.ktime;
    daemonExecveEvent.cleanup_key.pid = daemonCloneEvent.tgid;
    daemonExecveEvent.cleanup_key.ktime = daemonCloneEvent.ktime;
    constexpr char daemonBinary[] = "/usr/local/bin/daemon";
    memcpy(daemonExecveEvent.buffer + SIZEOF_EVENT, daemonBinary, sizeof(daemonBinary));
    daemonExecveEvent.process.size = sizeof(daemonBinary) + SIZEOF_EVENT;
    daemonExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.op = MSG_OP_CLONE;
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
    appExecveEvent.common.ktime = 41;
    appExecveEvent.process.pid = 4;
    appExecveEvent.process.ktime = 41;
    appExecveEvent.parent.pid = daemonExecveEvent.process.pid;
    appExecveEvent.parent.ktime = daemonExecveEvent.process.ktime;
    appExecveEvent.cleanup_key.pid = appCloneEvent.tgid;
    appExecveEvent.cleanup_key.ktime = appCloneEvent.ktime;
    constexpr char appBinary[] = "/usr/local/bin/app";
    memcpy(appExecveEvent.buffer + SIZEOF_EVENT, appBinary, sizeof(appBinary));
    appExecveEvent.process.size = sizeof(appBinary) + SIZEOF_EVENT;
    appExecveEvent.process.flags |= EVENT_CLONE;

    if (!outOfOrder) {
        rawEvents.emplace_back(shExecveEvent);
        rawEvents.emplace_back(daemonCloneEvent);
        rawEvents.emplace_back(daemonExecveEvent);
        rawEvents.emplace_back(appCloneEvent);
        rawEvents.emplace_back(appExecveEvent);
    } else {
        rawEvents.emplace_back(appExecveEvent);
        rawEvents.emplace_back(appCloneEvent);
        rawEvents.emplace_back(daemonExecveEvent);
        rawEvents.emplace_back(daemonCloneEvent);
        rawEvents.emplace_back(shExecveEvent);
        ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
        ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    }

    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check cache
    APSARA_TEST_EQUAL(initProc->mRefCount, 2);

    auto shProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 2);

    auto daemonClone = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(daemonClone != nullptr);
    APSARA_TEST_EQUAL((*daemonClone).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(daemonClone->mRefCount, 0);

    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 2);

    auto appClone
        = mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(appClone != nullptr);
    APSARA_TEST_EQUAL((*appClone).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);

    auto appProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(5UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {daemonCloneEvent.tgid,
         daemonCloneEvent.ktime,
         KernelEventType::PROCESS_CLONE_EVENT,
         daemonCloneEvent.common.ktime},
        {daemonExecveEvent.process.pid,
         daemonExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         daemonExecveEvent.common.ktime},
        {appCloneEvent.tgid, appCloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, appCloneEvent.common.ktime},
        {appExecveEvent.process.pid,
         appExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         appExecveEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;
    rawEvents.emplace_back(daemonExitEvent);

    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check cache
    APSARA_TEST_EQUAL(shProc->mRefCount, 1);
    APSARA_TEST_EQUAL(daemonClone->mRefCount, 0);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(1UL, eventCount);
    auto& event6 = (ProcessExitEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXIT_EVENT, event6.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.pid, event6.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.ktime, event6.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.common.ktime, event6.mTimestamp);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.code, event6.mExitCode);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.tid, event6.mExitTid);

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL_FATAL(4UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime})
                          .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime})
            .get());
    APSARA_TEST_EQUAL(initProc->mRefCount, 2);
}

void ProcessCacheManagerUnittest::TestProcessEventCloneExecveExit() {
    testProcessEventCloneExecveExit(false);
}

void ProcessCacheManagerUnittest::TestProcessEventCloneExecveExitOutOfOrder() {
    testProcessEventCloneExecveExit(true);
}


/*
 * Before daemon exit
 * Lineage:      ┌------┐ ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       2             0                 2               0              1
 * After daemon exit
 * Lineage:      ┌------┐ ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       1             0                 1               0              1
 */
void ProcessCacheManagerUnittest::TestProcessEventCloneExecveExitOutOfOrder2() {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event daemonCloneEvent{};
    daemonCloneEvent.common.op = MSG_OP_CLONE;
    daemonCloneEvent.common.ktime = 30;
    daemonCloneEvent.tgid = 3;
    daemonCloneEvent.ktime = 30;
    daemonCloneEvent.parent.pid = shExecveEvent.process.pid;
    daemonCloneEvent.parent.ktime = shExecveEvent.process.ktime;

    msg_execve_event daemonExecveEvent = CreateStubExecveEvent();
    daemonExecveEvent.common.ktime = 31;
    daemonExecveEvent.process.pid = 3;
    daemonExecveEvent.process.ktime = 31;
    daemonExecveEvent.parent.pid = shExecveEvent.process.pid;
    daemonExecveEvent.parent.ktime = shExecveEvent.process.ktime;
    daemonExecveEvent.cleanup_key.pid = daemonCloneEvent.tgid;
    daemonExecveEvent.cleanup_key.ktime = daemonCloneEvent.ktime;
    constexpr char daemonBinary[] = "/usr/local/bin/daemon";
    memcpy(daemonExecveEvent.buffer + SIZEOF_EVENT, daemonBinary, sizeof(daemonBinary));
    daemonExecveEvent.process.size = sizeof(daemonBinary) + SIZEOF_EVENT;
    daemonExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.op = MSG_OP_CLONE;
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
    appExecveEvent.common.ktime = 41;
    appExecveEvent.process.pid = 4;
    appExecveEvent.process.ktime = 41;
    appExecveEvent.parent.pid = daemonExecveEvent.process.pid;
    appExecveEvent.parent.ktime = daemonExecveEvent.process.ktime;
    appExecveEvent.cleanup_key.pid = appCloneEvent.tgid;
    appExecveEvent.cleanup_key.ktime = appCloneEvent.ktime;
    constexpr char appBinary[] = "/usr/local/bin/app";
    memcpy(appExecveEvent.buffer + SIZEOF_EVENT, appBinary, sizeof(appBinary));
    appExecveEvent.process.size = sizeof(appBinary) + SIZEOF_EVENT;
    appExecveEvent.process.flags |= EVENT_CLONE;

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;

    rawEvents.emplace_back(shExecveEvent);
    rawEvents.emplace_back(daemonExitEvent);
    rawEvents.emplace_back(appExecveEvent);
    rawEvents.emplace_back(daemonExecveEvent);
    rawEvents.emplace_back(daemonCloneEvent);
    rawEvents.emplace_back(appCloneEvent);
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(6UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {daemonCloneEvent.tgid,
         daemonCloneEvent.ktime,
         KernelEventType::PROCESS_CLONE_EVENT,
         daemonCloneEvent.common.ktime},
        {daemonExecveEvent.process.pid,
         daemonExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         daemonExecveEvent.common.ktime},
        {appCloneEvent.tgid, appCloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, appCloneEvent.common.ktime},
        {appExecveEvent.process.pid,
         appExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         appExecveEvent.common.ktime},
        {daemonExitEvent.current.pid,
         daemonExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         daemonExitEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL_FATAL(4UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime})
                          .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime})
            .get());

    auto shProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 1);

    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);

    auto appProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    APSARA_TEST_EQUAL(initProc->mRefCount, 2);
}

/*
 * Before daemon exit
 * Lineage:      ┌------┐ ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       1       0             1                 2              0              1
 * After daemon exit
 * Lineage:      ┌------┐ ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       1       0             0                 1              0              1
 */
void ProcessCacheManagerUnittest::testProcessEventExecveExit(bool outOfOrder) {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_execve_event bashExecveEvent = CreateStubExecveEvent();
    bashExecveEvent.common.ktime = 21;
    bashExecveEvent.process.pid = 2;
    bashExecveEvent.process.ktime = 21;
    bashExecveEvent.parent.pid = initProcKey.pid;
    bashExecveEvent.parent.ktime = initProcKey.time;
    bashExecveEvent.cleanup_key.pid = shExecveEvent.process.pid;
    bashExecveEvent.cleanup_key.ktime = shExecveEvent.process.ktime;
    constexpr char bashBinary[] = "/usr/bin/bash";
    memcpy(bashExecveEvent.buffer + SIZEOF_EVENT, bashBinary, sizeof(bashBinary));
    bashExecveEvent.process.size = sizeof(bashBinary) + SIZEOF_EVENT;
    bashExecveEvent.process.flags &= ~EVENT_CLONE;

    msg_execve_event daemonExecveEvent = CreateStubExecveEvent();
    daemonExecveEvent.common.ktime = 22;
    daemonExecveEvent.process.pid = 2;
    daemonExecveEvent.process.ktime = 22;
    daemonExecveEvent.parent.pid = initProcKey.pid;
    daemonExecveEvent.parent.ktime = initProcKey.time;
    daemonExecveEvent.cleanup_key.pid = bashExecveEvent.process.pid;
    daemonExecveEvent.cleanup_key.ktime = bashExecveEvent.process.ktime;
    constexpr char daemonBinary[] = "/usr/local/bin/daemon";
    memcpy(daemonExecveEvent.buffer + SIZEOF_EVENT, daemonBinary, sizeof(daemonBinary));
    daemonExecveEvent.process.size = sizeof(daemonBinary) + SIZEOF_EVENT;
    daemonExecveEvent.process.flags &= ~EVENT_CLONE;

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.op = MSG_OP_CLONE;
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
    appExecveEvent.common.ktime = 41;
    appExecveEvent.process.pid = 4;
    appExecveEvent.process.ktime = 41;
    appExecveEvent.parent.pid = daemonExecveEvent.process.pid;
    appExecveEvent.parent.ktime = daemonExecveEvent.process.ktime;
    appExecveEvent.cleanup_key.pid = appCloneEvent.tgid;
    appExecveEvent.cleanup_key.ktime = appCloneEvent.ktime;
    constexpr char appBinary[] = "/usr/local/bin/app";
    memcpy(appExecveEvent.buffer + SIZEOF_EVENT, appBinary, sizeof(appBinary));
    appExecveEvent.process.size = sizeof(appBinary) + SIZEOF_EVENT;
    appExecveEvent.process.flags |= EVENT_CLONE;

    if (!outOfOrder) {
        rawEvents.emplace_back(shExecveEvent);
        rawEvents.emplace_back(bashExecveEvent);
        rawEvents.emplace_back(daemonExecveEvent);
        rawEvents.emplace_back(appCloneEvent);
        rawEvents.emplace_back(appExecveEvent);
    } else {
        rawEvents.emplace_back(appExecveEvent);
        rawEvents.emplace_back(appCloneEvent);
        rawEvents.emplace_back(daemonExecveEvent);
        rawEvents.emplace_back(bashExecveEvent);
        rawEvents.emplace_back(shExecveEvent);
        ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
        ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    }
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check cache
    APSARA_TEST_EQUAL(initProc->mRefCount, 1);

    auto shProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 0);

    auto bashProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{bashExecveEvent.process.pid, bashExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(bashProc != nullptr);
    APSARA_TEST_EQUAL((*bashProc).Get<kBinary>().to_string(), bashBinary);
    APSARA_TEST_EQUAL(bashProc->mRefCount, 1);

    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 2);

    auto appClone
        = mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(appClone != nullptr);
    APSARA_TEST_EQUAL((*appClone).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);

    auto appProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(5UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {bashExecveEvent.process.pid,
         bashExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         bashExecveEvent.common.ktime},
        {daemonExecveEvent.process.pid,
         daemonExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         daemonExecveEvent.common.ktime},
        {appCloneEvent.tgid, appCloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, appCloneEvent.common.ktime},
        {appExecveEvent.process.pid,
         appExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         appExecveEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;
    rawEvents.emplace_back(daemonExitEvent);

    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check cache
    APSARA_TEST_EQUAL(shProc->mRefCount, 0);
    APSARA_TEST_EQUAL(bashProc->mRefCount, 0);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(1UL, eventCount);
    auto& event6 = (ProcessExitEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXIT_EVENT, event6.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.pid, event6.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.ktime, event6.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.common.ktime, event6.mTimestamp);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.code, event6.mExitCode);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.tid, event6.mExitTid);

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL_FATAL(3UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime})
                          .get());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{bashExecveEvent.process.pid, bashExecveEvent.process.ktime})
                          .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime})
            .get());
    APSARA_TEST_EQUAL(initProc->mRefCount, 1);
}

void ProcessCacheManagerUnittest::TestProcessEventExecveExit() {
    testProcessEventExecveExit(false);
}
void ProcessCacheManagerUnittest::TestProcessEventExecveExitOutOfOrder() {
    testProcessEventExecveExit(true);
}

/*
 * Before daemon exit
 * Lineage:      ┌------┐ ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       1       0             1                 2              0              1
 * After daemon exit
 * Lineage:      ┌------┐ ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       1       0             0                 1              0              1
 */
void ProcessCacheManagerUnittest::TestProcessEventExecveExitOutOfOrder2() {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_execve_event bashExecveEvent = CreateStubExecveEvent();
    bashExecveEvent.common.ktime = 21;
    bashExecveEvent.process.pid = 2;
    bashExecveEvent.process.ktime = 21;
    bashExecveEvent.parent.pid = initProcKey.pid;
    bashExecveEvent.parent.ktime = initProcKey.time;
    bashExecveEvent.cleanup_key.pid = shExecveEvent.process.pid;
    bashExecveEvent.cleanup_key.ktime = shExecveEvent.process.ktime;
    constexpr char bashBinary[] = "/usr/bin/bash";
    memcpy(bashExecveEvent.buffer + SIZEOF_EVENT, bashBinary, sizeof(bashBinary));
    bashExecveEvent.process.size = sizeof(bashBinary) + SIZEOF_EVENT;
    bashExecveEvent.process.flags &= ~EVENT_CLONE;

    msg_execve_event daemonExecveEvent = CreateStubExecveEvent();
    daemonExecveEvent.common.ktime = 22;
    daemonExecveEvent.process.pid = 2;
    daemonExecveEvent.process.ktime = 22;
    daemonExecveEvent.parent.pid = initProcKey.pid;
    daemonExecveEvent.parent.ktime = initProcKey.time;
    daemonExecveEvent.cleanup_key.pid = bashExecveEvent.process.pid;
    daemonExecveEvent.cleanup_key.ktime = bashExecveEvent.process.ktime;
    constexpr char daemonBinary[] = "/usr/local/bin/daemon";
    memcpy(daemonExecveEvent.buffer + SIZEOF_EVENT, daemonBinary, sizeof(daemonBinary));
    daemonExecveEvent.process.size = sizeof(daemonBinary) + SIZEOF_EVENT;
    daemonExecveEvent.process.flags &= ~EVENT_CLONE;

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.op = MSG_OP_CLONE;
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
    appExecveEvent.common.ktime = 41;
    appExecveEvent.process.pid = 4;
    appExecveEvent.process.ktime = 41;
    appExecveEvent.parent.pid = daemonExecveEvent.process.pid;
    appExecveEvent.parent.ktime = daemonExecveEvent.process.ktime;
    appExecveEvent.cleanup_key.pid = appCloneEvent.tgid;
    appExecveEvent.cleanup_key.ktime = appCloneEvent.ktime;
    constexpr char appBinary[] = "/usr/local/bin/app";
    memcpy(appExecveEvent.buffer + SIZEOF_EVENT, appBinary, sizeof(appBinary));
    appExecveEvent.process.size = sizeof(appBinary) + SIZEOF_EVENT;
    appExecveEvent.process.flags |= EVENT_CLONE;

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;

    rawEvents.emplace_back(shExecveEvent);
    rawEvents.emplace_back(daemonExitEvent);
    rawEvents.emplace_back(appCloneEvent);
    rawEvents.emplace_back(daemonExecveEvent);
    rawEvents.emplace_back(bashExecveEvent);
    rawEvents.emplace_back(appExecveEvent);
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(6UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {bashExecveEvent.process.pid,
         bashExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         bashExecveEvent.common.ktime},
        {daemonExecveEvent.process.pid,
         daemonExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         daemonExecveEvent.common.ktime},
        {appCloneEvent.tgid, appCloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, appCloneEvent.common.ktime},
        {appExecveEvent.process.pid,
         appExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         appExecveEvent.common.ktime},
        {daemonExitEvent.current.pid,
         daemonExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         daemonExitEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    APSARA_TEST_EQUAL_FATAL(3UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);
    auto appProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);
    APSARA_TEST_EQUAL(initProc->mRefCount, 1);
}

void ProcessCacheManagerUnittest::TestFinalizeProcessTags() {
    // 创建进程事件
    data_event_id key{1234, 5678};
    auto execveEvent = std::make_shared<ProcessCacheValue>();
    execveEvent->SetContent<kProcessId>(StringView("1234"));
    execveEvent->SetContent<kKtime>(StringView("5678"));
    execveEvent->SetContent<kUid>(StringView("1000"));
    execveEvent->SetContent<kBinary>(StringView("test_binary"));
    execveEvent->mPPid = 2345;
    execveEvent->mPKtime = 6789;

    // parent
    data_event_id pKey{2345, 6789};
    auto pExecveEvent = std::make_shared<ProcessCacheValue>();
    pExecveEvent->SetContent<kProcessId>(StringView("2345"));
    pExecveEvent->SetContent<kKtime>(StringView("6789"));
    pExecveEvent->SetContent<kUid>(StringView("1000"));
    pExecveEvent->SetContent<kBinary>(StringView("test_binary_parent"));

    // 更新缓存
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(key, execveEvent);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(pKey, pExecveEvent);

    // 测试进程标签生成
    PipelineEventGroup sharedEventGroup(std::make_shared<SourceBuffer>());
    auto sharedEvent = sharedEventGroup.CreateLogEvent();
    APSARA_TEST_TRUE(mWrapper.mProcessCacheManager->FinalizeProcessTags(key.pid, key.time, *sharedEvent));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kProcessId.LogKey()), StringView("1234"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kKtime.LogKey()), StringView("5678"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kUid.LogKey()), StringView("1000"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kBinary.LogKey()), StringView("test_binary"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentProcessId.LogKey()), StringView("2345"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentKtime.LogKey()), StringView("6789"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentUid.LogKey()), StringView("1000"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentBinary.LogKey()), StringView("test_binary_parent"));
}

/*
 * Before daemon and app exit
 * Lineage:      ┌------┐ ┌-----------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       2             2               0              1
 * After daemon and app exit
 * Lineage:      ┌------┐ ┌------------┐ ┌-----------------------------┐
 * CallChain: (init)   (sh) -clone- (daemon) -clone- (app) -execve- (app)
 * RefCnt:       2       1             0               0              0
 */
void ProcessCacheManagerUnittest::TestProcessEventCloneExecveExitExitOutOfOrder() {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event daemonCloneEvent{};
    daemonCloneEvent.common.op = MSG_OP_CLONE;
    daemonCloneEvent.common.ktime = 30;
    daemonCloneEvent.tgid = 3;
    daemonCloneEvent.ktime = 30;
    daemonCloneEvent.parent.pid = shExecveEvent.process.pid;
    daemonCloneEvent.parent.ktime = shExecveEvent.process.ktime;

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.op = MSG_OP_CLONE;
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonCloneEvent.tgid;
    appCloneEvent.parent.ktime = daemonCloneEvent.ktime;

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
    appExecveEvent.common.ktime = 41;
    appExecveEvent.process.pid = 4;
    appExecveEvent.process.ktime = 41;
    appExecveEvent.parent.pid = daemonCloneEvent.tgid;
    appExecveEvent.parent.ktime = daemonCloneEvent.ktime;
    appExecveEvent.cleanup_key.pid = appCloneEvent.tgid;
    appExecveEvent.cleanup_key.ktime = appCloneEvent.ktime;
    constexpr char appBinary[] = "/usr/local/bin/app";
    memcpy(appExecveEvent.buffer + SIZEOF_EVENT, appBinary, sizeof(appBinary));
    appExecveEvent.process.size = sizeof(appBinary) + SIZEOF_EVENT;
    appExecveEvent.process.flags |= EVENT_CLONE;

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonCloneEvent.tgid;
    daemonExitEvent.current.ktime = daemonCloneEvent.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;

    // app exit
    msg_exit appExitEvent{};
    appExitEvent.common.op = MSG_OP_EXIT;
    appExitEvent.common.ktime = 60;
    appExitEvent.current.pid = appExecveEvent.process.pid;
    appExitEvent.current.ktime = appExecveEvent.process.ktime;
    appExitEvent.info.code = -1;
    appExitEvent.info.tid = 3;

    rawEvents.emplace_back(shExecveEvent);
    rawEvents.emplace_back(appCloneEvent);
    rawEvents.emplace_back(appExecveEvent);
    rawEvents.emplace_back(daemonCloneEvent);
    rawEvents.emplace_back(appExitEvent);
    rawEvents.emplace_back(daemonExitEvent);
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry for an extra round
    // shExecveEvent is done
    // appCloneEvent cannot find parent
    // appExecveEvent write cache but cannot inc ref parent
    // daemonCloneEvent is done
    // appExitEvent is done, will dec app and daemon ref to 0
    // daemonExitEvent is done, will dec daemon ref to -1
    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL((*daemonProc).mPPid, shExecveEvent.process.pid);
    APSARA_TEST_EQUAL((*daemonProc).mPKtime, shExecveEvent.process.ktime);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, -1);

    auto appProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL((*appProc).mPPid, daemonCloneEvent.tgid);
    APSARA_TEST_EQUAL((*appProc).mPKtime, daemonCloneEvent.ktime);
    APSARA_TEST_EQUAL(appProc->mRefCount, 0);

    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents);
    // appCloneEvent is done, will recover daemon ref to 0
    // appExecveEvent is done

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(6UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {daemonCloneEvent.tgid,
         daemonCloneEvent.ktime,
         KernelEventType::PROCESS_CLONE_EVENT,
         daemonCloneEvent.common.ktime},
        {appCloneEvent.tgid, appCloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, appCloneEvent.common.ktime},
        {appExecveEvent.process.pid,
         appExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         appExecveEvent.common.ktime},
        {daemonExitEvent.current.pid,
         daemonExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         daemonExitEvent.common.ktime},
        {appExitEvent.current.pid,
         appExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         appExitEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();

    APSARA_TEST_EQUAL_FATAL(2UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime})
                          .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime})
            .get());

    auto shProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 1);

    APSARA_TEST_EQUAL(initProc->mRefCount, 2);
}

/*
 * Before daemon and app exit                          0               1
 * Lineage:                             ┌-----clone- (app1) -execve- (app1)
 *               ┌------┐ ┌-----------┐ ├------------------------------┤
 * CallChain: (init)   (sh) -clone- (daemon) -clone- (app2) -execve- (app2)
 * RefCnt:       2       2             3               0               1
 * After daemon and app exit                           0               0
 * Lineage:                             ┌-----clone- (app1) -execve- (app1)
 *              ┌------┐ ┌------------┐ ├------------------------------┤
 * CallChain: (init)   (sh) -clone- (daemon) -clone- (app2) -execve- (app2)
 * RefCnt:       2       1             0               0               0
 */
void ProcessCacheManagerUnittest::TestProcessEventCloneExecveExitK8sMetaFail() {
    mWrapper.mProcessCacheManager->MarkProcessEventFlushStatus(true);
    std::vector<EventVariant> rawEvents;
    data_event_id initProcKey{1, 0};
    std::string containerId = "793d6a33c08f245724b198e72d9ffab0acab289ef8ab81b573b011b2d5738870";
    auto initProc = std::make_shared<ProcessCacheValue>();
    initProc->SetContent<kProcessId>(initProcKey.pid);
    initProc->SetContent<kKtime>(initProcKey.time);
    initProc->SetContent<kContainerId>(containerId);
    mWrapper.mProcessCacheManager->mProcessCache.AddCache(initProcKey, initProc);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = initProcKey.pid;
    shExecveEvent.parent.ktime = initProcKey.time;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event daemonCloneEvent{};
    daemonCloneEvent.common.op = MSG_OP_CLONE;
    daemonCloneEvent.common.ktime = 30;
    daemonCloneEvent.tgid = 3;
    daemonCloneEvent.ktime = 30;
    daemonCloneEvent.parent.pid = shExecveEvent.process.pid;
    daemonCloneEvent.parent.ktime = shExecveEvent.process.ktime;

    msg_clone_event app1CloneEvent{};
    app1CloneEvent.common.op = MSG_OP_CLONE;
    app1CloneEvent.common.ktime = 40;
    app1CloneEvent.tgid = 4;
    app1CloneEvent.ktime = 40;
    app1CloneEvent.parent.pid = daemonCloneEvent.tgid;
    app1CloneEvent.parent.ktime = daemonCloneEvent.ktime;

    msg_execve_event app1ExecveEvent = CreateStubExecveEvent();
    app1ExecveEvent.common.ktime = 41;
    app1ExecveEvent.process.pid = 4;
    app1ExecveEvent.process.ktime = 41;
    app1ExecveEvent.parent.pid = daemonCloneEvent.tgid;
    app1ExecveEvent.parent.ktime = daemonCloneEvent.ktime;
    app1ExecveEvent.cleanup_key.pid = app1CloneEvent.tgid;
    app1ExecveEvent.cleanup_key.ktime = app1CloneEvent.ktime;
    constexpr char app1Binary[] = "/usr/local/bin/app1";
    memcpy(app1ExecveEvent.buffer + SIZEOF_EVENT, app1Binary, sizeof(app1Binary));
    app1ExecveEvent.process.size = sizeof(app1Binary) + SIZEOF_EVENT;
    app1ExecveEvent.process.flags |= EVENT_CLONE;

    msg_clone_event app2CloneEvent{};
    app2CloneEvent.common.op = MSG_OP_CLONE;
    app2CloneEvent.common.ktime = 50;
    app2CloneEvent.tgid = 5;
    app2CloneEvent.ktime = 50;
    app2CloneEvent.parent.pid = daemonCloneEvent.tgid;
    app2CloneEvent.parent.ktime = daemonCloneEvent.ktime;

    msg_execve_event app2ExecveEvent = CreateStubExecveEvent();
    app2ExecveEvent.common.ktime = 51;
    app2ExecveEvent.process.pid = 5;
    app2ExecveEvent.process.ktime = 51;
    app2ExecveEvent.parent.pid = daemonCloneEvent.tgid;
    app2ExecveEvent.parent.ktime = daemonCloneEvent.ktime;
    app2ExecveEvent.cleanup_key.pid = app2CloneEvent.tgid;
    app2ExecveEvent.cleanup_key.ktime = app2CloneEvent.ktime;
    constexpr char app2Binary[] = "/usr/local/bin/app2";
    memcpy(app2ExecveEvent.buffer + SIZEOF_EVENT, app2Binary, sizeof(app2Binary));
    app2ExecveEvent.process.size = sizeof(app2Binary) + SIZEOF_EVENT;
    app2ExecveEvent.process.flags |= EVENT_CLONE;

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.op = MSG_OP_EXIT;
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonCloneEvent.tgid;
    daemonExitEvent.current.ktime = daemonCloneEvent.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;

    // app exit
    msg_exit app1ExitEvent{};
    app1ExitEvent.common.op = MSG_OP_EXIT;
    app1ExitEvent.common.ktime = 60;
    app1ExitEvent.current.pid = app1ExecveEvent.process.pid;
    app1ExitEvent.current.ktime = app1ExecveEvent.process.ktime;
    app1ExitEvent.info.code = -1;
    app1ExitEvent.info.tid = 3;

    msg_exit app2ExitEvent{};
    app2ExitEvent.common.op = MSG_OP_EXIT;
    app2ExitEvent.common.ktime = 60;
    app2ExitEvent.current.pid = app2ExecveEvent.process.pid;
    app2ExitEvent.current.ktime = app2ExecveEvent.process.ktime;
    app2ExitEvent.info.code = -1;
    app2ExitEvent.info.tid = 3;

    rawEvents.emplace_back(shExecveEvent);
    rawEvents.emplace_back(app1CloneEvent);
    rawEvents.emplace_back(app1ExecveEvent);
    rawEvents.emplace_back(daemonCloneEvent);
    rawEvents.emplace_back(app2ExecveEvent);
    rawEvents.emplace_back(app2CloneEvent);
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times

    // shExecveEvent write cache but pending on k8s meta
    // app1CloneEvent cannot find parent
    // app1ExecveEvent write cache but cannot inc ref parent, will cleanup app1CloneEvent
    // daemonCloneEvent write cache but pending on k8s meta
    // app2ExecveEvent write cache and inc parent daemon ref, will cleanup app2CloneEvent
    // app2CloneEvent write cache and inc parent daemon ref, but pending on k8s meta
    auto daemonProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL((*daemonProc).mPPid, shExecveEvent.process.pid);
    APSARA_TEST_EQUAL((*daemonProc).mPKtime, shExecveEvent.process.ktime);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 3);

    auto app1Proc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{app1ExecveEvent.process.pid, app1ExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(app1Proc != nullptr);
    APSARA_TEST_EQUAL((*app1Proc).Get<kBinary>().to_string(), app1Binary);
    APSARA_TEST_EQUAL((*app1Proc).mPPid, daemonCloneEvent.tgid);
    APSARA_TEST_EQUAL((*app1Proc).mPKtime, daemonCloneEvent.ktime);
    APSARA_TEST_EQUAL(app1Proc->mRefCount, 1);

    auto app2Proc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{app2ExecveEvent.process.pid, app2ExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(app2Proc != nullptr);
    APSARA_TEST_EQUAL((*app2Proc).Get<kBinary>().to_string(), app2Binary);
    APSARA_TEST_EQUAL((*app2Proc).mPPid, daemonCloneEvent.tgid);
    APSARA_TEST_EQUAL((*app2Proc).mPKtime, daemonCloneEvent.ktime);
    APSARA_TEST_EQUAL(app2Proc->mRefCount, 1);

    rawEvents.emplace_back(app1ExitEvent);
    rawEvents.emplace_back(app2ExitEvent);
    rawEvents.emplace_back(daemonExitEvent);
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times
    ConsumeKernelProcessEvents(mWrapper.mProcessCacheManager.get(), rawEvents); // retry > retry limit times

    // app1ExitEvent is pending on k8s meta, will dec app1 and daemon ref
    // app2ExitEvent is pending on k8s meta, will dec app2 and daemon ref
    // daemonExitEvent is pending on k8s meta, will dec daemon ref
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 0);
    APSARA_TEST_EQUAL(app1Proc->mRefCount, 0);
    APSARA_TEST_EQUAL(app2Proc->mRefCount, 0);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mWrapper.mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(9UL, eventCount);

    std::unordered_set<ProcessEvent, ProcessEventHash, ProcessEventEqual> expectedEvents{
        {shExecveEvent.process.pid,
         shExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         shExecveEvent.common.ktime},
        {daemonCloneEvent.tgid,
         daemonCloneEvent.ktime,
         KernelEventType::PROCESS_CLONE_EVENT,
         daemonCloneEvent.common.ktime},
        {app1CloneEvent.tgid, app1CloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, app1CloneEvent.common.ktime},
        {app1ExecveEvent.process.pid,
         app1ExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         app1ExecveEvent.common.ktime},
        {daemonExitEvent.current.pid,
         daemonExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         daemonExitEvent.common.ktime},
        {app1ExitEvent.current.pid,
         app1ExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         app1ExitEvent.common.ktime},
        {app2CloneEvent.tgid, app2CloneEvent.ktime, KernelEventType::PROCESS_CLONE_EVENT, app2CloneEvent.common.ktime},
        {app2ExecveEvent.process.pid,
         app2ExecveEvent.process.ktime,
         KernelEventType::PROCESS_EXECVE_EVENT,
         app2ExecveEvent.common.ktime},
        {app2ExitEvent.current.pid,
         app2ExitEvent.current.ktime,
         KernelEventType::PROCESS_EXIT_EVENT,
         app2ExitEvent.common.ktime}};

    for (size_t i = 0; i < eventCount; ++i) {
        auto event = static_cast<ProcessEvent&>(*items[i]);
        auto it = expectedEvents.find(event);

        APSARA_TEST_NOT_EQUAL_FATAL(expectedEvents.end(), it);
        APSARA_TEST_EQUAL_FATAL(event.mTimestamp, it->mTimestamp);
    }

    // zero ref processes should be cleared
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();
    mWrapper.mProcessCacheManager->mProcessCache.ClearExpiredCache();

    APSARA_TEST_EQUAL_FATAL(2UL, mWrapper.mProcessCacheManager->mProcessCache.Size());
    APSARA_TEST_EQUAL(nullptr,
                      mWrapper.mProcessCacheManager->mProcessCache
                          .Lookup(data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime})
                          .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{app1CloneEvent.tgid, app1CloneEvent.ktime})
            .get());
    APSARA_TEST_EQUAL(
        nullptr,
        mWrapper.mProcessCacheManager->mProcessCache.Lookup(data_event_id{app2CloneEvent.tgid, app2CloneEvent.ktime})
            .get());
    auto shProc = mWrapper.mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 1);

    APSARA_TEST_EQUAL(initProc->mRefCount, 2);
}

// void ProcessCacheManagerUnittest::TestPollPerfBuffers() {
//     // 初始化ProcessCacheManager
//     APSARA_TEST_TRUE(mProcessCacheManager->Init());

//     // 测试PerfBuffer轮询
//     mProcessCacheManager->pollPerfBuffers();

//     // 测试停止操作
//     mProcessCacheManager->Stop();
// }

UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestListRunningProcs);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventCloneExecveExit);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventExecveExit);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventCloneExecveExitOutOfOrder);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventExecveExitOutOfOrder);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventCloneExecveExitOutOfOrder2);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventExecveExitOutOfOrder2);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventCloneExecveExitExitOutOfOrder);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcessEventCloneExecveExitK8sMetaFail);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestFinalizeProcessTags);

UNIT_TEST_MAIN
