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

#include <cstdint>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "ProcParser.h"
#include "ebpf/SourceManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/ProcessEvent.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcFsStub.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessCacheManagerUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        mSourceManager = std::make_shared<SourceManager>();
        mTestRoot = std::filesystem::path(GetProcessExecutionDir()) / "ProcessCacheManagerUnittest";
        mProcDir = mTestRoot / "proc";
        mProcessCacheManager
            = std::make_shared<ProcessCacheManager>(mSourceManager, "test_host", mTestRoot.string(), mEventQueue);
    }

    void TearDown() override {
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> emptyQueue;
        mEventQueue.swap(emptyQueue);
        std::filesystem::remove_all(mTestRoot);
    }

    // ProcessCacheManager测试用例
    void TestListRunningProcs();
    // void TestWriteProcToBPFMap();
    void TestProcToExecveEvent();
    void TestProcessExecveEvents();
    void TestProcessCloneEventEvent();
    void TestProcessExitEvent();
    void TestProcessDataEventNormal();
    void TestProcessDataEventExceedLimit();
    void TestFinalizeProcessTags();
    // void TestPollPerfBuffers();
    void TestHandleCacheUpdate();
    void TestUpdateCacheNormal();
    void TestUpdateCacheExceedLimit();

private:
    void FillKernelThreadProc(Proc& proc);
    void FillRootCwdProc(Proc& proc);

    std::shared_ptr<SourceManager> mSourceManager;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
    std::filesystem::path mTestRoot;
    std::filesystem::path mProcDir;
};

void ProcessCacheManagerUnittest::FillKernelThreadProc(Proc& proc) {
    proc.pid = 10002;
    proc.ppid = 0;
    proc.tid = proc.pid;
    proc.nspid = 0; // no container_id
    proc.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD | ApiEventFlag::NeedsAUID
                                       | ApiEventFlag::RootCWD);
    proc.cwd = "/";
    proc.comm = "ksoftirqd/18";
    proc.cmdline = ""; // \0 separated binary and args
    proc.exe = "";
    proc.container_id.resize(0);
    proc.effective = 0x000001ffffffffff;
    proc.inheritable = 0x0000000000000000;
    proc.permitted = 0x000001ffffffffff;
}

void ProcessCacheManagerUnittest::FillRootCwdProc(Proc& proc) {
    proc.pid = 20001;
    proc.ppid = 99999;
    proc.tid = proc.pid;
    proc.nspid = 0; // no container_id
    proc.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD | ApiEventFlag::NeedsAUID
                                       | ApiEventFlag::RootCWD);
    proc.cwd = "/";
    proc.comm = "cat";
    proc.cmdline = "cat"; // \0 separated binary and args
    proc.exe = "/usr/bin/cat";
    proc.container_id.resize(0);
}

void ProcessCacheManagerUnittest::TestListRunningProcs() {
    ProcFsStub procFsStub(mProcDir);
    std::unordered_map<int, Proc> pidMap;
    for (uint32_t i = 1; i < 11; ++i) { // max i-1 is 9 so that container_id generated is ok
        Proc proc = CreateStubProc();
        proc.pid = i;
        proc.ppid = i - 1;
        proc.ktime = i * 1000000000UL;
        proc.pktime = (i - 1) * 1000000000UL;
        proc.auid = i + 500;
        proc.uids = {i + 500, i + 500, i + 500, i + 500}; // Real UID, Effective UID, Saved Set-UID, Filesystem UID
        proc.gids = {i + 500, i + 500, i + 500, i + 500};
        proc.tid = proc.pid;
        proc.nspid = proc.pid;
        proc.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD | ApiEventFlag::NeedsAUID);
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
    auto procs = mProcessCacheManager->ListRunningProcs();
    for (const auto& proc : procs) {
        const auto it = pidMap.find(proc->pid);
        APSARA_TEST_TRUE_FATAL(it != pidMap.end());
        const auto& expected = it->second;
        APSARA_TEST_EQUAL(proc->pid, expected.pid);
        APSARA_TEST_EQUAL(proc->ppid, expected.ppid);
        APSARA_TEST_EQUAL(proc->ktime, expected.ktime);
        APSARA_TEST_EQUAL(proc->auid, expected.auid);
        APSARA_TEST_EQUAL(proc->uids, expected.uids);
        APSARA_TEST_EQUAL(proc->gids, expected.gids);
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
            // APSARA_TEST_EQUAL(proc->pflags, static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD |
            // ApiEventFlag::NeedsAUID);
        }
    }
}

