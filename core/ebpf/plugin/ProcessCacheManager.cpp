// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ebpf/plugin/ProcessCacheManager.h"

#include <charconv>
#include <coolbpf/security/bpf_common.h>
#include <coolbpf/security/bpf_process_event_type.h>
#include <coolbpf/security/data_msg.h>
#include <coolbpf/security/msg_type.h>

#include <algorithm>
#include <atomic>
#include <string_view>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "_thirdparty/coolbpf/src/security/bpf_process_event_type.h"
#include "common/CapabilityUtil.h"
#include "common/EncodingUtil.h"
#include "common/LRUCache.h"
#include "common/ProcParser.h"
#include "common/StringTools.h"
#include "ebpf/type/ProcessEvent.h"
#include "ebpf/type/table/ProcessTable.h"
#include "logger/Logger.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "type/table/BaseElements.h"
#include "util/FrequencyManager.h"

namespace logtail {
namespace ebpf {

static constexpr size_t kInitCacheSize = 65536UL;
static constexpr size_t kInitDataMapSize = 1024UL;
static constexpr size_t kMaxCacheSize = 4194304UL;
static constexpr size_t kMaxDataMapSize = kInitDataMapSize * 4;
static constexpr auto kOneMinuteNanoseconds = std::chrono::minutes(1) / std::chrono::nanoseconds(1);
static constexpr time_t kMaxCacheExpiredTimeout = kOneMinuteNanoseconds;
static constexpr int kMaxBatchConsumeSize = 1024;
static constexpr int kMaxWaitTimeMS = 200;

/////////// ================= for perfbuffer handlers ================= ///////////
void HandleKernelProcessEvent(void* ctx, int cpu, void* data, uint32_t data_sz) {
    auto* processCacheMgr = static_cast<ProcessCacheManager*>(ctx);
    if (!processCacheMgr) {
        LOG_ERROR(sLogger, ("ProcessCacheManager is null!", ""));
        return;
    }
    if (!data) {
        LOG_ERROR(sLogger, ("data is null!", ""));
        return;
    }

    processCacheMgr->UpdateRecvEventTotal();

    auto* common = static_cast<struct msg_common*>(data);
    switch (common->op) {
        case MSG_OP_CLONE: {
            auto* event = static_cast<struct msg_clone_event*>(data);
            // TODO enqueue
            processCacheMgr->RecordCloneEvent(event);
            break;
        }
        case MSG_OP_EXIT: {
            auto* event = static_cast<struct msg_exit*>(data);
            // TODO set into delete queue ...
            processCacheMgr->RecordExitEvent(event);
            break;
        }
        case MSG_OP_EXECVE: {
            auto* eventPtr = static_cast<struct msg_execve_event*>(data);
            processCacheMgr->RecordExecveEvent(eventPtr);
            break;
        }
        case MSG_OP_DATA: {
            auto* eventPtr = static_cast<msg_data*>(data);
            processCacheMgr->RecordDataEvent(eventPtr);
            break;
        }
        default: {
            LOG_WARNING(sLogger, ("Unknown event op", static_cast<int>(common->op)));
            break;
        }
    }
}

void HandleKernelProcessEventLost(void* ctx, int cpu, unsigned long long cnt) {
    auto* processCacheMgr = static_cast<ProcessCacheManager*>(ctx);
    if (!processCacheMgr) {
        LOG_ERROR(sLogger, ("ProcessCacheManager is null!", "")("lost events", cnt)("cpu", cpu));
        return;
    }
    processCacheMgr->UpdateLossEventTotal(cnt);
}

////////////////////////////////////////////////////////////////////////////////////////

void ProcessCacheManager::UpdateRecvEventTotal(uint64_t count) {
    ADD_COUNTER(mPollProcessEventsTotal, count);
}
void ProcessCacheManager::UpdateLossEventTotal(uint64_t count) {
    ADD_COUNTER(mLossProcessEventsTotal, count);
}

ProcessCacheManager::ProcessCacheManager(std::shared_ptr<SourceManager>& sm,
                                         const std::string& hostName,
                                         const std::string& hostPathPrefix,
                                         moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                         CounterPtr pollEventsTotal,
                                         CounterPtr lossEventsTotal,
                                         CounterPtr cacheMissTotal)
    : mSourceManager(sm),
      mProcParser(hostPathPrefix),
      mHostName(hostName),
      mHostPathPrefix(hostPathPrefix),
      mCommonEventQueue(queue),
      mPollProcessEventsTotal(pollEventsTotal),
      mLossProcessEventsTotal(lossEventsTotal),
      mProcessCacheMissTotal(cacheMissTotal) {
    mCache.reserve(kInitCacheSize);
    mDataMap.reserve(kInitDataMapSize);
}

bool ProcessCacheManager::Init() {
    if (mInited) {
        return true;
    }
    mInited = true;
    mFrequencyMgr.SetPeriod(std::chrono::milliseconds(100));
    auto ebpfConfig = std::make_unique<PluginConfig>();
    ebpfConfig->mPluginType = PluginType::PROCESS_SECURITY;
    ProcessConfig pconfig;

    pconfig.mPerfBufferSpec = {{"tcpmon_map", 128, this, HandleKernelProcessEvent, HandleKernelProcessEventLost}};
    ebpfConfig->mConfig = pconfig;
    mRunFlag = true;
    mPoller = async(std::launch::async, &ProcessCacheManager::pollPerfBuffers, this);
    bool status = mSourceManager->StartPlugin(PluginType::PROCESS_SECURITY, std::move(ebpfConfig));
    if (!status) {
        LOG_ERROR(sLogger, ("failed to start process security plugin", ""));
        return false;
    }
    auto ret = SyncAllProc();
    if (ret) {
        LOG_WARNING(sLogger, ("failed to sync all proc, ret", ret));
    }
    return true;
}

void ProcessCacheManager::pollPerfBuffers() {
    int zero = 0;
    LOG_DEBUG(sLogger, ("enter poller thread", ""));
    while (mRunFlag) {
        auto now = std::chrono::steady_clock::now();
        auto nextWindow = mFrequencyMgr.Next();
        if (!mFrequencyMgr.Expired(now)) {
            std::this_thread::sleep_until(nextWindow);
            mFrequencyMgr.Reset(nextWindow);
        } else {
            mFrequencyMgr.Reset(now);
        }
        auto ret = mSourceManager->PollPerfBuffers(
            PluginType::PROCESS_SECURITY, kMaxBatchConsumeSize, &zero, kMaxWaitTimeMS);
        LOG_DEBUG(sLogger, ("poll event num", ret));
    }
    LOG_DEBUG(sLogger, ("exit poller thread", ""));
}

void ProcessCacheManager::Stop() {
    if (!mInited) {
        return;
    }
    auto res = mSourceManager->StopPlugin(PluginType::PROCESS_SECURITY);
    LOG_INFO(sLogger, ("stop process probes for base manager, status", res));
    mRunFlag = false;
    std::future_status s1 = mPoller.wait_for(std::chrono::seconds(1));
    if (mPoller.valid()) {
        if (s1 == std::future_status::ready) {
            LOG_INFO(sLogger, ("poller thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("poller thread", "forced to stopped"));
        }
    }
    mInited = false;
}

void ProcessCacheManager::dataAdd(msg_data* dataPtr) {
    auto size = dataPtr->common.size - offsetof(msg_data, arg);
    if (size <= MSG_DATA_ARG_LEN) {
        // std::vector<uint64_t> key = {dataPtr->id.pid, dataPtr->id.time};
        std::lock_guard<std::mutex> lk(mDataMapMutex);
        auto res = mDataMap.find(dataPtr->id);
        if (res != mDataMap.end()) {
            auto& prevData = res->second;
            LOG_DEBUG(sLogger,
                      ("already have data, pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("prevData", prevData)(
                          "data", std::string(dataPtr->arg, size)));
            prevData.append(dataPtr->arg, size);
        } else {
            // restrict memory usage in abnormal conditions
            // if there is some unused old data, clear it
            // if we cannot clear old data, just clear all
            if (mDataMap.size() > kMaxDataMapSize
                && mLastDataMapClearTime < std::chrono::system_clock::now() - std::chrono::minutes(1)) {
                clearExpiredData(dataPtr->id.time);
                mLastDataMapClearTime = std::chrono::system_clock::now();
                LOG_WARNING(sLogger, ("data map size exceed limit", kInitDataMapSize)("size", mDataMap.size()));
            }
            if (mDataMap.size() > kMaxDataMapSize) {
                LOG_WARNING(sLogger, ("data map size exceed limit", kInitDataMapSize)("size", mDataMap.size()));
                mDataMap.clear();
            }
            LOG_DEBUG(sLogger,
                      ("no prev data, pid",
                       dataPtr->id.pid)("ktime", dataPtr->id.time)("data", std::string(dataPtr->arg, size)));
            mDataMap[dataPtr->id] = std::string(dataPtr->arg, size);
        }
    } else {
        LOG_ERROR(sLogger, ("pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("size limit exceeded", size));
    }
}

std::string ProcessCacheManager::dataGetAndRemove(data_event_desc* desc) {
    std::string data;
    {
        std::lock_guard<std::mutex> lk(mDataMapMutex);
        auto res = mDataMap.find(desc->id);
        if (res == mDataMap.end()) {
            return data;
        }
        data.swap(res->second);
        mDataMap.erase(res);
    }
    if (data.size() != desc->size - desc->leftover) {
        LOG_WARNING(sLogger, ("size bad! data size", data.size())("expect", desc->size - desc->leftover));
        return data;
    }

    return data;
}

void ProcessCacheManager::clearExpiredData(time_t ktime) {
    ktime -= kMaxCacheExpiredTimeout;
    for (auto it = mDataMap.begin(); it != mDataMap.end();) {
        if (time_t(it->first.time) < ktime) {
            it = mDataMap.erase(it);
        } else {
            ++it;
        }
    }
}

void ProcessCacheManager::RecordDataEvent(msg_data* eventPtr) {
    LOG_DEBUG(sLogger,
              ("[receive_data_event] size", eventPtr->common.size)("pid", eventPtr->id.pid)("time", eventPtr->id.time)(
                  "data", std::string(eventPtr->arg, eventPtr->common.size - offsetof(msg_data, arg))));
    dataAdd(eventPtr);
}

std::string DecodeArgs(std::string_view& rawArgs) {
    std::string args;
    if (rawArgs.empty()) {
        return args;
    }
    args.reserve(rawArgs.size() * 2);
    StringViewSplitter splitter(args, "\0");
    bool first = true;
    for (auto field : splitter) {
        if (first) {
            first = false;
        } else {
            args += " ";
        }
        if (field.find(' ') != std::string::npos || field.find('\t') != std::string::npos
            || field.find('\n') != std::string::npos) {
            args += "\"";
            for (char c : field) {
                if (c == '"' || c == '\\') {
                    args += '\\'; // Escape the character
                }
                if (c == '\n') {
                    args += "\\n";
                } else if (c == '\t') {
                    args += "\\t";
                } else {
                    args += c;
                }
            }
            args += "\"";
        } else {
            args += field;
        }
    }
    return args;
}

void ProcessCacheManager::RecordExecveEvent(msg_execve_event* eventPtr) {
    // copy msg
    auto event = std::make_shared<MsgExecveEventUnix>();
    static_assert(offsetof(msg_execve_event, buffer) == sizeof(MsgExecveEvent),
                  "offsetof(msg_execve_event, buffer) must be equal to sizeof(MsgExecveEvent)");
    std::memcpy(&event->msg, eventPtr, sizeof(MsgExecveEvent));

    // parse exec
    event->process.size = eventPtr->process.size;
    event->process.pid = eventPtr->process.pid;
    event->process.tid = eventPtr->process.tid;
    event->process.nspid = eventPtr->process.nspid;
    event->process.uid = eventPtr->process.uid;
    event->process.flags = eventPtr->process.flags;
    event->process.ktime = eventPtr->process.ktime;
    event->process.auid = eventPtr->process.auid;
    event->process.secure_exec = eventPtr->process.secureexec;
    event->process.nlink = eventPtr->process.i_nlink;
    event->process.ino = eventPtr->process.i_ino;

    // dockerid
    event->kube.docker = std::string(eventPtr->kube.docker_id);

#ifdef APSARA_UNIT_TEST_MAIN
    event->process.testFileName = mProcParser.GetPIDExePath(event->process.pid);
    event->process.testCmdline = mProcParser.GetPIDCmdline(event->process.pid);
#endif

    // args && filename
    // verifier size
    // SIZEOF_EVENT is the total size of all fixed fields, = offsetof(msg_process, args) = 56
    auto size = eventPtr->process.size - SIZEOF_EVENT; // remain size
    if (size > PADDED_BUFFER - SIZEOF_EVENT) { // size exceed args buffer size
        LOG_ERROR(sLogger,
                  ("error", "msg exec size larger than argsbuffer")("pid", event->process.pid)("ktime",
                                                                                               event->process.ktime));
        event->process.args = "enomem enomem";
        event->process.filename = "enomem";
        eventPtr->process.size = SIZEOF_EVENT;
        PostHandlerExecveEvent(eventPtr, std::move(event));
        return;
    }

    // executable filename
    char* buffer = eventPtr->buffer + SIZEOF_EVENT; // equivalent to eventPtr->process.args;
    if (eventPtr->process.flags & EVENT_DATA_FILENAME) { // filename should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_FILENAME", "msg exec size less than sizeof(data_event_desc)")(
                          "pid", event->process.pid)("ktime", event->process.ktime));
            event->process.args = "enomem enomem";
            event->process.filename = "enomem";
            eventPtr->process.size = SIZEOF_EVENT;
            PostHandlerExecveEvent(eventPtr, std::move(event));
            return;
        }
        auto* desc = reinterpret_cast<data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_FILENAME, size",
                   desc->size)("leftover", desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        auto data = dataGetAndRemove(desc);
        if (data.empty()) {
            LOG_WARNING(
                sLogger,
                ("EVENT_DATA_FILENAME", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        event->process.filename = data;
        buffer += sizeof(data_event_desc);
        size -= sizeof(data_event_desc);
    } else if ((eventPtr->process.flags & EVENT_ERROR_FILENAME) == 0) { // filename should be in process.args
        char* nullPos = std::find(buffer, buffer + size, '\0');
        event->process.filename = std::string(buffer, nullPos - buffer);
        size -= nullPos - buffer;
        if (size == 0) { // no tailing \0 found
            buffer = nullPos;
        } else {
            buffer = nullPos + 1; // skip \0
            --size;
        }
    } else {
        LOG_WARNING(
            sLogger,
            ("EVENT_DATA_FILENAME", "ebpf get data error")("pid", event->process.pid)("ktime", event->process.ktime));
    }

    // args & cmd
    std::string data;
    std::string_view args;
    std::string_view cwd;
    if (eventPtr->process.flags & EVENT_DATA_ARGS) { // arguments should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_ARGS", "msg exec size less than sizeof(data_event_desc)")("pid", event->process.pid)(
                          "ktime", event->process.ktime));
            event->process.args = "enomem enomem";
            eventPtr->process.size = SIZEOF_EVENT;
            PostHandlerExecveEvent(eventPtr, std::move(event));
            return;
        }
        auto* desc = reinterpret_cast<data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_ARGS, size", desc->size)("leftover",
                                                        desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        data = dataGetAndRemove(desc);
        if (data.empty()) {
            LOG_WARNING(sLogger,
                        ("EVENT_DATA_ARGS", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        args = data;
        // the remaining data is cwd
        cwd = std::string_view(buffer + sizeof(data_event_desc), size - sizeof(data_event_desc));
    } else {
        bool hasCwd = false;
        if (((eventPtr->process.flags & EVENT_NO_CWD_SUPPORT) | (eventPtr->process.flags & EVENT_ERROR_CWD)
             | (eventPtr->process.flags & EVENT_ROOT_CWD))
            == 0) {
            hasCwd = true;
        }
        char* nullPos = nullptr;
        args = std::string_view(buffer, size - 1); // excluding tailing zero
        if (hasCwd) {
            // find the last \0 to serapate args and cwd
            for (int i = size - 1; i >= 0; i--) {
                if (buffer[i] == '\0') {
                    nullPos = buffer + i;
                    break;
                }
            }
            if (nullPos == nullptr) {
                cwd = std::string_view(buffer, size);
                args = std::string_view(buffer, 0);
            } else {
                cwd = std::string_view(nullPos + 1, size - (nullPos - buffer + 1));
                args = std::string_view(buffer, nullPos - buffer);
            }
        }
    }
    // Post handle cwd
    if (eventPtr->process.flags & EVENT_ROOT_CWD) {
        event->process.cwd = "/";
    } else if (eventPtr->process.flags & EVENT_PROCFS) {
        event->process.cwd = Trim(cwd);
    } else {
        event->process.cwd = cwd;
    }
    // Post handle args
    event->process.args = DecodeArgs(args);
    if (eventPtr->process.flags & EVENT_ERROR_ARGS) {
        LOG_WARNING(
            sLogger,
            ("EVENT_DATA_ARGS", "ebpf get data error")("pid", event->process.pid)("ktime", event->process.ktime));
    }
    if (eventPtr->process.flags & EVENT_ERROR_CWD) {
        LOG_WARNING(
            sLogger,
            ("EVENT_DATA_CWD", "ebpf get data error")("pid", event->process.pid)("ktime", event->process.ktime));
    }
    if (eventPtr->process.flags & EVENT_ERROR_PATH_COMPONENTS) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_CWD",
                     "cwd too long, maybe truncated")("pid", event->process.pid)("ktime", event->process.ktime));
    }
    PostHandlerExecveEvent(eventPtr, std::move(event));
}

void ProcessCacheManager::PostHandlerExecveEvent(msg_execve_event* eventPtr,
                                                 std::shared_ptr<MsgExecveEventUnix>&& event) {
    // set binary
    if (event->process.filename.size()) {
        if (event->process.filename[0] == '/') {
            event->process.binary = event->process.filename;
        } else if (!event->process.cwd.empty()) {
            event->process.binary.reserve(event->process.cwd.size() + 1 + event->process.filename.size());
            event->process.binary.assign(event->process.cwd).append("/").append(event->process.filename);
        }
    } else {
        LOG_WARNING(sLogger,
                    ("filename is empty, should not happen. pid", event->process.pid)("ktime", event->process.ktime));
    }

    LOG_DEBUG(
        sLogger,
        ("begin enqueue pid", event->process.pid)("ktime", event->process.ktime)("cmdline", event->process.cmdline)(
            "filename", event->process.filename)("dockerid", event->kube.docker)("args", event->process.args)(
            "cwd", event->process.cwd)("flag", eventPtr->process.flags)
#ifdef APSARA_UNIT_TEST_MAIN
            ("procParser.exePath", event->process.testFileName)("procParser.cmdLine", event->process.testCmdline)
#endif
    );

    handleCacheUpdate(std::move(event));
    clearExpiredCache(eventPtr->process.ktime);
    // restrict memory usage in abnormal conditions
    // if we cannot clear old data, just clear all
    // TODO: maybe we can iterate over the /proc folder and remove unexisting entries
    if (mCache.size() > kMaxCacheSize) {
        LOG_WARNING(sLogger, ("process cache size exceed limit", kMaxCacheSize)("size", mCache.size()));
        clearCache();
    }

    if (mFlushProcessEvent) {
        auto processEvent = std::make_shared<ProcessEvent>(eventPtr->process.pid,
                                                           eventPtr->process.ktime,
                                                           KernelEventType::PROCESS_EXECVE_EVENT,
                                                           eventPtr->common.ktime);
        if (event) {
            mCommonEventQueue.enqueue(std::move(processEvent));
        }
    }
}

void ProcessCacheManager::RecordExitEvent(msg_exit* eventPtr) {
    enqueueExpiredEntry({eventPtr->current.pid, eventPtr->current.ktime}, eventPtr->common.ktime);
    if (mFlushProcessEvent) {
        auto event = std::make_shared<ProcessExitEvent>(eventPtr->current.pid,
                                                        eventPtr->current.ktime,
                                                        KernelEventType::PROCESS_EXIT_EVENT,
                                                        eventPtr->common.ktime,
                                                        eventPtr->info.code,
                                                        eventPtr->info.tid);
        if (event) {
            mCommonEventQueue.enqueue(std::move(event));
        }
    }
}

void ProcessCacheManager::RecordCloneEvent(msg_clone_event* eventPtr) {
    // auto currentPid = event_ptr->tgid;
    // auto currentKtime = event_ptr->ktime;
    // auto parentPid = event_ptr->parent.pid;
    // auto parentKtime = event_ptr->parent.ktime;

    // auto event = std::make_unique<MsgExecveEventUnix>();
    // LOG_DEBUG(
    //     sLogger,
    //     ("begin enqueue pid", event->process.pid)("ktime", event->process.ktime)("cmdline", event->process.cmdline)(
    //         "filename", event->process.filename)("raw_args", raw_args)("cwd", cwd)("dockerid", event->kube.docker));

    // mRecordQueue.enqueue(std::move(event));

    if (mFlushProcessEvent) {
        uint32_t pid = eventPtr->tgid;
        uint64_t kt = eventPtr->ktime;
        auto event = std::make_shared<ProcessEvent>(
            pid, kt, KernelEventType::PROCESS_CLONE_EVENT, eventPtr->common.ktime);
        if (event) {
            mCommonEventQueue.enqueue(std::move(event));
        }
    }
}

std::string ReplaceCmdlineWithAbsExe(const std::string& exe, const std::string& cmdline) {
    std::string res;
    auto idx = cmdline.find('\0');
    if (idx == std::string::npos) {
        res = exe;
    } else {
        res = exe + cmdline.substr(idx);
    }
    return res;
}

std::vector<std::shared_ptr<Proc>> ProcessCacheManager::ListRunningProcs() {
    std::vector<std::shared_ptr<Proc>> processes;
    for (const auto& entry : std::filesystem::directory_iterator(mHostPathPrefix / "proc")) {
        if (!entry.is_directory()) {
            continue;
        }
        auto dirName = entry.path().filename().string();
        int32_t pid = 0;
        const char* dirNameLast = dirName.data() + dirName.size();
        auto convresult = std::from_chars(dirName.data(), dirNameLast, pid);
        if (convresult.ec != std::errc() || convresult.ptr != dirNameLast) {
            continue;
        }

        auto procPtr = std::make_shared<Proc>();
        if (mProcParser.ParseProc(pid, *procPtr)) {
            processes.emplace_back(procPtr);
        }
    }
    LOG_DEBUG(sLogger, ("Read ProcFS prefix", mHostPathPrefix)("append process cnt", processes.size()));

    return processes;
}

int ProcessCacheManager::WriteProcToBPFMap(const std::shared_ptr<Proc>& proc) {
    // Proc -> execve_map_value
    execve_map_value value{};
    value.pkey.pid = proc->ppid;
    value.pkey.ktime = proc->pktime;
    value.key.pid = proc->pid;
    value.key.ktime = proc->ktime;
    value.flags = 0;
    value.nspid = proc->nspid;
    value.caps = {{{proc->permitted, proc->effective, proc->inheritable}}};
    value.ns = {{{proc->uts_ns,
                  proc->ipc_ns,
                  proc->mnt_ns,
                  proc->pid,
                  proc->pid_for_children_ns,
                  proc->net_ns,
                  proc->time_ns,
                  proc->time_for_children_ns,
                  proc->cgroup_ns,
                  proc->user_ns}}};
    value.bin.path_length = proc->exe.size();
    ::memcpy(value.bin.path, proc->exe.data(), std::min(BINARY_PATH_MAX_LEN, static_cast<int>(proc->exe.size())));

    // update bpf map
    int res = mSourceManager->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &proc->pid, &value, 0);
    LOG_DEBUG(sLogger, ("update bpf map, pid", proc->pid)("res", res));
    return res;
}


int ProcessCacheManager::SyncAllProc() {
    std::vector<std::shared_ptr<Proc>> procs = ListRunningProcs();
    // update execve map
    for (auto& proc : procs) {
        WriteProcToBPFMap(proc);
    }
    // add kernel thread (pid 0)
    msg_execve_key key{};
    key.pid = 0;
    key.ktime = 0;
    execve_map_value value{};
    value.pkey.pid = 0;
    value.pkey.ktime = 1;
    value.key.pid = 0;
    value.key.ktime = 1;
    mSourceManager->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &key.pid, &value, 0);

