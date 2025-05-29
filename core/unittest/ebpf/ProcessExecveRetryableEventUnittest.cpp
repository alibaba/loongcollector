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

#include "ebpf/plugin/ProcessExecveRetryableEvent.h"
#include "security/bpf_process_event_type.h"
#include "type/table/BaseElements.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/EBPFRawEventStub.h"
#include "unittest/ebpf/ProcessCacheManagerWrapper.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessExecveRetryableEventUnittest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override { mWrapper.Clear(); }

    void TestMsgExecveEventToProcessCacheValueNoClone();
    void TestMsgExecveEventToProcessCacheValueLongFilename();
    void TestMsgExecveEventToProcessCacheValueLongArgs();
    void TestMsgExecveEventToProcessCacheValueNoArgs();
    void TestMsgExecveEventToProcessCacheValueNoArgsNoCwd();

private:
    ProcessCacheManagerWrapper mWrapper;
};


void ProcessExecveRetryableEventUnittest::TestMsgExecveEventToProcessCacheValueNoClone() {
    msg_execve_event event = CreateStubExecveEvent();
    constexpr char args[] = "/usr/bin/ls\0-l\0/root/one more thing\0/root";
    constexpr uint32_t argsSize = sizeof(args) - 1;
    memcpy(event.buffer + SIZEOF_EVENT, args, argsSize);
    event.process.size = argsSize + SIZEOF_EVENT;

    event.cleanup_key.pid = 1234;
    event.cleanup_key.ktime = 123456780;

    std::unique_ptr<ProcessExecveRetryableEvent> processExecveRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessExecveRetryableEvent(&event));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    processExecveRetryableEvent->fillProcessPlainFields(event, *cacheValue);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.cleanup_key.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.cleanup_key.ktime);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue->Get<kCWD>().to_string(), "/root");
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>().to_string(), "/usr/bin/ls");
    APSARA_TEST_EQUAL(cacheValue->Get<kArguments>().to_string(), "-l \"/root/one more thing\"");

    APSARA_TEST_EQUAL(cacheValue->Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessExecveRetryableEventUnittest::TestMsgExecveEventToProcessCacheValueLongFilename() {
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

    mWrapper.mProcessCacheManager->RecordDataEvent(&msgData);

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

    std::unique_ptr<ProcessExecveRetryableEvent> processExecveRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessExecveRetryableEvent(&event));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    processExecveRetryableEvent->fillProcessPlainFields(event, *cacheValue);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue->Get<kCWD>().to_string(), "/");
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>().to_string(), "/" + filename);
    APSARA_TEST_EQUAL(cacheValue->Get<kArguments>().to_string(), "-l \"/root/one more thing\"");

    APSARA_TEST_EQUAL(cacheValue->Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessExecveRetryableEventUnittest::TestMsgExecveEventToProcessCacheValueLongArgs() {
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
    mWrapper.mProcessCacheManager->RecordDataEvent(&msgData);

    std::string arg2(1023, 'b');
    std::copy(arg2.begin(), arg2.end(), msgData.arg);
    msgData.common.size = offsetof(struct msg_data, arg) + arg2.size();
    mWrapper.mProcessCacheManager->RecordDataEvent(&msgData);

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

    std::unique_ptr<ProcessExecveRetryableEvent> processExecveRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessExecveRetryableEvent(&event));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    processExecveRetryableEvent->fillProcessPlainFields(event, *cacheValue);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue->Get<kCWD>().to_string(), cwd);
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue->Get<kArguments>().to_string(), arguments);

    APSARA_TEST_EQUAL(cacheValue->Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessExecveRetryableEventUnittest::TestMsgExecveEventToProcessCacheValueNoArgs() {
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

    std::unique_ptr<ProcessExecveRetryableEvent> processExecveRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessExecveRetryableEvent(&event));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    processExecveRetryableEvent->fillProcessPlainFields(event, *cacheValue);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue->Get<kCWD>().to_string(), cwd);
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue->Get<kArguments>().to_string(), "");

    APSARA_TEST_EQUAL(cacheValue->Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

void ProcessExecveRetryableEventUnittest::TestMsgExecveEventToProcessCacheValueNoArgsNoCwd() {
    msg_execve_event event = CreateStubExecveEvent();
    // fill binary
    constexpr char binary[] = "/usr/bin/ls";
    memcpy(event.buffer + SIZEOF_EVENT, binary, sizeof(binary));
    uint32_t currentOffset = sizeof(binary);
    event.process.size = currentOffset + SIZEOF_EVENT;
    event.process.flags &= ~(EVENT_NO_CWD_SUPPORT | EVENT_ERROR_CWD | EVENT_ROOT_CWD);

    std::unique_ptr<ProcessExecveRetryableEvent> processExecveRetryableEvent(
        mWrapper.mProcessCacheManager->CreateProcessExecveRetryableEvent(&event));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    processExecveRetryableEvent->fillProcessPlainFields(event, *cacheValue);
    APSARA_TEST_EQUAL(cacheValue->mPPid, event.parent.pid);
    APSARA_TEST_EQUAL(cacheValue->mPKtime, event.parent.ktime);
    APSARA_TEST_EQUAL(cacheValue->Get<kProcessId>().to_string(), std::to_string(event.process.pid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUid>().to_string(), std::to_string(event.process.uid));
    APSARA_TEST_EQUAL(cacheValue->Get<kUser>().to_string(), "root");
    APSARA_TEST_EQUAL(cacheValue->Get<kKtime>().to_string(), std::to_string(event.process.ktime));
    APSARA_TEST_EQUAL(cacheValue->Get<kCWD>().to_string(), "");
    APSARA_TEST_EQUAL(cacheValue->Get<kBinary>().to_string(), binary);
    APSARA_TEST_EQUAL(cacheValue->Get<kArguments>().to_string(), "");

    APSARA_TEST_EQUAL(cacheValue->Get<kCapPermitted>().to_string(), std::string("CAP_CHOWN CAP_FSETID"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapEffective>().to_string(),
                      std::string("CAP_CHOWN DAC_OVERRIDE CAP_FSETID CAP_KILL"));
    APSARA_TEST_EQUAL(cacheValue->Get<kCapInheritable>().to_string(), std::string("DAC_OVERRIDE CAP_KILL"));
}

UNIT_TEST_CASE(ProcessExecveRetryableEventUnittest, TestMsgExecveEventToProcessCacheValueNoClone);
UNIT_TEST_CASE(ProcessExecveRetryableEventUnittest, TestMsgExecveEventToProcessCacheValueLongFilename);
UNIT_TEST_CASE(ProcessExecveRetryableEventUnittest, TestMsgExecveEventToProcessCacheValueLongArgs);
UNIT_TEST_CASE(ProcessExecveRetryableEventUnittest, TestMsgExecveEventToProcessCacheValueNoArgs);
UNIT_TEST_CASE(ProcessExecveRetryableEventUnittest, TestMsgExecveEventToProcessCacheValueNoArgsNoCwd);

UNIT_TEST_MAIN
