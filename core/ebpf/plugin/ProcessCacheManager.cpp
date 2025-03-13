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

#include "StringView.h"
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

static constexpr size_t kInitDataMapSize = 1024UL;
static constexpr size_t kMaxCacheSize = 4194304UL;
static constexpr size_t kMaxDataMapSize = kInitDataMapSize * 4;
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

constexpr size_t kArgOffset = offsetof(msg_data, arg);

void ProcessCacheManager::dataAdd(msg_data* dataPtr) {
    if (dataPtr->common.size < kArgOffset) {
        LOG_ERROR(sLogger, ("size is negative, dataPtr.common.size", dataPtr->common.size)("arg offset", kArgOffset));
        return;
    }
    size_t size = dataPtr->common.size - offsetof(msg_data, arg);
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

std::string ProcessCacheManager::dataGetAndRemove(const data_event_desc* desc) {
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
        return "";
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

std::string DecodeArgs(StringView& rawArgs) {
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
            args.append(field.data(), field.size());
        }
    }
    return args;
}

static bool hasIndependentParent(const msg_execve_event& event) {
    return event.cleanup_key.ktime == 0 || (event.process.flags & EVENT_CLONE) != 0;
}

void ProcessCacheManager::RecordExecveEvent(msg_execve_event* eventPtr) {
    if (hasIndependentParent(*eventPtr)) {
        mProcessCache.IncRef({eventPtr->parent.pid, eventPtr->parent.ktime});
    } else {
        auto parent = mProcessCache.Lookup({eventPtr->cleanup_key.pid, eventPtr->cleanup_key.ktime});
        if (parent) { // dec grand parent's ref count
            mProcessCache.DecRef({parent->mPPid, parent->mPKtime}, eventPtr->process.ktime);
        }
    }
    auto cacheValue = msgExecveEventToProcessCacheValue(*eventPtr);
    mProcessCache.AddCache({eventPtr->process.pid, eventPtr->process.ktime}, std::move(cacheValue));
    if (mFlushProcessEvent) {
        auto processEvent = std::make_shared<ProcessEvent>(eventPtr->process.pid,
                                                           eventPtr->process.ktime,
                                                           KernelEventType::PROCESS_EXECVE_EVENT,
                                                           eventPtr->common.ktime);
        mCommonEventQueue.enqueue(std::move(processEvent));
    }
    mProcessCache.ClearExpiredCache(eventPtr->process.ktime);
    // restrict memory usage in abnormal conditions
    // if we cannot clear old data, just clear all
    // TODO: maybe we can iterate over the /proc folder and remove unexisting entries
    if (mProcessCache.Size() > kMaxCacheSize) {
        LOG_WARNING(sLogger, ("process cache size exceed limit", kMaxCacheSize)("size", mProcessCache.Size()));
        mProcessCache.ClearCache();
    }
}

std::shared_ptr<ProcessCacheValue>
ProcessCacheManager::msgExecveEventToProcessCacheValue(const msg_execve_event& event) {
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    if (hasIndependentParent(event)) {
        cacheValue->mPPid = event.parent.pid;
        cacheValue->mPKtime = event.parent.ktime;
    } else { // process created from execve only
        cacheValue->mPPid = event.cleanup_key.pid;
        cacheValue->mPKtime = event.cleanup_key.ktime;
    }

    auto execId = GenerateExecId(event.process.pid, event.process.ktime);
    auto userName = mProcParser.GetUserNameByUid(event.process.uid);
    auto permitted = GetCapabilities(event.creds.caps.permitted, *cacheValue->mSourceBuffer);
    auto effective = GetCapabilities(event.creds.caps.effective, *cacheValue->mSourceBuffer);
    auto inheritable = GetCapabilities(event.creds.caps.inheritable, *cacheValue->mSourceBuffer);

    fillProcessDataFields(event, *cacheValue);
    cacheValue->SetContent(kExecId, execId);
    cacheValue->SetContent(kProcessId, event.process.pid);
    cacheValue->SetContent(kUid, event.process.uid);
    cacheValue->SetContent(kUser, userName);
    cacheValue->SetContent(kKtime, event.process.ktime);
    cacheValue->SetContentNoCopy(kCapPermitted, permitted);
    cacheValue->SetContentNoCopy(kCapEffective, effective);
    cacheValue->SetContentNoCopy(kCapInheritable, inheritable);
    // parse exec
    // event->process.tid = eventPtr->process.tid;
    // event->process.nspid = eventPtr->process.nspid;
    // event->process.auid = eventPtr->process.auid;
    // event->process.secure_exec = eventPtr->process.secureexec;
    // event->process.nlink = eventPtr->process.i_nlink;
    // event->process.ino = eventPtr->process.i_ino;

    // dockerid
    // event->kube.docker = std::string(eventPtr->kube.docker_id);
    return cacheValue;
}