    // generage execve event ...
    for (const auto& proc : procs) {
        PushExecveEvent(*proc);
    }

    return 0;
}

void ProcessCacheManager::ProcToExecveEvent(const Proc& proc, MsgExecveEventUnix& event) {
    std::string_view rawArgs = proc.cmdline;
    auto nullPos = rawArgs.find('\0');
    if (nullPos != std::string::npos) {
        rawArgs = rawArgs.substr(nullPos + 1);
    } else {
        rawArgs = rawArgs.substr(rawArgs.size(), 0);
    }

    event.process.args = DecodeArgs(rawArgs);
    LOG_DEBUG(sLogger, ("raw_args", rawArgs)("args", event.process.args));

    if (proc.cmdline.empty()) {
        event.kernel_thread = true;
        event.msg.parent.pid = proc.ppid;
        event.msg.parent.ktime = proc.ktime;
        event.process.pid = proc.pid;
        event.process.tid = proc.tid;
        event.process.nspid = proc.nspid;
        event.process.uid = 0;
        event.process.auid = std::numeric_limits<uint32_t>::max();
        event.process.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS);
        event.process.ktime = proc.ktime;
        event.process.cwd = proc.cwd;
        event.process.filename = proc.comm;
        event.process.binary = proc.comm;
    } else {
        event.kernel_thread = false;
        event.msg.common.op = MSG_OP_EXECVE;
        event.msg.common.size = SIZEOF_EVENT;
        event.kube.docker = proc.container_id;
        event.msg.parent.pid = proc.ppid;
        event.msg.parent.ktime = proc.pktime;
        event.msg.ns.uts_inum = proc.uts_ns;
        event.msg.ns.ipc_inum = proc.ipc_ns;
        event.msg.ns.mnt_inum = proc.mnt_ns;
        event.msg.ns.pid_inum = proc.pid_ns;
        event.msg.ns.pid_for_children_inum = proc.pid_for_children_ns;
        event.msg.ns.net_inum = proc.net_ns;
        event.msg.ns.time_inum = proc.time_ns;
        event.msg.ns.time_for_children_inum = proc.time_for_children_ns;
        event.msg.ns.cgroup_inum = proc.cgroup_ns;
        event.msg.ns.user_inum = proc.user_ns;
        event.process.pid = proc.pid;
        event.process.tid = proc.tid;
        event.process.nspid = proc.nspid;
        event.process.uid = proc.uids[1];
        event.process.auid = proc.auid;
        event.msg.creds.uid = proc.uids[0];
        event.msg.creds.gid = proc.uids[1];
        event.msg.creds.suid = proc.uids[2];
        event.msg.creds.sgid = proc.uids[3];
        event.msg.creds.euid = proc.gids[0];
        event.msg.creds.egid = proc.gids[1];
        event.msg.creds.fsuid = proc.gids[2];
        event.msg.creds.fsgid = proc.gids[3];
        event.msg.creds.caps.permitted = proc.permitted;
        event.msg.creds.caps.effective = proc.effective;
        event.msg.creds.caps.inheritable = proc.inheritable;
        event.process.flags = proc.flags;
        event.process.ktime = proc.ktime;
        event.msg.common.ktime = proc.ktime;
        event.process.filename = proc.exe;
        event.process.binary = proc.exe;
        event.process.cmdline = proc.cmdline;
        event.process.cwd = proc.cwd;
    }
}

