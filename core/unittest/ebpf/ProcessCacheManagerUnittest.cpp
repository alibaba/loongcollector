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

#include <cstdint>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "ProcParser.h"
#include "common/memory/SourceBuffer.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/ProcessEvent.h"
#include "models/PipelineEventGroup.h"
#include "security/bpf_process_event_type.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ProcFsStub.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessCacheManagerUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mTestRoot = std::filesystem::path(GetProcessExecutionDir()) / "ProcessCacheManagerUnittest";
        mProcDir = mTestRoot / "proc";
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(
            mEBPFAdapter, "test_host", mTestRoot.string(), mEventQueue, nullptr, nullptr, nullptr, nullptr);
    }

    void TearDown() override {
        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> emptyQueue;
        mEventQueue.swap(emptyQueue);
        std::filesystem::remove_all(mTestRoot);
    }

    // ProcessCacheManager测试用例
    void TestListRunningProcs();
    // void TestWriteProcToBPFMap();
    void TestProcToProcessCacheValue();

    void TestRecordDataEventNormal();
    void TestDataGetAndRemoveSizeBad();
    void TestRecordDataEventExceedLimit();

    void TestMsgExecveEventToProcessCacheValueNoClone();
    void TestMsgExecveEventToProcessCacheValueLongFilename();
    void TestMsgExecveEventToProcessCacheValueLongArgs();
    void TestMsgExecveEventToProcessCacheValueNoArgs();
    void TestMsgExecveEventToProcessCacheValueNoArgsNoCwd();

    void TestMsgCloneEventToProcessCacheValue();
    void TestMsgCloneEventToProcessCacheValueParentNotFound();

    void TestRecordEventCloneExecveExit();
    void TestRecordEventExecveExit();

    void TestFinalizeProcessTags();

private:
    void FillKernelThreadProc(Proc& proc);
    void FillRootCwdProc(Proc& proc);
    void FillExecveEventNoClone(msg_execve_event& event);
    void FillExecveEventLongFilename(msg_execve_event& event);

    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
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
    proc.flags = static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID | EVENT_ROOT_CWD);
    proc.cwd = "/";
    proc.comm = "ksoftirqd/18";
    proc.cmdline = ""; // \0 separated binary and args
    proc.exe = "";
    proc.container_id.resize(0);
    proc.effective = 0x000001ffffffffff;
    proc.inheritable = 0x0000000000000000;
    proc.permitted = 0x000001ffffffffff;
}

msg_execve_event CreateStubExecveEvent() {
    msg_execve_event event{};
    event.process.pid = 1234;
    event.process.ktime = 123456789;
    event.process.uid = 0;
    event.creds.caps.permitted = 0x11;
    event.creds.caps.effective = 0x33;
    event.creds.caps.inheritable = 0x22;
    event.parent.pid = 5678;
    event.parent.ktime = 567891234;
    return event;
}

void ProcessCacheManagerUnittest::FillExecveEventNoClone(msg_execve_event& event) {
    constexpr char args[] = "/usr/bin/ls\0-l\0/root/one more thing\0/root";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT, args, argsSize);
    event.process.size = argsSize + SIZEOF_EVENT;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;
}

void ProcessCacheManagerUnittest::FillExecveEventLongFilename(msg_execve_event& event) {
    // fill msg_data
    struct msg_data msgData {};
    msgData.id.pid = event.process.pid;
    msgData.id.time = event.process.ktime;
    std::fill_n(msgData.arg, 255, 'a');
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = event.process.ktime;
    msgData.common.size = offsetof(struct msg_data, arg) + 255;

    mProcessCacheManager->RecordDataEvent(&msgData);

    // fill data_event_desc
    auto* desc = reinterpret_cast<data_event_desc*>(event.buffer + SIZEOF_EVENT);
    desc->error = 0;
    desc->pad = 0;
    desc->size = 256;
    desc->leftover = 1;
    desc->id.pid = event.process.pid;
    desc->id.time = event.process.ktime;
    // fill arguments and cwd
    constexpr char args[] = "-l\0/root/one more thing\0";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT + sizeof(data_event_desc), args, argsSize);
    event.process.size = argsSize + sizeof(data_event_desc) + SIZEOF_EVENT;

    event.process.flags |= EVENT_DATA_FILENAME | EVENT_ROOT_CWD;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;
}