bool ProcessCacheManager::fillProcessDataFields(const msg_execve_event& event, ProcessCacheValue& cacheValue) {
    // args && filename
    static const StringView kENoMem = "enomem";
    thread_local std::string filename;
    thread_local std::string argsdata;
    StringView args;
    StringView cwd;
    // verifier size
    // SIZEOF_EVENT is the total size of all fixed fields, = offsetof(msg_process, args) = 56
    auto size = event.process.size - SIZEOF_EVENT; // remain size
    if (size > PADDED_BUFFER - SIZEOF_EVENT) { // size exceed args buffer size
        LOG_ERROR(
            sLogger,
            ("error", "msg exec size larger than argsbuffer")("pid", event.process.pid)("ktime", event.process.ktime));
        cacheValue.SetContentNoCopy(kBinary, kENoMem);
        cacheValue.SetContentNoCopy(kArguments, kENoMem);
        cacheValue.SetContentNoCopy(kCWD, kENoMem);
        return false;
    }

    // executable filename
    const char* buffer = event.buffer + SIZEOF_EVENT; // equivalent to eventPtr->process.args;
    if (event.process.flags & EVENT_DATA_FILENAME) { // filename should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_FILENAME", "msg exec size less than sizeof(data_event_desc)")(
                          "pid", event.process.pid)("ktime", event.process.ktime));
            cacheValue.SetContentNoCopy(kBinary, kENoMem);
            cacheValue.SetContentNoCopy(kArguments, kENoMem);
            cacheValue.SetContentNoCopy(kCWD, kENoMem);
            return false;
        }
        auto* desc = reinterpret_cast<const data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_FILENAME, size",
                   desc->size)("leftover", desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        filename = dataGetAndRemove(desc);
        if (filename.empty()) {
            LOG_WARNING(
                sLogger,
                ("EVENT_DATA_FILENAME", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        buffer += sizeof(data_event_desc);
        size -= sizeof(data_event_desc);
    } else if ((event.process.flags & EVENT_ERROR_FILENAME) == 0) { // filename should be in process.args
        const char* nullPos = std::find(buffer, buffer + size, '\0');
        filename = std::string(buffer, nullPos - buffer);
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
            ("EVENT_DATA_FILENAME", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
        filename.clear();
    }

    // args & cmd
    if (event.process.flags & EVENT_DATA_ARGS) { // arguments should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_ARGS", "msg exec size less than sizeof(data_event_desc)")("pid", event.process.pid)(
                          "ktime", event.process.ktime));
            cacheValue.SetContent(kBinary, filename);
            cacheValue.SetContentNoCopy(kArguments, kENoMem);
            cacheValue.SetContentNoCopy(kCWD, kENoMem);
            return false;
        }
        const auto* desc = reinterpret_cast<const data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_ARGS, size", desc->size)("leftover",
                                                        desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        argsdata = dataGetAndRemove(desc);
        if (argsdata.empty()) {
            LOG_WARNING(sLogger,
                        ("EVENT_DATA_ARGS", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        args = argsdata;
        // the remaining data is cwd
        cwd = StringView(buffer + sizeof(data_event_desc), size - sizeof(data_event_desc));
    } else {
        bool hasCwd = false;
        if (((event.process.flags & EVENT_NO_CWD_SUPPORT) | (event.process.flags & EVENT_ERROR_CWD)
             | (event.process.flags & EVENT_ROOT_CWD))
            == 0) {
            hasCwd = true;
        }
        const char* nullPos = nullptr;
        args = StringView(buffer, size - 1); // excluding tailing zero
        if (hasCwd) {
            // find the last \0 to serapate args and cwd
            for (int i = size - 1; i >= 0; i--) {
                if (buffer[i] == '\0') {
                    nullPos = buffer + i;
                    break;
                }
            }
            if (nullPos == nullptr) {
                cwd = StringView(buffer, size);
                args = StringView(buffer, 0);
            } else {
                cwd = StringView(nullPos + 1, size - (nullPos - buffer + 1));
                args = StringView(buffer, nullPos - buffer);
            }
        }
    }
    if (event.process.flags & EVENT_ERROR_ARGS) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_ARGS", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
    }
    if (event.process.flags & EVENT_ERROR_CWD) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_CWD", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
    }
    if (event.process.flags & EVENT_ERROR_PATH_COMPONENTS) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_CWD", "cwd too long, maybe truncated")("pid", event.process.pid)("ktime",
                                                                                                  event.process.ktime));
    }

    // Post handle cwd
    if (event.process.flags & EVENT_ROOT_CWD) {
        cwd = "/";
    } else if (event.process.flags & EVENT_PROCFS) {
        cwd = Trim(cwd);
    }
    cacheValue.SetContent(kCWD, cwd);
    // Post handle args
    cacheValue.SetContent(kArguments, DecodeArgs(args));
    // Post handle binary
    if (filename.size()) {
        if (filename[0] == '/') {
            ;
        } else if (!cwd.empty()) {
            // argsdata is not used anymore, as args and cwd has already been SetContent
            argsdata.reserve(cwd.size() + 1 + filename.size());
            argsdata.assign(cwd.data(), cwd.size()).append("/").append(filename);
            filename.swap(argsdata);
        }
        cacheValue.SetContent(kBinary, filename);
    } else {
        LOG_WARNING(sLogger,
                    ("filename is empty, should not happen. pid", event.process.pid)("ktime", event.process.ktime));
        cacheValue.SetContentNoCopy(kBinary, kENoMem);
        return false;
    }
    return true;
}