void ProcessCacheManager::PushExecveEvent(const Proc& proc) {
    auto event = std::make_shared<MsgExecveEventUnix>();
    ProcToExecveEvent(proc, *event);
    handleCacheUpdate(std::move(event));
}

std::string ProcessCacheManager::GenerateParentExecId(const std::shared_ptr<MsgExecveEventUnix>& event) {
    if (event->msg.cleanup_key.ktime == 0 || event->process.flags & EVENT_CLONE) {
        return GenerateExecId(event->msg.parent.pid, event->msg.parent.ktime);
    }
    return GenerateExecId(event->msg.cleanup_key.pid, event->msg.cleanup_key.ktime);
}

std::string ProcessCacheManager::GenerateExecId(uint32_t pid, uint64_t ktime) {
    // /proc/sys/kernel/pid_max is usually 7 digits 4194304
    // nano timestamp is usually 19 digits
    std::string execid;
    execid.reserve(mHostName.size() + 1 + 19 + 1 + 7);
    execid.assign(mHostName).append(":").append(std::to_string(ktime)).append(":").append(std::to_string(pid));
    return Base64Enconde(execid);
}

// consume record queue
void ProcessCacheManager::handleCacheUpdate(std::shared_ptr<MsgExecveEventUnix>&& event) {
    event->process.user.name = mProcParser.GetUserNameByUid(event->process.uid);
    event->exec_id = GenerateExecId(event->process.pid, event->process.ktime);
    event->parent_exec_id = GenerateParentExecId(event);
    LOG_DEBUG(sLogger,
              ("[RecordExecveEvent][DUMP] begin update cache pid", event->process.pid)("ktime", event->process.ktime)(
                  "execId", event->exec_id)("cmdline", mProcParser.GetPIDCmdline(event->process.pid))(
                  "filename", mProcParser.GetPIDExePath(event->process.pid))("args", event->process.args));
    updateCache({event->process.pid, event->process.ktime}, std::move(event));
}