void ProcessCacheManagerUnittest::FillRootCwdProc(Proc& proc) {
    proc.pid = 20001;
    proc.ppid = 99999;
    proc.tid = proc.pid;
    proc.nspid = 0; // no container_id
    proc.flags = static_cast<uint32_t>(EVENT_PROCFS | EVENT_NEEDS_CWD | EVENT_NEEDS_AUID | EVENT_ROOT_CWD);
    proc.cwd = "/";
    proc.comm = "cat";
    constexpr char cmdline[] = "cat\0/etc/host.conf\0/etc/resolv.conf";
    proc.cmdline.assign(cmdline, sizeof(cmdline) - 1); // \0 separated binary and args
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
    auto procs = mProcessCacheManager->listRunningProcs();
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

void ProcessCacheManagerUnittest::TestProcToProcessCacheValue() {
    { // kernel thread
        Proc proc = CreateStubProc();
        FillKernelThreadProc(proc);
        auto cacheValuePtr = mProcessCacheManager->procToProcessCacheValue(proc);
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
        auto cacheValuePtr = mProcessCacheManager->procToProcessCacheValue(proc);
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
    }
}

void ProcessCacheManagerUnittest::TestRecordDataEventNormal() {
    {
        // fill msg_data
        struct msg_data msgData {};
        msgData.id.pid = 1234;
        msgData.id.time = 123546789;
        std::string filename(255, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

        mProcessCacheManager->RecordDataEvent(&msgData);
        APSARA_TEST_EQUAL(1UL, mProcessCacheManager->mDataMap.size());

        // fill data_event_desc
        data_event_desc desc{};
        desc.error = 0;
        desc.pad = 0;
        desc.size = filename.size();
        desc.leftover = 0;
        desc.id.pid = msgData.id.pid;
        desc.id.time = msgData.id.time;
        auto dataStr = mProcessCacheManager->dataGetAndRemove(&desc);
        APSARA_TEST_EQUAL(dataStr, filename);
        APSARA_TEST_EQUAL(0UL, mProcessCacheManager->mDataMap.size());
    }
    {
        // fill msg_data
        struct msg_data msgData {};
        msgData.id.pid = 1234;
        msgData.id.time = 123546789;
        std::string filename(255, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

        mProcessCacheManager->RecordDataEvent(&msgData);
        mProcessCacheManager->RecordDataEvent(&msgData);
        mProcessCacheManager->RecordDataEvent(&msgData);
        mProcessCacheManager->RecordDataEvent(&msgData);
        mProcessCacheManager->RecordDataEvent(&msgData);
        APSARA_TEST_EQUAL(1UL, mProcessCacheManager->mDataMap.size());

        std::string fullFilename = filename + filename + filename + filename + filename;
        // fill data_event_desc
        data_event_desc desc{};
        desc.error = 0;
        desc.pad = 0;
        desc.size = fullFilename.size();
        desc.leftover = 0;
        desc.id.pid = msgData.id.pid;
        desc.id.time = msgData.id.time;
        auto dataStr = mProcessCacheManager->dataGetAndRemove(&desc);
        APSARA_TEST_EQUAL(dataStr, fullFilename);
        APSARA_TEST_EQUAL(0UL, mProcessCacheManager->mDataMap.size());
    }
}

void ProcessCacheManagerUnittest::TestDataGetAndRemoveSizeBad() {
    // fill msg_data
    struct msg_data msgData {};
    msgData.id.pid = 1234;
    msgData.id.time = 123546789;
    std::string filename(255, 'a');
    std::copy(filename.begin(), filename.end(), msgData.arg);
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = msgData.id.time;
    msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

    mProcessCacheManager->RecordDataEvent(&msgData);
    APSARA_TEST_EQUAL(1UL, mProcessCacheManager->mDataMap.size());

    // fill data_event_desc
    data_event_desc desc{};
    desc.error = 0;
    desc.pad = 0;
    desc.size = filename.size();
    desc.leftover = 1; // let size - leftover != filename.size()
    desc.id.pid = msgData.id.pid;
    desc.id.time = msgData.id.time;
    auto dataStr = mProcessCacheManager->dataGetAndRemove(&desc);
    APSARA_TEST_EQUAL(dataStr, "");
    APSARA_TEST_EQUAL(0UL, mProcessCacheManager->mDataMap.size());
}


void ProcessCacheManagerUnittest::TestRecordDataEventExceedLimit() {
    for (size_t i = 1; i <= ProcessCacheManager::kMaxDataMapSize; i++) {
        struct msg_data msgData {};
        msgData.id.pid = i;
        msgData.id.time = 123546789;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessCacheManager->RecordDataEvent(&msgData);
    }
    APSARA_TEST_EQUAL(ProcessCacheManager::kMaxDataMapSize, mProcessCacheManager->mDataMap.size());
    {
        struct msg_data msgData {};
        msgData.id.pid = 0;
        msgData.id.time = 123546789 + kMaxCacheExpiredTimeout + 1;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessCacheManager->RecordDataEvent(&msgData);
    }
    APSARA_TEST_EQUAL(1UL, mProcessCacheManager->mDataMap.size()); // keep one item not expired
    for (size_t i = 1; i <= ProcessCacheManager::kMaxDataMapSize; i++) {
        struct msg_data msgData {};
        msgData.id.pid = i;
        msgData.id.time = 123546789 + kMaxCacheExpiredTimeout + 2;
        std::string filename(1, 'a');
        std::copy(filename.begin(), filename.end(), msgData.arg);
        msgData.common.op = MSG_OP_DATA;
        msgData.common.ktime = msgData.id.time;
        msgData.common.size = offsetof(struct msg_data, arg) + filename.size();
        mProcessCacheManager->RecordDataEvent(&msgData);
    }
    APSARA_TEST_EQUAL(1UL, mProcessCacheManager->mDataMap.size()); // forced clear all, only keep the last one
}

void ProcessCacheManagerUnittest::TestMsgExecveEventToProcessCacheValueNoClone() {
    msg_execve_event event = CreateStubExecveEvent();
    constexpr char args[] = "/usr/bin/ls\0-l\0/root/one more thing\0/root";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT, args, argsSize);
    event.process.size = argsSize + SIZEOF_EVENT;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;

    auto cacheValuePtr = mProcessCacheManager->msgExecveEventToProcessCacheValue(event);
    auto& cacheValue = *cacheValuePtr;
    APSARA_TEST_EQUAL(cacheValue.mPPid, event.cleanup_key.pid);
    APSARA_TEST_EQUAL(cacheValue.mPKtime, event.cleanup_key.ktime);
    APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), "/root");
    APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), "/usr/bin/ls");
    APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), "-l \"/root/one more thing\"");

    APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessCacheManagerUnittest::TestMsgExecveEventToProcessCacheValueLongFilename() {
    msg_execve_event event = CreateStubExecveEvent();
    // fill msg_data
    struct msg_data msgData {};
    msgData.id.pid = event.process.pid;
    msgData.id.time = event.process.ktime;
    std::string filename(255, 'a');
    std::copy(filename.begin(), filename.end(), msgData.arg);
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = event.process.ktime;
    msgData.common.size = offsetof(struct msg_data, arg) + filename.size();

    mProcessCacheManager->RecordDataEvent(&msgData);

    // fill data_event_desc
    auto* desc = reinterpret_cast<data_event_desc*>(event.buffer + SIZEOF_EVENT);
    desc->error = 0;
    desc->pad = 0;
    desc->size = 256;
    desc->leftover = desc->size - filename.size();
    desc->id.pid = event.process.pid;
    desc->id.time = event.process.ktime;
    // fill arguments and cwd
    constexpr char args[] = "-l\0/root/one more thing";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT + sizeof(data_event_desc), args, argsSize);
    event.process.size = argsSize + sizeof(data_event_desc) + SIZEOF_EVENT;

    event.process.flags |= EVENT_DATA_FILENAME | EVENT_ROOT_CWD | EVENT_CLONE;

    auto cacheValuePtr = mProcessCacheManager->msgExecveEventToProcessCacheValue(event);
    auto& cacheValue = *cacheValuePtr;
    APSARA_TEST_EQUAL(cacheValue.mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue.mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), "/");
    APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), "/" + filename);
    APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), "-l \"/root/one more thing\"");

    APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessCacheManagerUnittest::TestMsgExecveEventToProcessCacheValueLongArgs() {
    msg_execve_event event = CreateStubExecveEvent();
    // fill msg_data
    struct msg_data msgData {};
    msgData.id.pid = event.process.pid;
    msgData.id.time = event.process.ktime;
    std::string arg1(1023, 'a');
    arg1.append(1, '\0');
    std::copy(arg1.begin(), arg1.end(), msgData.arg);
    msgData.common.op = MSG_OP_DATA;
    msgData.common.ktime = event.process.ktime;
    msgData.common.size = offsetof(struct msg_data, arg) + arg1.size();
    mProcessCacheManager->RecordDataEvent(&msgData);

    std::string arg2(1023, 'b');
    std::copy(arg2.begin(), arg2.end(), msgData.arg);
    msgData.common.size = offsetof(struct msg_data, arg) + arg2.size();
    mProcessCacheManager->RecordDataEvent(&msgData);

    std::string arguments(arg1 + arg2);
    arguments[1023] = ' ';

    // fill arguments
    constexpr char binary[] = "/usr/bin/ls";
    memcpy(event.buffer + SIZEOF_EVENT, binary, sizeof(binary));
    uint32_t currentOffset = sizeof(binary);
    // fill data_event_desc
    auto* desc = reinterpret_cast<data_event_desc*>(event.buffer + SIZEOF_EVENT + currentOffset);
    desc->error = 0;
    desc->pad = 0;
    desc->size = arg1.size() + arg2.size();
    desc->leftover = 0;
    desc->id.pid = event.process.pid;
    desc->id.time = event.process.ktime;
    currentOffset += sizeof(data_event_desc);
    // fill cwd
    constexpr char cwd[] = "/root";
    memcpy(event.buffer + SIZEOF_EVENT + currentOffset, cwd, sizeof(cwd) - 1);
    currentOffset += sizeof(cwd) - 1;

    event.process.size = currentOffset + SIZEOF_EVENT;


    event.process.flags |= EVENT_DATA_ARGS | EVENT_PROCFS;

    auto cacheValuePtr = mProcessCacheManager->msgExecveEventToProcessCacheValue(event);
    auto& cacheValue = *cacheValuePtr;
    APSARA_TEST_EQUAL(cacheValue.mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue.mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), cwd);
    APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), arguments);

    APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessCacheManagerUnittest::TestMsgExecveEventToProcessCacheValueNoArgs() {
    msg_execve_event event = CreateStubExecveEvent();
    // fill binary
    constexpr char binary[] = "/usr/bin/ls";
    memcpy(event.buffer + SIZEOF_EVENT, binary, sizeof(binary));
    uint32_t currentOffset = sizeof(binary);
    // fill cwd
    constexpr char cwd[] = "/root";
    memcpy(event.buffer + SIZEOF_EVENT + currentOffset, cwd, sizeof(cwd) - 1);
    currentOffset += sizeof(cwd) - 1;

    event.process.size = currentOffset + SIZEOF_EVENT;
    event.process.flags |= EVENT_ERROR_PATH_COMPONENTS;

    auto cacheValuePtr = mProcessCacheManager->msgExecveEventToProcessCacheValue(event);
    auto& cacheValue = *cacheValuePtr;
    APSARA_TEST_EQUAL(cacheValue.mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue.mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), cwd);
    APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), "");

    APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessCacheManagerUnittest::TestMsgExecveEventToProcessCacheValueNoArgsNoCwd() {
    msg_execve_event event = CreateStubExecveEvent();
    // fill binary
    constexpr char binary[] = "/usr/bin/ls";
    memcpy(event.buffer + SIZEOF_EVENT, binary, sizeof(binary));
    uint32_t currentOffset = sizeof(binary);
    event.process.size = currentOffset + SIZEOF_EVENT;
    event.process.flags &= ~(EVENT_NO_CWD_SUPPORT | EVENT_ERROR_CWD | EVENT_ROOT_CWD);

    auto cacheValuePtr = mProcessCacheManager->msgExecveEventToProcessCacheValue(event);
    auto& cacheValue = *cacheValuePtr;
    APSARA_TEST_EQUAL(cacheValue.mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue.mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue.Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue.Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue.Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue.Get<kCWD>().to_string(), "");
    APSARA_TEST_EQUAL(cacheValue.Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue.Get<kArguments>().to_string(), "");

    APSARA_TEST_EQUAL(cacheValue.Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue.Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessCacheManagerUnittest::TestMsgCloneEventToProcessCacheValue() {
    // 测试缓存操作
    data_event_id parentkey{12345, 123456789};
    auto parentExecId = mProcessCacheManager->GenerateExecId(parentkey.pid, parentkey.time);
    auto parentCacheValue = std::make_shared<ProcessCacheValue>();
    parentCacheValue->SetContent<kProcessId>(StringView("1234"));
    parentCacheValue->SetContent<kKtime>(StringView("123456789"));
    parentCacheValue->SetContent<kExecId>(parentExecId);
    parentCacheValue->SetContent<kUid>(StringView("1000"));
    parentCacheValue->SetContent<kBinary>(StringView("test_binary"));

    // 测试缓存更新
    mProcessCacheManager->mProcessCache.AddCache(parentkey, std::move(parentCacheValue));

    msg_clone_event event{};
    event.tgid = 5678;
    event.ktime = 123456790;
    event.parent.pid = parentkey.pid;
    event.parent.ktime = parentkey.time;
    auto execId = mProcessCacheManager->GenerateExecId(event.tgid, event.ktime);
    std::shared_ptr<ProcessCacheValue> cacheValue = mProcessCacheManager->msgCloneEventToProcessCacheValue(event);
    APSARA_TEST_TRUE(cacheValue != nullptr);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL((*cacheValue).Get<kProcessId>().to_string(), std::to_string(event.tgid));
    APSARA_TEST_EQUAL((*cacheValue).Get<kKtime>().to_string(), std::to_string(event.ktime));
    APSARA_TEST_EQUAL((*cacheValue).Get<kExecId>().to_string(), execId);
    APSARA_TEST_EQUAL((*cacheValue).Get<kUid>().to_string(), "1000");
    APSARA_TEST_EQUAL((*cacheValue).Get<kBinary>().to_string(), "test_binary");
}

void ProcessCacheManagerUnittest::TestMsgCloneEventToProcessCacheValueParentNotFound() {
    msg_clone_event event{};
    event.tgid = 5678;
    event.ktime = 123456790;
    event.parent.pid = 1234;
    event.parent.pid = 123456789;
    std::shared_ptr<ProcessCacheValue> cacheValue = mProcessCacheManager->msgCloneEventToProcessCacheValue(event);
    APSARA_TEST_TRUE(cacheValue == nullptr);
}

/*
 * Before daemon exit
 * Lineage:     ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:      2             0                 2               0              1
 * After daemon exit
 * Lineage:     ┌------------------------------┐ ┌-----------------------------┐
 * CallChain: (sh) -clone- (daemon) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:      1             0                 1               0              1
 */
void ProcessCacheManagerUnittest::TestRecordEventCloneExecveExit() {
    mProcessCacheManager->MarkProcessEventFlushStatus(true);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = 1;
    shExecveEvent.parent.ktime = 1;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;
    mProcessCacheManager->RecordExecveEvent(&shExecveEvent);

    msg_clone_event daemonCloneEvent{};
    daemonCloneEvent.common.ktime = 30;
    daemonCloneEvent.tgid = 3;
    daemonCloneEvent.ktime = 30;
    daemonCloneEvent.parent.pid = shExecveEvent.process.pid;
    daemonCloneEvent.parent.ktime = shExecveEvent.process.ktime;
    mProcessCacheManager->RecordCloneEvent(&daemonCloneEvent);

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
    mProcessCacheManager->RecordExecveEvent(&daemonExecveEvent);

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;
    mProcessCacheManager->RecordCloneEvent(&appCloneEvent);

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
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
    mProcessCacheManager->RecordExecveEvent(&appExecveEvent);

    // check cache
    auto shProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 2);

    auto daemonClone
        = mProcessCacheManager->mProcessCache.Lookup(data_event_id{daemonCloneEvent.tgid, daemonCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(daemonClone != nullptr);
    APSARA_TEST_EQUAL((*daemonClone).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(daemonClone->mRefCount, 0);

    auto daemonProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 2);

    auto appClone = mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(appClone != nullptr);
    APSARA_TEST_EQUAL((*appClone).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);

    auto appProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(5UL, eventCount);
    auto& event0 = (ProcessEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event0.mEventType);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.process.pid, event0.mPid);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.process.ktime, event0.mKtime);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.common.ktime, event0.mTimestamp);

    auto& event1 = (ProcessEvent&)(*items[1]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_CLONE_EVENT, event1.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonCloneEvent.tgid, event1.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonCloneEvent.ktime, event1.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonCloneEvent.common.ktime, event1.mTimestamp);

    auto& event2 = (ProcessEvent&)(*items[2]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event2.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.process.pid, event2.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.process.ktime, event2.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.common.ktime, event2.mTimestamp);

    auto& event3 = (ProcessEvent&)(*items[3]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_CLONE_EVENT, event3.mEventType);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.tgid, event3.mPid);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.ktime, event3.mKtime);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.common.ktime, event3.mTimestamp);

    auto& event4 = (ProcessEvent&)(*items[4]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event4.mEventType);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.process.pid, event4.mPid);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.process.ktime, event4.mKtime);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.common.ktime, event4.mTimestamp);

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;
    mProcessCacheManager->RecordExitEvent(&daemonExitEvent);

    // check cache
    APSARA_TEST_EQUAL(shProc->mRefCount, 1);
    APSARA_TEST_EQUAL(daemonClone->mRefCount, 0);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    eventCount = mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(1UL, eventCount);
    auto& event6 = (ProcessExitEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXIT_EVENT, event6.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.pid, event6.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.ktime, event6.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.common.ktime, event6.mTimestamp);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.code, event6.mExitCode);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.tid, event6.mExitTid);
}