void ProcessCacheManager::RecordExitEvent(msg_exit* eventPtr) {
    auto cacheValue = mProcessCache.Lookup({eventPtr->current.pid, eventPtr->current.ktime});
    if (cacheValue) { // dec self and parent's ref
        mProcessCache.DecRef({cacheValue->mPPid, cacheValue->mPKtime}, eventPtr->common.ktime);
        mProcessCache.DecRef({eventPtr->current.pid, eventPtr->current.ktime}, eventPtr->common.ktime);
    }
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
    mProcessCache.ClearExpiredCache(eventPtr->common.ktime);
}

void ProcessCacheManager::RecordCloneEvent(msg_clone_event* eventPtr) {
    auto cacheValue = msgCloneEventToProcessCacheValue(*eventPtr);
    if (!cacheValue) {
        return;
    }
    mProcessCache.IncRef({eventPtr->parent.pid, eventPtr->parent.ktime});
    mProcessCache.AddCache({eventPtr->tgid, eventPtr->ktime}, std::move(cacheValue));
    if (mFlushProcessEvent) {
        auto event = std::make_shared<ProcessEvent>(
            eventPtr->tgid, eventPtr->ktime, KernelEventType::PROCESS_CLONE_EVENT, eventPtr->common.ktime);
        if (event) {
            mCommonEventQueue.enqueue(std::move(event));
        }
    }
    mProcessCache.ClearExpiredCache(eventPtr->common.ktime);
    // restrict memory usage in abnormal conditions
    // if we cannot clear old data, just clear all
    // TODO: maybe we can iterate over the /proc folder and remove unexisting entries
    if (mProcessCache.Size() > kMaxCacheSize) {
        LOG_WARNING(sLogger, ("process cache size exceed limit", kMaxCacheSize)("size", mProcessCache.Size()));
        mProcessCache.ClearCache();
    }
}

std::shared_ptr<ProcessCacheValue> ProcessCacheManager::msgCloneEventToProcessCacheValue(const msg_clone_event& event) {
    auto parent = mProcessCache.Lookup({event.parent.pid, event.parent.ktime});
    if (!parent) {
        LOG_WARNING(sLogger,
                    ("parent process not found. ppid",
                     event.parent.pid)("pktime", event.parent.ktime)("pid", event.tgid)("ktime", event.ktime));
        return nullptr;
    }
    auto execId = GenerateExecId(event.tgid, event.ktime);
    auto cacheValue = std::shared_ptr<ProcessCacheValue>(parent->CloneContents());
    cacheValue->mPPid = event.parent.pid;
    cacheValue->mPKtime = event.parent.ktime;
    cacheValue->SetContent(kExecId, execId);
    cacheValue->SetContent(kProcessId, event.tgid);
    cacheValue->SetContent(kKtime, event.ktime);
    return cacheValue;
}