void ProcessCacheManagerUnittest::TestProcToExecveEvent() {
    { // kernel thread
        Proc proc = CreateStubProc();
        FillKernelThreadProc(proc);
        MsgExecveEventUnix event;
        mProcessCacheManager->ProcToExecveEvent(proc, event);
        APSARA_TEST_EQUAL(event.kernel_thread, true);
        APSARA_TEST_EQUAL(event.msg.parent.pid, proc.ppid);
        APSARA_TEST_EQUAL(event.msg.parent.ktime, proc.ktime);
        APSARA_TEST_EQUAL(event.process.pid, proc.pid);
        APSARA_TEST_EQUAL(event.process.tid, proc.tid);
        APSARA_TEST_EQUAL(event.process.nspid, proc.nspid);
        APSARA_TEST_EQUAL(event.process.uid, 0U);
        APSARA_TEST_EQUAL(event.process.auid, std::numeric_limits<uint32_t>::max());
        APSARA_TEST_EQUAL(event.process.flags, static_cast<uint32_t>(ApiEventFlag::ProcFS));
        APSARA_TEST_EQUAL(event.process.ktime, proc.ktime);
        APSARA_TEST_EQUAL(event.process.cwd, proc.cwd);
        APSARA_TEST_EQUAL(event.process.filename, proc.comm);
        APSARA_TEST_EQUAL(event.process.binary, proc.comm);
    }
    { // cwd is root and invalid ppid
        Proc proc = CreateStubProc();
        FillRootCwdProc(proc);
        MsgExecveEventUnix event;
        mProcessCacheManager->ProcToExecveEvent(proc, event);
        APSARA_TEST_EQUAL(event.kernel_thread, false);
        APSARA_TEST_EQUAL(event.msg.common.op, MSG_OP_EXECVE);
        APSARA_TEST_EQUAL(event.msg.common.size, uint32_t(SIZEOF_EVENT));
        APSARA_TEST_EQUAL(event.kube.docker, proc.container_id);
        APSARA_TEST_EQUAL(event.msg.parent.pid, proc.ppid);
        APSARA_TEST_EQUAL(event.msg.parent.ktime, proc.pktime);
        APSARA_TEST_EQUAL(event.msg.ns.uts_inum, proc.uts_ns);
        APSARA_TEST_EQUAL(event.msg.ns.ipc_inum, proc.ipc_ns);
        APSARA_TEST_EQUAL(event.msg.ns.mnt_inum, proc.mnt_ns);
        APSARA_TEST_EQUAL(event.msg.ns.pid_inum, proc.pid_ns);
        APSARA_TEST_EQUAL(event.msg.ns.pid_for_children_inum, proc.pid_for_children_ns);
        APSARA_TEST_EQUAL(event.msg.ns.net_inum, proc.net_ns);
        APSARA_TEST_EQUAL(event.msg.ns.time_inum, proc.time_ns);
        APSARA_TEST_EQUAL(event.msg.ns.time_for_children_inum, proc.time_for_children_ns);
        APSARA_TEST_EQUAL(event.msg.ns.cgroup_inum, proc.cgroup_ns);
        APSARA_TEST_EQUAL(event.msg.ns.user_inum, proc.user_ns);
        APSARA_TEST_EQUAL(event.process.pid, proc.pid);
        APSARA_TEST_EQUAL(event.process.tid, proc.tid);
        APSARA_TEST_EQUAL(event.process.nspid, proc.nspid);
        APSARA_TEST_EQUAL(event.process.uid, proc.uids[1]);
        APSARA_TEST_EQUAL(event.process.auid, proc.auid);
        APSARA_TEST_EQUAL(event.msg.creds.uid, proc.uids[0]);
        APSARA_TEST_EQUAL(event.msg.creds.gid, proc.uids[1]);
        APSARA_TEST_EQUAL(event.msg.creds.suid, proc.uids[2]);
        APSARA_TEST_EQUAL(event.msg.creds.sgid, proc.uids[3]);
        APSARA_TEST_EQUAL(event.msg.creds.euid, proc.gids[0]);
        APSARA_TEST_EQUAL(event.msg.creds.egid, proc.gids[1]);
        APSARA_TEST_EQUAL(event.msg.creds.fsuid, proc.gids[2]);
        APSARA_TEST_EQUAL(event.msg.creds.fsgid, proc.gids[3]);
        APSARA_TEST_EQUAL(event.msg.creds.caps.permitted, proc.permitted);
        APSARA_TEST_EQUAL(event.msg.creds.caps.effective, proc.effective);
        APSARA_TEST_EQUAL(event.msg.creds.caps.inheritable, proc.inheritable);
        APSARA_TEST_EQUAL(event.process.flags, proc.flags);
        APSARA_TEST_EQUAL(event.process.ktime, proc.ktime);
        APSARA_TEST_EQUAL(event.msg.common.ktime, proc.ktime);
        APSARA_TEST_EQUAL(event.process.filename, proc.exe);
        APSARA_TEST_EQUAL(event.process.binary, proc.exe);
        APSARA_TEST_EQUAL(event.process.cmdline, proc.cmdline);
        APSARA_TEST_EQUAL(event.process.cwd, proc.cwd);
    }
}