void ProcessCacheManager::clearExpiredCache(time_t ktime) {
    ktime -= kMaxCacheExpiredTimeout;
    if (mCacheExpireQueue.empty() || mCacheExpireQueue.front().time > ktime) {
        return;
    }
    while (!mCacheExpireQueue.empty() && mCacheExpireQueue.front().time <= ktime) {
        auto& key = mCacheExpireQueue.front().key;
        LOG_DEBUG(sLogger, ("[RecordExecveEvent][DUMP] clear expired cache pid", key.pid)("ktime", key.time));
        releaseCache(key);
        mCacheExpireQueue.pop_front();
    }
}

void ProcessCacheManager::releaseCache(const data_event_id& key) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.erase(key);
}

void ProcessCacheManager::updateCache(const data_event_id& key, std::shared_ptr<MsgExecveEventUnix>&& value) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.emplace(key, std::move(value));
}

void ProcessCacheManager::enqueueExpiredEntry(const data_event_id& key, time_t ktime) {
    mCacheExpireQueue.emplace_back(ExitedEntry{ktime, key});
}

void ProcessCacheManager::clearCache() {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.clear();
}

SizedMap ProcessCacheManager::FinalizeProcessTags(std::shared_ptr<SourceBuffer>& sb, uint32_t pid, uint64_t ktime) {
    static const std::string kUnkownStr = "unknown";

    SizedMap res;
    auto proc = LookupCache({pid, ktime});
    if (!proc) {
        ADD_COUNTER(mProcessCacheMissTotal, 1);
        LOG_WARNING(sLogger,
                    ("cannot find proc in cache, pid",
                     pid)("ktime", ktime)("contains", proc.get() != nullptr)("size", mCache.size()));
        return res;
    }

    // event_type, added by xxx_security_manager
    // call_name, added by xxx_security_manager
    // event_time, added by xxx_security_manager

    // finalize proc tags
    auto execIdSb = sb->CopyString(proc->exec_id);
    res.Insert(kExecId.LogKey(), StringView(execIdSb.data, execIdSb.size));

    auto pidSb = sb->CopyString(std::to_string(proc->process.pid));
    res.Insert(kProcessId.LogKey(), StringView(pidSb.data, pidSb.size));

    auto uidSb = sb->CopyString(std::to_string(proc->process.uid));
    res.Insert(kUid.LogKey(), StringView(uidSb.data, uidSb.size));

    auto userSb = sb->CopyString(proc->process.user.name);
    res.Insert(kUser.LogKey(), StringView(userSb.data, userSb.size));

    auto binarySb = sb->CopyString(proc->process.filename);
    res.Insert(kBinary.LogKey(), StringView(binarySb.data, binarySb.size));

    auto argsSb = sb->CopyString(proc->process.args);
    res.Insert(kArguments.LogKey(), StringView(argsSb.data, argsSb.size));

    auto cwdSb = sb->CopyString(proc->process.cwd);
    res.Insert(kCWD.LogKey(), StringView(cwdSb.data, cwdSb.size));

    auto ktimeSb = sb->CopyString(std::to_string(proc->process.ktime));
    res.Insert(kKtime.LogKey(), StringView(ktimeSb.data, ktimeSb.size));

    auto permitted = GetCapabilities(proc->msg.creds.caps.permitted, *sb);
    res.Insert(kCapPermitted.LogKey(), permitted);

    auto effective = GetCapabilities(proc->msg.creds.caps.effective, *sb);
    res.Insert(kCapEffective.LogKey(), effective);

    auto inheritable = GetCapabilities(proc->msg.creds.caps.inheritable, *sb);
    res.Insert(kCapInheritable.LogKey(), inheritable);

    auto parentProc = LookupCache({proc->msg.parent.pid, proc->msg.parent.ktime});
    // for parent
    if (!parentProc) {
        return res;
    }
    // finalize parent tags
    auto parentExecIdSb = sb->CopyString(parentProc->exec_id);
    res.Insert(kParentExecId.LogKey(), StringView(parentExecIdSb.data, parentExecIdSb.size));

    auto parentPidSb = sb->CopyString(std::to_string(parentProc->process.pid));
    res.Insert(kParentProcessId.LogKey(), StringView(parentPidSb.data, parentPidSb.size));

    auto parentUidSb = sb->CopyString(std::to_string(parentProc->process.uid));
    res.Insert(kParentUid.LogKey(), StringView(parentUidSb.data, parentUidSb.size));

    auto parentUserSb = sb->CopyString(parentProc->process.user.name);
    res.Insert(kParentUser.LogKey(), StringView(parentUserSb.data, parentUserSb.size));

    auto parentBinarySb = sb->CopyString(parentProc->process.filename);
    res.Insert(kParentBinary.LogKey(), StringView(parentBinarySb.data, parentBinarySb.size));

    auto parentArgsSb = sb->CopyString(parentProc->process.args);
    res.Insert(kParentArguments.LogKey(), StringView(parentArgsSb.data, parentArgsSb.size));

    auto parentCwdSb = sb->CopyString(parentProc->process.cwd);
    res.Insert(kParentCWD.LogKey(), StringView(parentCwdSb.data, parentCwdSb.size));

    auto parentKtimeSb = sb->CopyString(std::to_string(parentProc->process.ktime));
    res.Insert(kParentKtime.LogKey(), StringView(parentKtimeSb.data, parentKtimeSb.size));

    auto parentPermitted = GetCapabilities(parentProc->msg.creds.caps.permitted, *sb);
    res.Insert(kParentCapPermitted.LogKey(), parentPermitted);

    auto parentEffective = GetCapabilities(parentProc->msg.creds.caps.effective, *sb);
    res.Insert(kParentCapEffective.LogKey(), parentEffective);

    auto parentInheritable = GetCapabilities(parentProc->msg.creds.caps.inheritable, *sb);
    res.Insert(kParentCapInheritable.LogKey(), parentInheritable);

    return res;
}

} // namespace ebpf
} // namespace logtail