std::vector<std::shared_ptr<Proc>> ProcessCacheManager::ListRunningProcs() {
    std::vector<std::shared_ptr<Proc>> processes;
    for (const auto& entry : std::filesystem::directory_iterator(mHostPathPrefix / "proc")) {
        if (!entry.is_directory()) {
            continue;
        }
        std::string dirName = entry.path().filename().string();
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

std::shared_ptr<ProcessCacheValue> ProcessCacheManager::procToProcessCacheValue(const Proc& proc) {
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    auto execId = GenerateExecId(proc.pid, proc.ktime);
    StringView rawArgs = proc.cmdline;
    auto nullPos = rawArgs.find('\0');
    if (nullPos != std::string::npos) {
        rawArgs = rawArgs.substr(nullPos + 1);
    } else {
        rawArgs = rawArgs.substr(rawArgs.size(), 0);
    }

    cacheValue->mPPid = proc.ppid;
    cacheValue->mPKtime = proc.pktime;
    cacheValue->SetContent(kArguments, DecodeArgs(rawArgs));
    LOG_DEBUG(sLogger, ("raw_args", rawArgs)("args", (*cacheValue)[kArguments]));
    cacheValue->SetContent(kExecId, execId);
    cacheValue->SetContent(kProcessId, proc.pid);
    cacheValue->SetContent(kCWD, proc.cwd);
    cacheValue->SetContent(kKtime, proc.ktime);

    if (proc.cmdline.empty()) {
        cacheValue->SetContent(kBinary, proc.comm);
        // event.process.nspid = proc.nspid;
        cacheValue->SetContent(kUid, 0);
        auto userName = mProcParser.GetUserNameByUid(0);
        cacheValue->SetContent(kUser, userName);
        // event.process.auid = std::numeric_limits<uint32_t>::max();
        // event.process.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS);

    } else {
        cacheValue->SetContent(kBinary, proc.exe);
        cacheValue->SetContent(kUid, proc.uids[1]);
        auto userName = mProcParser.GetUserNameByUid(proc.uids[1]);
        auto permitted = GetCapabilities(proc.permitted, *cacheValue->mSourceBuffer);
        auto effective = GetCapabilities(proc.effective, *cacheValue->mSourceBuffer);
        auto inheritable = GetCapabilities(proc.inheritable, *cacheValue->mSourceBuffer);
        cacheValue->SetContentNoCopy(kCapPermitted, permitted);
        cacheValue->SetContentNoCopy(kCapEffective, effective);
        cacheValue->SetContentNoCopy(kCapInheritable, inheritable);

        // event.process.nspid = proc.nspid;
        // event.process.auid = proc.auid;
        // event.process.flags = proc.flags;
        // event.process.cmdline = proc.cmdline;
        // event.kube.docker = proc.container_id;
    }
    return cacheValue;
}

void ProcessCacheManager::PushExecveEvent(const Proc& proc) {
    std::shared_ptr<ProcessCacheValue> cacheValue = procToProcessCacheValue(proc);
    mProcessCache.AddCache({proc.pid, proc.ktime}, std::move(cacheValue));
}

std::string ProcessCacheManager::GenerateExecId(uint32_t pid, uint64_t ktime) {
    // /proc/sys/kernel/pid_max is usually 7 digits 4194304
    // nano timestamp is usually 19 digits
    std::string execid;
    execid.reserve(mHostName.size() + 1 + 19 + 1 + 7);
    execid.assign(mHostName).append(":").append(std::to_string(ktime)).append(":").append(std::to_string(pid));
    return Base64Enconde(execid);
}

bool ProcessCacheManager::FinalizeProcessTags(uint32_t pid, uint64_t ktime, LogEvent& logEvent) {
    static const std::string kUnkownStr = "unknown";

    auto procPtr = mProcessCache.Lookup({pid, ktime});
    if (!procPtr) {
        ADD_COUNTER(mProcessCacheMissTotal, 1);
        LOG_WARNING(sLogger, ("cannot find proc in cache, pid", pid)("ktime", ktime)("size", mProcessCache.Size()));
        return false;
    }

    // event_type, added by xxx_security_manager
    // call_name, added by xxx_security_manager
    // event_time, added by xxx_security_manager

    // finalize proc tags
    auto& proc = *procPtr;
    auto& sb = logEvent.GetSourceBuffer();
    auto execIdSb = sb->CopyString(proc[kExecId]);
    logEvent.SetContentNoCopy(kExecId.LogKey(), StringView(execIdSb.data, execIdSb.size));

    auto pidSb = sb->CopyString(proc[kProcessId]);
    logEvent.SetContentNoCopy(kProcessId.LogKey(), StringView(pidSb.data, pidSb.size));

    auto uidSb = sb->CopyString(proc[kUid]);
    logEvent.SetContentNoCopy(kUid.LogKey(), StringView(uidSb.data, uidSb.size));

    auto userSb = sb->CopyString(proc[kUser]);
    logEvent.SetContentNoCopy(kUser.LogKey(), StringView(userSb.data, userSb.size));

    auto binarySb = sb->CopyString(proc[kBinary]);
    logEvent.SetContentNoCopy(kBinary.LogKey(), StringView(binarySb.data, binarySb.size));

    auto argsSb = sb->CopyString(proc[kArguments]);
    logEvent.SetContentNoCopy(kArguments.LogKey(), StringView(argsSb.data, argsSb.size));

    auto cwdSb = sb->CopyString(proc[kCWD]);
    logEvent.SetContentNoCopy(kCWD.LogKey(), StringView(cwdSb.data, cwdSb.size));

    auto ktimeSb = sb->CopyString(proc[kKtime]);
    logEvent.SetContentNoCopy(kKtime.LogKey(), StringView(ktimeSb.data, ktimeSb.size));

    auto permitted = sb->CopyString(proc[kCapPermitted]);
    logEvent.SetContentNoCopy(kCapPermitted.LogKey(), StringView(permitted.data, permitted.size));

    auto effective = sb->CopyString(proc[kCapEffective]);
    logEvent.SetContentNoCopy(kCapEffective.LogKey(), StringView(effective.data, effective.size));

    auto inheritable = sb->CopyString(proc[kCapInheritable]);
    logEvent.SetContentNoCopy(kCapInheritable.LogKey(), StringView(inheritable.data, inheritable.size));

    auto parentProcPtr = mProcessCache.Lookup({proc.mPPid, proc.mPKtime});
    // for parent
    if (!parentProcPtr) {
        return true;
    }
    // finalize parent tags
    auto& parentProc = *parentProcPtr;
    auto parentExecIdSb = sb->CopyString(parentProc[kExecId]);
    logEvent.SetContentNoCopy(kParentExecId.LogKey(), StringView(parentExecIdSb.data, parentExecIdSb.size));

    auto parentPidSb = sb->CopyString(parentProc[kProcessId]);
    logEvent.SetContentNoCopy(kParentProcessId.LogKey(), StringView(parentPidSb.data, parentPidSb.size));

    auto parentUidSb = sb->CopyString(parentProc[kUid]);
    logEvent.SetContentNoCopy(kParentUid.LogKey(), StringView(parentUidSb.data, parentUidSb.size));

    auto parentUserSb = sb->CopyString(parentProc[kUser]);
    logEvent.SetContentNoCopy(kParentUser.LogKey(), StringView(parentUserSb.data, parentUserSb.size));

    auto parentBinarySb = sb->CopyString(parentProc[kBinary]);
    logEvent.SetContentNoCopy(kParentBinary.LogKey(), StringView(parentBinarySb.data, parentBinarySb.size));

    auto parentArgsSb = sb->CopyString(parentProc[kArguments]);
    logEvent.SetContentNoCopy(kParentArguments.LogKey(), StringView(parentArgsSb.data, parentArgsSb.size));

    auto parentCwdSb = sb->CopyString(parentProc[kCWD]);
    logEvent.SetContentNoCopy(kParentCWD.LogKey(), StringView(parentCwdSb.data, parentCwdSb.size));

    auto parentKtimeSb = sb->CopyString(parentProc[kKtime]);
    logEvent.SetContentNoCopy(kParentKtime.LogKey(), StringView(parentKtimeSb.data, parentKtimeSb.size));

    auto parentPermitted = sb->CopyString(parentProc[kCapPermitted]);
    logEvent.SetContentNoCopy(kParentCapPermitted.LogKey(), StringView(parentPermitted.data, parentPermitted.size));

    auto parentEffective = sb->CopyString(parentProc[kCapEffective]);
    logEvent.SetContentNoCopy(kParentCapEffective.LogKey(), StringView(parentEffective.data, parentEffective.size));

    auto parentInheritable = sb->CopyString(parentProc[kCapInheritable]);
    logEvent.SetContentNoCopy(kParentCapInheritable.LogKey(),
                              StringView(parentInheritable.data, parentInheritable.size));

    return true;
}

} // namespace ebpf
} // namespace logtail