void ProcessCacheManagerUnittest::TestUpdateCacheNormal() {
    // 测试缓存操作
    data_event_id key{12345, 1234567890};
    auto execveEvent = std::make_shared<MsgExecveEventUnix>();
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";

    // 测试缓存更新

    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(execveEvent));
    APSARA_TEST_TRUE(mProcessCacheManager->ContainsKey(key));

    // 测试缓存查找
    auto cachedEvent = mProcessCacheManager->LookupCache(key);
    APSARA_TEST_TRUE(cachedEvent != nullptr);
    APSARA_TEST_EQUAL(cachedEvent->process.pid, execveEvent->process.pid);
    APSARA_TEST_EQUAL(cachedEvent->process.ktime, execveEvent->process.ktime);
    APSARA_TEST_EQUAL(cachedEvent->process.uid, execveEvent->process.uid);
    APSARA_TEST_EQUAL(cachedEvent->process.binary, execveEvent->process.binary);

    // 测试缓存释放
    mProcessCacheManager->releaseCache(key);
    APSARA_TEST_FALSE(mProcessCacheManager->ContainsKey(key));
}

void ProcessCacheManagerUnittest::TestProcessExecveEvents() {
    // 测试Execve事件处理
    auto execveEvent = std::make_unique<MsgExecveEventUnix>();
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";

    msg_execve_event kernelExecveEvent;
    kernelExecveEvent.process.pid = execveEvent->process.pid;
    kernelExecveEvent.process.ktime = execveEvent->process.ktime;
    mProcessCacheManager->RecordExecveEvent(&kernelExecveEvent);

    // 测试Exit事件处理
    msg_exit exitEvent;
    exitEvent.current.pid = 1234;
    exitEvent.current.ktime = 5678;
    exitEvent.info.code = 0;
    mProcessCacheManager->RecordExitEvent(&exitEvent);

    // 测试Clone事件处理
    msg_clone_event cloneEvent;
    cloneEvent.tgid = 1234;
    cloneEvent.ktime = 5678;
    mProcessCacheManager->RecordCloneEvent(&cloneEvent);
}

void ProcessCacheManagerUnittest::TestProcessDataEventNormal() {
    // 测试数据事件处理
    std::string testArg = "test_arg";
    msg_data dataEvent;
    dataEvent.id.pid = 1234;
    dataEvent.id.time = 5678;
    memcpy(dataEvent.arg, testArg.c_str(), testArg.size());
    dataEvent.common.size = offsetof(msg_data, arg) + testArg.size();

    mProcessCacheManager->RecordDataEvent(&dataEvent);

    // 测试数据事件ID生成和查找
    data_event_desc desc{};
    desc.id.pid = 1234;
    desc.id.time = 5678;
    desc.size = testArg.size();
    desc.leftover = 0;

    // mProcessCacheManager->DataAdd(&dataEvent);
    std::string retrievedData = mProcessCacheManager->dataGetAndRemove(&desc);
    APSARA_TEST_FALSE(retrievedData.empty());
}

void ProcessCacheManagerUnittest::TestFinalizeProcessTags() {
    // 创建进程事件
    auto execveEvent = std::make_shared<MsgExecveEventUnix>();
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";
    execveEvent->msg.cleanup_key.ktime = 0;
    execveEvent->msg.parent.pid = 2345;
    execveEvent->msg.parent.ktime = 6789;

    // parent
    auto pExecveEvent = std::make_shared<MsgExecveEventUnix>();
    pExecveEvent->process.pid = 2345;
    pExecveEvent->process.ktime = 6789;
    pExecveEvent->process.uid = 1000;
    pExecveEvent->process.binary = "test_binary_parent";

    // 更新缓存
    data_event_id key{execveEvent->process.pid, execveEvent->process.ktime};
    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(execveEvent));
    key = {pExecveEvent->process.pid, pExecveEvent->process.ktime};
    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(pExecveEvent));

    // 测试进程标签生成
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    auto tags
        = mProcessCacheManager->FinalizeProcessTags(sourceBuffer, execveEvent->process.pid, execveEvent->process.ktime);
    APSARA_TEST_FALSE(tags.mInner.empty());
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
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcToExecveEvent);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestFinalizeProcessTags);

UNIT_TEST_MAIN