/*
 * Before daemon exit
 * Lineage:     ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:      0             1                 2              0              1
 * After daemon exit
 * Lineage:     ┌------------┐ ┌--------------┐ ┌-----------------------------┐
 * CallChain: (sh) -execve- (bash) -execve- (daemon) -clone- (app) -execve- (app)
 * RefCnt:      0             0                 1              0              1
 */
void ProcessCacheManagerUnittest::TestRecordEventExecveExit() {
    mProcessCacheManager->MarkProcessEventFlushStatus(true);
    // sprawn processes
    msg_execve_event shExecveEvent = CreateStubExecveEvent();
    shExecveEvent.common.ktime = 20;
    shExecveEvent.process.pid = 2;
    shExecveEvent.process.ktime = 20;
    shExecveEvent.parent.pid = 1;
    shExecveEvent.parent.ktime = 1;
    constexpr char shBinary[] = "/usr/bin/sh";
    memcpy(shExecveEvent.buffer + SIZEOF_EVENT, shBinary, sizeof(shBinary));
    shExecveEvent.process.size = sizeof(shBinary) + SIZEOF_EVENT;
    shExecveEvent.process.flags |= EVENT_CLONE;
    mProcessCacheManager->RecordExecveEvent(&shExecveEvent);

    msg_execve_event bashExecveEvent = CreateStubExecveEvent();
    bashExecveEvent.common.ktime = 21;
    bashExecveEvent.process.pid = 2;
    bashExecveEvent.process.ktime = 21;
    bashExecveEvent.parent.pid = 1;
    bashExecveEvent.parent.ktime = 1;
    bashExecveEvent.cleanup_key.pid = shExecveEvent.process.pid;
    bashExecveEvent.cleanup_key.ktime = shExecveEvent.process.ktime;
    constexpr char bashBinary[] = "/usr/bin/bash";
    memcpy(bashExecveEvent.buffer + SIZEOF_EVENT, bashBinary, sizeof(bashBinary));
    bashExecveEvent.process.size = sizeof(bashBinary) + SIZEOF_EVENT;
    bashExecveEvent.process.flags &= ~EVENT_CLONE;
    mProcessCacheManager->RecordExecveEvent(&bashExecveEvent);

    msg_execve_event daemonExecveEvent = CreateStubExecveEvent();
    daemonExecveEvent.common.ktime = 22;
    daemonExecveEvent.process.pid = 2;
    daemonExecveEvent.process.ktime = 22;
    daemonExecveEvent.parent.pid = 1;
    daemonExecveEvent.parent.ktime = 1;
    daemonExecveEvent.cleanup_key.pid = bashExecveEvent.process.pid;
    daemonExecveEvent.cleanup_key.ktime = bashExecveEvent.process.ktime;
    constexpr char daemonBinary[] = "/usr/local/bin/daemon";
    memcpy(daemonExecveEvent.buffer + SIZEOF_EVENT, daemonBinary, sizeof(daemonBinary));
    daemonExecveEvent.process.size = sizeof(daemonBinary) + SIZEOF_EVENT;
    daemonExecveEvent.process.flags &= ~EVENT_CLONE;
    mProcessCacheManager->RecordExecveEvent(&daemonExecveEvent);

    msg_clone_event appCloneEvent{};
    appCloneEvent.common.ktime = 40;
    appCloneEvent.tgid = 4;
    appCloneEvent.ktime = 40;
    appCloneEvent.parent.pid = daemonExecveEvent.process.pid;
    appCloneEvent.parent.ktime = daemonExecveEvent.process.ktime;
    mProcessCacheManager->RecordCloneEvent(&appCloneEvent);

    msg_execve_event appExecveEvent = CreateStubExecveEvent();
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
    mProcessCacheManager->RecordExecveEvent(&appExecveEvent);

    // check cache
    auto shProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{shExecveEvent.process.pid, shExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(shProc != nullptr);
    APSARA_TEST_EQUAL((*shProc).Get<kBinary>().to_string(), shBinary);
    APSARA_TEST_EQUAL(shProc->mRefCount, 0);

    auto bashProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{bashExecveEvent.process.pid, bashExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(bashProc != nullptr);
    APSARA_TEST_EQUAL((*bashProc).Get<kBinary>().to_string(), bashBinary);
    APSARA_TEST_EQUAL(bashProc->mRefCount, 1);

    auto daemonProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{daemonExecveEvent.process.pid, daemonExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(daemonProc != nullptr);
    APSARA_TEST_EQUAL((*daemonProc).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 2);

    auto appClone = mProcessCacheManager->mProcessCache.Lookup(data_event_id{appCloneEvent.tgid, appCloneEvent.ktime});
    APSARA_TEST_TRUE_FATAL(appClone != nullptr);
    APSARA_TEST_EQUAL((*appClone).Get<kBinary>().to_string(), daemonBinary);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);

    auto appProc = mProcessCacheManager->mProcessCache.Lookup(
        data_event_id{appExecveEvent.process.pid, appExecveEvent.process.ktime});
    APSARA_TEST_TRUE_FATAL(appProc != nullptr);
    APSARA_TEST_EQUAL((*appProc).Get<kBinary>().to_string(), appBinary);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    std::array<std::shared_ptr<CommonEvent>, 10> items{};
    size_t eventCount = mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(5UL, eventCount);
    auto& event0 = (ProcessEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event0.mEventType);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.process.pid, event0.mPid);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.process.ktime, event0.mKtime);
    APSARA_TEST_EQUAL_FATAL(shExecveEvent.common.ktime, event0.mTimestamp);

    auto& event1 = (ProcessEvent&)(*items[1]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event1.mEventType);
    APSARA_TEST_EQUAL_FATAL(bashExecveEvent.process.pid, event1.mPid);
    APSARA_TEST_EQUAL_FATAL(bashExecveEvent.process.ktime, event1.mKtime);
    APSARA_TEST_EQUAL_FATAL(bashExecveEvent.common.ktime, event1.mTimestamp);

    auto& event2 = (ProcessEvent&)(*items[2]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event2.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.process.pid, event2.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.process.ktime, event2.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExecveEvent.common.ktime, event2.mTimestamp);

    auto& event3 = (ProcessEvent&)(*items[3]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_CLONE_EVENT, event3.mEventType);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.tgid, event3.mPid);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.ktime, event3.mKtime);
    APSARA_TEST_EQUAL_FATAL(appCloneEvent.common.ktime, event3.mTimestamp);

    auto& event4 = (ProcessEvent&)(*items[4]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXECVE_EVENT, event4.mEventType);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.process.pid, event4.mPid);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.process.ktime, event4.mKtime);
    APSARA_TEST_EQUAL_FATAL(appExecveEvent.common.ktime, event4.mTimestamp);

    // daemon exit
    msg_exit daemonExitEvent{};
    daemonExitEvent.common.ktime = 60;
    daemonExitEvent.current.pid = daemonExecveEvent.process.pid;
    daemonExitEvent.current.ktime = daemonExecveEvent.process.ktime;
    daemonExitEvent.info.code = -1;
    daemonExitEvent.info.tid = 3;
    mProcessCacheManager->RecordExitEvent(&daemonExitEvent);

    // check cache
    APSARA_TEST_EQUAL(shProc->mRefCount, 0);
    APSARA_TEST_EQUAL(bashProc->mRefCount, 0);
    APSARA_TEST_EQUAL(daemonProc->mRefCount, 1);
    APSARA_TEST_EQUAL(appClone->mRefCount, 0);
    APSARA_TEST_EQUAL(appProc->mRefCount, 1);

    // check output events
    eventCount = mEventQueue.try_dequeue_bulk(items.data(), items.size());
    APSARA_TEST_EQUAL_FATAL(1UL, eventCount);
    auto& event6 = (ProcessExitEvent&)(*items[0]);
    APSARA_TEST_EQUAL_FATAL(KernelEventType::PROCESS_EXIT_EVENT, event6.mEventType);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.pid, event6.mPid);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.current.ktime, event6.mKtime);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.common.ktime, event6.mTimestamp);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.code, event6.mExitCode);
    APSARA_TEST_EQUAL_FATAL(daemonExitEvent.info.tid, event6.mExitTid);
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
    mProcessCacheManager->mProcessCache.AddCache(key, std::move(execveEvent));
    mProcessCacheManager->mProcessCache.AddCache(pKey, std::move(pExecveEvent));

    // 测试进程标签生成
    PipelineEventGroup sharedEventGroup(std::make_shared<SourceBuffer>());
    auto sharedEvent = sharedEventGroup.CreateLogEvent();
    APSARA_TEST_TRUE(mProcessCacheManager->FinalizeProcessTags(key.pid, key.time, *sharedEvent));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kProcessId.LogKey()), StringView("1234"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kKtime.LogKey()), StringView("5678"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kUid.LogKey()), StringView("1000"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kBinary.LogKey()), StringView("test_binary"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentProcessId.LogKey()), StringView("2345"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentKtime.LogKey()), StringView("6789"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentUid.LogKey()), StringView("1000"));
    APSARA_TEST_EQUAL(sharedEvent->GetContent(kParentBinary.LogKey()), StringView("test_binary_parent"));
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
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestProcToProcessCacheValue);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestRecordDataEventNormal);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestDataGetAndRemoveSizeBad);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestRecordDataEventExceedLimit);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgExecveEventToProcessCacheValueNoClone);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgExecveEventToProcessCacheValueLongFilename);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgExecveEventToProcessCacheValueLongArgs);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgExecveEventToProcessCacheValueNoArgs);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgExecveEventToProcessCacheValueNoArgsNoCwd);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgCloneEventToProcessCacheValue);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestMsgCloneEventToProcessCacheValueParentNotFound);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestRecordEventCloneExecveExit);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestRecordEventExecveExit);
UNIT_TEST_CASE(ProcessCacheManagerUnittest, TestFinalizeProcessTags);

UNIT_TEST_MAIN
