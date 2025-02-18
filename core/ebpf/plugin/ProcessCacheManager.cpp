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

#include <coolbpf/security/bpf_common.h>
#include <coolbpf/security/bpf_process_event_type.h>
#include <coolbpf/security/data_msg.h>
#include <coolbpf/security/msg_type.h>

#include <algorithm>
#include <atomic>
#include <regex>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "common/CapabilityUtil.h"
#include "common/EncodingUtil.h"
#include "common/LRUCache.h"
#include "common/ProcParser.h"
#include "ebpf/type/ProcessEvent.h"
#include "ebpf/type/table/ProcessTable.h"
#include "logger/Logger.h"
#include "util/FrequencyManager.h"

namespace logtail {
namespace ebpf {

const std::string UNKOWN_STR = "unknown";

/////////// ================= for perfbuffer handlers ================= ///////////
void HandleKernelProcessEvent(void* ctx, int cpu, void* data, uint32_t data_sz) {
    auto* bm = static_cast<ProcessCacheManager*>(ctx);
    if (!bm) {
        LOG_ERROR(sLogger, ("BaseManager is null!", ""));
        return;
    }
    if (!data) {
        LOG_ERROR(sLogger, ("data is null!", ""));
        return;
    }

    auto* common = static_cast<struct msg_common*>(data);
    switch (common->op) {
        case MSG_OP_CLONE: {
            auto* event = static_cast<struct msg_clone_event*>(data);
            // TODO enqueue
            bm->RecordCloneEvent(event);
            break;
        }
        case MSG_OP_EXIT: {
            auto* event = static_cast<struct msg_exit*>(data);
            // TODO set into delete queue ...
            bm->RecordExitEvent(event);
            break;
        }
        case MSG_OP_EXECVE: {
            auto* event_ptr = static_cast<struct msg_execve_event*>(data);
            bm->RecordExecveEvent(event_ptr);
            break;
        }
        case MSG_OP_DATA: {
            auto* eventPtr = static_cast<msg_data*>(data);
            bm->RecordDataEvent(eventPtr);
            // TODO
            break;
        }
        default: {
            LOG_WARNING(sLogger, ("Unknown event op", static_cast<int>(common->op)));
            break;
        }
    }
}

void HandleKernelProcessEventLost(void* ctx, int cpu, unsigned long long lost_cnt) {
    LOG_WARNING(sLogger, ("lost events", lost_cnt)("cpu", cpu));
    // TODO self monitor...
}
////////////////////////////////////////////////////////////////////////////////////////

bool ProcessCacheManager::Init() {
    if (mInited) {
        return true;
    }
    mInited = true;
    mFrequencyMgr.SetPeriod(std::chrono::milliseconds(100));
    auto ebpfConfig = std::make_unique<PluginConfig>();
    ebpfConfig->mPluginType = PluginType::PROCESS_SECURITY;
    ProcessConfig pconfig;

    pconfig.mPerfBufferSpec
        = {{"tcpmon_map",
            128,
            this,
            [](void* ctx, int cpu, void* data, uint32_t size) { HandleKernelProcessEvent(ctx, cpu, data, size); },
            [](void* ctx, int cpu, unsigned long long cnt) { HandleKernelProcessEventLost(ctx, cpu, cnt); }}};
    ebpfConfig->mConfig = pconfig;
    mFlag = true;
    mPoller = async(std::launch::async, &ProcessCacheManager::PollPerfBuffers, this);
    mCacheUpdater = async(std::launch::async, &ProcessCacheManager::HandleCacheUpdate, this);
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

void ProcessCacheManager::PollPerfBuffers() {
    int zero = 0;
    LOG_DEBUG(sLogger, ("enter poller thread", ""));
    while (mFlag) {
        auto now = std::chrono::steady_clock::now();
        auto nextWindow = mFrequencyMgr.Next();
        if (!mFrequencyMgr.Expired(now)) {
            std::this_thread::sleep_until(nextWindow);
            mFrequencyMgr.Reset(nextWindow);
        } else {
            mFrequencyMgr.Reset(now);
        }
        auto ret = mSourceManager->PollPerfBuffers(PluginType::PROCESS_SECURITY, 4096, &zero, 200);
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
    mFlag = false;
    std::future_status s1 = mPoller.wait_for(std::chrono::seconds(1));
    if (mPoller.valid()) {
        if (s1 == std::future_status::ready) {
            LOG_INFO(sLogger, ("poller thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("poller thread", "forced to stopped"));
        }
    }

    std::future_status s2 = mCacheUpdater.wait_for(std::chrono::seconds(1));
    if (mCacheUpdater.valid()) {
        if (s2 == std::future_status::ready) {
            LOG_INFO(sLogger, ("cachee updater thread", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("cachee updater thread", "forced to stopped"));
        }
    }
    mInited = false;
}

void ProcessCacheManager::DataAdd(msg_data* dataPtr) {
    auto size = dataPtr->common.size - offsetof(msg_data, arg);
    if (size <= MSG_DATA_ARG_LEN) {
        // std::vector<uint64_t> key = {dataPtr->id.pid, dataPtr->id.time};
        auto res = mDataCache.find(dataPtr->id);
        if (res != mDataCache.end()) {
            std::string prevData = mDataCache[dataPtr->id];
            LOG_DEBUG(sLogger,
                      ("already have data, pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("prevData", res->second)(
                          "data", std::string(dataPtr->arg, size)));
            res->second.append(dataPtr->arg, size);
        } else {
            LOG_DEBUG(sLogger,
                      ("no prev data, pid",
                       dataPtr->id.pid)("ktime", dataPtr->id.time)("data", std::string(dataPtr->arg, size)));
            mDataCache[dataPtr->id] = std::string(dataPtr->arg, size);
        }
    } else {
        LOG_ERROR(sLogger, ("pid", dataPtr->id.pid)("ktime", dataPtr->id.time)("size limit exceeded", size));
    }
}
std::string ProcessCacheManager::DataGet(data_event_desc* desc) {
    std::vector<uint64_t> key = {desc->id.pid, desc->id.time};
    auto res = mDataCache.find(desc->id);
    static std::string sEmpty;
    if (res == mDataCache.end()) {
        return sEmpty;
    }
    std::string data;
    data.swap(res->second);
    mDataCache.erase(res);

    if (data.size() != desc->size - desc->leftover) {
        LOG_WARNING(sLogger, ("size bad! data size", data.size())("expect", desc->size - desc->leftover));
        return sEmpty;
    }

    return data;
}

void ProcessCacheManager::RecordDataEvent(msg_data* eventPtr) {
    LOG_DEBUG(sLogger,
              ("[receive_data_event] size", eventPtr->common.size)("pid", eventPtr->id.pid)("time", eventPtr->id.time)(
                  "data", std::string(eventPtr->arg, eventPtr->common.size - offsetof(msg_data, arg))));
    DataAdd(eventPtr);
}

std::tuple<std::string, std::string> ArgsDecoder(const std::string& args, uint32_t flags) {
    LOG_DEBUG(sLogger, ("args", args)("flags", flags));
    int hasCWD = 0;
    std::string cwd;
    if (((flags & EVENT_NO_CWD_SUPPORT) | (flags & EVENT_ERROR_CWD) | (flags & EVENT_ROOT_CWD)) == 0) {
        hasCWD = 1;
    }

    std::vector<std::string> argTokens;
    std::string item;

    // split with \0
    for (auto x : args) {
        if (x == '\0') {
            if (item.size()) {
                argTokens.push_back(item);
                item.clear();
            }
        } else {
            item += x;
        }
    }
    if (item.size()) {
        argTokens.push_back(item);
    }

    LOG_DEBUG(sLogger, ("args", args)("flags", flags)("length", argTokens.size())("hasCWD", hasCWD));

    if (flags & EVENT_NO_CWD_SUPPORT) {
        LOG_DEBUG(sLogger, ("args", args)("flags", flags)("length", argTokens.size())("hasCWD", hasCWD));
        hasCWD = 1;
    } else if (flags & EVENT_ERROR_CWD) {
        LOG_DEBUG(sLogger, ("args", args)("flags", flags)("length", argTokens.size())("hasCWD", hasCWD));
        cwd = "ERROR";
        hasCWD = 1;
    } else if (flags & EVENT_ROOT_CWD) {
        LOG_DEBUG(sLogger, ("args", args)("flags", flags)("length", argTokens.size())("hasCWD", hasCWD));
        cwd = "/";
        hasCWD = 1;
    } else {
        LOG_DEBUG(sLogger, ("args", args)("flags", flags)("length", argTokens.size())("hasCWD", hasCWD));
        if (argTokens.size()) {
            cwd = argTokens[argTokens.size() - 1];
        }
    }

    std::string arguments;

    if (argTokens.empty()) {
        LOG_DEBUG(sLogger, ("arg", args)("cwd", cwd)("arguments", arguments)("flag", flags));
        return std::make_tuple(std::move(arguments), std::move(cwd));
    }

    for (size_t i = 0; i < argTokens.size() - hasCWD; i++) {
        if (argTokens[i].find(' ') != std::string::npos) {
            arguments += ("\"" + argTokens[i] + "\"");
        } else {
            if (arguments.empty()) {
                arguments = argTokens[i];
            } else {
                arguments = arguments + " " + argTokens[i];
            }
        }
    }

    LOG_DEBUG(sLogger, ("arg", args)("cwd", cwd)("arguments", arguments)("flag", flags));

    return std::make_tuple(std::move(arguments), std::move(cwd));
}

void ProcessCacheManager::RecordExecveEvent(msg_execve_event* eventPtr) {
    // copy msg
    auto event = std::make_unique<MsgExecveEventUnix>();
    event->msg = std::make_unique<MsgExecveEvent>();
    static_assert(offsetof(msg_execve_event, buffer) == sizeof(MsgExecveEvent),
                  "offsetof(msg_execve_event, buffer) must be equal to sizeof(MsgExecveEvent)");
    std::memcpy(event->msg.get(), eventPtr, sizeof(MsgExecveEvent));

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
    // constexpr auto argOffset = offsetof(msg_process, args); // 56
    auto size = eventPtr->process.size - SIZEOF_EVENT; // remain size
    if (size > PADDED_BUFFER - SIZEOF_EVENT) {
        event->process.args = "enomem enomem";
        event->process.filename = "enomem";
        eventPtr->process.size = SIZEOF_EVENT;
        PostHandlerExecveEvent(eventPtr, std::move(event));
        return;
    }

    char* argStart = eventPtr->buffer + SIZEOF_EVENT;
    // auto args = std::string(eventPtr->buffer + SIZEOF_EVENT, size);
    if (eventPtr->process.flags & EVENT_DATA_FILENAME) {
        if (size < sizeof(data_event_desc)) {
            event->process.args = "enomem enomem";
            event->process.filename = "enomem";
            eventPtr->process.size = SIZEOF_EVENT;
            PostHandlerExecveEvent(eventPtr, std::move(event));
            return;
        }
        auto* desc = reinterpret_cast<data_event_desc*>(argStart);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_FILENAME, size",
                   desc->size)("leftover", desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        auto data = DataGet(desc);
        if (data.empty()) {
            PostHandlerExecveEvent(eventPtr, std::move(event));
            return;
        }
        event->process.filename = data;
        argStart += sizeof(data_event_desc);
        size -= sizeof(data_event_desc);
    } else if ((eventPtr->process.flags & EVENT_ERROR_FILENAME) == 0) {
        // args 中找第一个 \0 的索引 idx
        for (uint32_t i = 0; i < size; i++) {
            if (argStart[i] == '\0') {
                event->process.filename = std::string(argStart, i);
                argStart += (i + 1);
                size -= i;
                break;
            }
        }
    }

    // cmd args
    if (eventPtr->process.flags & EVENT_DATA_ARGS) {
        auto* desc = reinterpret_cast<data_event_desc*>(argStart);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_FILENAME, size",
                   desc->size)("leftover", desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        auto data = DataGet(desc);
        if (data.empty()) {
            PostHandlerExecveEvent(eventPtr, std::move(event));
            return;
        }
        // cwd
        event->process.cwd = std::string(argStart + sizeof(data_event_desc), size - sizeof(data_event_desc));
        event->process.args.reserve(data.size() + 1 + event->process.cwd.size());
        event->process.args.assign(data).append("\0").append(event->process.cwd);
    } else {
        event->process.args = std::string(argStart, size);
    }

    PostHandlerExecveEvent(eventPtr, std::move(event));
}

void ProcessCacheManager::PostHandlerExecveEvent(msg_execve_event* eventPtr,
                                                 std::unique_ptr<MsgExecveEventUnix>&& event) {
#ifdef APSARA_UNIT_TEST_MAIN
    LOG_DEBUG(
        sLogger,
        ("before ArgsDecoder", event->process.pid)("ktime", event->process.ktime)("cmdline", event->process.cmdline)(
            "filename", event->process.filename)("dockerid", event->kube.docker)("raw_args", event->process.args)(
            "cwd", event->process.cwd)("flag", eventPtr->process.flags)(
            "procParser.exePath", event->process.testFileName)("procParser.cmdLine", event->process.testCmdline));
#endif

    auto [args, cwd] = ArgsDecoder(event->process.args, event->process.flags);
    event->process.args = args;
    event->process.cwd = cwd;
    // set binary
    if (event->process.filename.size()) {
        if (event->process.filename[0] == '/') {
            event->process.binary = event->process.filename;
        } else {
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
            "cwd", event->process.cwd)("flag", eventPtr->process.flags));

    mRecordQueue.enqueue(std::move(event));

    if (mFlushProcessEvent) {
        auto event = std::make_shared<ProcessEvent>(eventPtr->process.pid,
                                                    eventPtr->process.ktime,
                                                    KernelEventType::PROCESS_EXECVE_EVENT,
                                                    eventPtr->common.ktime);
        if (event) {
            mCommonEventQueue.enqueue(std::move(event));
        }
    }
}

void ProcessCacheManager::RecordExitEvent(msg_exit* eventPtr) {
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
        auto tgid = eventPtr->tgid;
        auto ktime = eventPtr->ktime;
        auto commonKtime = eventPtr->common.ktime;
        auto event = std::make_shared<ProcessEvent>(tgid, ktime, KernelEventType::PROCESS_CLONE_EVENT, commonKtime);
        if (event) {
            mCommonEventQueue.enqueue(std::move(event));
        }
    }
}

std::vector<std::shared_ptr<Procs>> ProcessCacheManager::ListRunningProcs() {
    std::vector<std::shared_ptr<Procs>> processes;
    for (const auto& entry : std::filesystem::directory_iterator(mHostPathPrefix + "/proc")) {
        if (!entry.is_directory()) {
            continue;
        }
        auto dirName = entry.path().filename().string();
        if (!std::regex_match(dirName, mPidRegex)) {
            continue;
        }
        int32_t pid = std::stoi(dirName);
        auto cmdLine = mProcParser.GetPIDCmdline(pid);
        auto comm = mProcParser.GetPIDComm(pid);
        bool kernelThread = false;
        if (cmdLine == "") {
            cmdLine = comm;
            kernelThread = true;
        }

        std::vector<std::string> stats;
        try {
            stats = mProcParser.GetProcStatStrings(pid);
        } catch (std::runtime_error& e) {
            LOG_WARNING(sLogger, ("GetProcStatStrings failed", e.what()));
            continue;
        }

        auto ppid = stats[3];

        // get ppid
        int32_t _ppid = std::stoi(ppid);
        uint64_t ktime = mProcParser.GetStatsKtime(stats);

        std::vector<uint32_t> uids
            = {mProcParser.invalid_uid_, mProcParser.invalid_uid_, mProcParser.invalid_uid_, mProcParser.invalid_uid_};
        std::vector<uint32_t> gids
            = {mProcParser.invalid_uid_, mProcParser.invalid_uid_, mProcParser.invalid_uid_, mProcParser.invalid_uid_};
        uint32_t auid = mProcParser.invalid_uid_;

        try {
            auto status = mProcParser.GetStatus(pid);
            if (status) {
                uids = status->GetUids();
                gids = status->GetGids();
                auid = status->GetLoginUid();
            }
        } catch (std::runtime_error& e) {
            LOG_WARNING(sLogger, ("GetStatus failed", e.what()));
            continue;
        }

        auto [nspid, permitted, effective, inheritable] = mProcParser.GetPIDCaps(pid);
        auto uts_ns = mProcParser.GetPIDNsInode(pid, "uts");
        auto ipc_ns = mProcParser.GetPIDNsInode(pid, "ipc");
        auto mnt_ns = mProcParser.GetPIDNsInode(pid, "mnt");
        auto pid_ns = mProcParser.GetPIDNsInode(pid, "pid");
        auto pid_for_children_ns = mProcParser.GetPIDNsInode(pid, "pid_for_children");
        auto net_ns = mProcParser.GetPIDNsInode(pid, "net");
        auto cgroup_ns = mProcParser.GetPIDNsInode(pid, "cgroup");
        auto user_ns = mProcParser.GetPIDNsInode(pid, "user");
        uint32_t time_ns = 0;
        uint32_t time_for_children_ns = 0;
        try {
            time_ns = mProcParser.GetPIDNsInode(pid, "time");
            time_for_children_ns = mProcParser.GetPIDNsInode(pid, "time_for_children");
        } catch (std::runtime_error& e) {
            LOG_WARNING(sLogger, ("GetPIDNsInode failed", e.what()));
            continue;
        }

        std::string docker_id = mProcParser.GetPIDDockerId(pid);
        if (docker_id == "") {
            nspid = 0;
        }

        std::string parent_cmdline;
        std::string parent_comm;
        std::vector<std::string> parent_stats;
        uint64_t parent_ktime = 0;
        std::string parent_exe_path;
        uint32_t parent_nspid = 0;
        if (_ppid) {
            parent_cmdline = mProcParser.GetPIDCmdline(_ppid);
            parent_comm = mProcParser.GetPIDComm(_ppid);
            parent_stats = mProcParser.GetProcStatStrings(_ppid);
            parent_ktime = mProcParser.GetStatsKtime(parent_stats);
            parent_exe_path = mProcParser.GetPIDExePath(_ppid);
            auto [pnspid, ppermitted, peffective, pinheritable] = mProcParser.GetPIDCaps(_ppid);
            parent_nspid = pnspid;
        }

        std::string exec_path = mProcParser.GetPIDExePath(pid);

        std::shared_ptr<Procs> procs_ptr = std::make_shared<Procs>();
        procs_ptr->ppid = static_cast<uint32_t>(_ppid);
        procs_ptr->pnspid = parent_nspid;
        procs_ptr->pexe = parent_exe_path;
        procs_ptr->pcmdline = parent_cmdline;
        procs_ptr->pflags
            = static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD | ApiEventFlag::NeedsAUID);
        procs_ptr->pktime = parent_ktime;
        procs_ptr->uids = uids;
        procs_ptr->gids = gids;
        procs_ptr->auid = auid;
        procs_ptr->pid = static_cast<uint32_t>(pid);
        procs_ptr->tid = static_cast<uint32_t>(pid);
        procs_ptr->nspid = nspid;
        procs_ptr->exe = exec_path;
        procs_ptr->cmdline = cmdLine;
        procs_ptr->flags
            = static_cast<uint32_t>(ApiEventFlag::ProcFS | ApiEventFlag::NeedsCWD | ApiEventFlag::NeedsAUID);
        procs_ptr->ktime = ktime;
        procs_ptr->permitted = permitted;
        procs_ptr->effective = effective;
        procs_ptr->inheritable = inheritable;
        procs_ptr->uts_ns = uts_ns;
        procs_ptr->ipc_ns = ipc_ns;
        procs_ptr->mnt_ns = mnt_ns;
        procs_ptr->pid_ns = pid_ns;
        procs_ptr->pid_for_children_ns = pid_for_children_ns;
        procs_ptr->net_ns = net_ns;
        procs_ptr->time_ns = time_ns;
        procs_ptr->time_for_children_ns = time_for_children_ns;
        procs_ptr->cgroup_ns = cgroup_ns;
        procs_ptr->user_ns = user_ns;
        procs_ptr->kernel_thread = kernelThread;

        processes.emplace_back(procs_ptr);
    }
    LOG_DEBUG(sLogger, ("Read ProcFS prefix", mHostPathPrefix)("append process cnt", processes.size()));

    return processes;
}

int ProcessCacheManager::WriteProcToBPFMap(const std::shared_ptr<Procs>& proc) {
    msg_execve_key key;
    key.pid = proc->pid;
    key.ktime = 0;

    execve_map_value value;
    value.pkey.pid = proc->ppid;
    value.pkey.ktime = proc->pktime;
    value.key.pid = proc->pid;
    value.key.ktime = proc->ktime;
    value.flags = 0;
    value.nspid = proc->nspid;
    value.caps = {proc->permitted, proc->effective, proc->inheritable};
    value.ns = {proc->uts_ns,
                proc->ipc_ns,
                proc->mnt_ns,
                proc->pid,
                proc->pid_for_children_ns,
                proc->net_ns,
                proc->time_ns,
                proc->time_for_children_ns,
                proc->cgroup_ns,
                proc->user_ns};
    value.bin.path_length = proc->exe.size();
    ::memcpy(value.bin.path, proc->exe.data(), std::min(BINARY_PATH_MAX_LEN, static_cast<int>(proc->exe.size())));

    // update bpf map
    int res = mSourceManager->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &key, &value, 0);
    LOG_DEBUG(sLogger, ("update bpf map, pid", proc->pid)("res", res));
    return res;
}


int ProcessCacheManager::SyncAllProc() {
    std::vector<std::shared_ptr<Procs>> procs = ListRunningProcs();
    // update execve map
    for (auto& proc : procs) {
        WriteProcToBPFMap(proc);
    }
    // add kernel thread (pid 0)
    msg_execve_key key;
    key.pid = 0;
    key.ktime = 0;
    execve_map_value value;
    value.pkey.pid = 0;
    value.pkey.ktime = 1;
    value.key.pid = 0;
    value.key.ktime = 1;
    mSourceManager->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &key, &value, 0);

    // generage execve event ...
    for (const auto& proc : procs) {
        PushExecveEvent(proc);
    }

    return 0;
}

std::string PrependPath(const std::string& exe, const std::string& cmdline) {
    std::string res;
    auto idx = cmdline.find('\0');
    if (idx == std::string::npos) {
        res = exe;
    } else {
        res = exe + cmdline.substr(idx);
    }
    return res;
}


const uint32_t MSG_UNIX_SIZE = 640;
int ProcessCacheManager::PushExecveEvent(const std::shared_ptr<Procs>& proc) {
    if (proc == nullptr) {
        return 1;
    }
    std::string rawArgs = PrependPath(proc->exe, proc->cmdline);
    std::string rawPargs = PrependPath(proc->pexe, proc->pcmdline);

    auto [args, filename] = mProcParser.ProcsFilename(rawArgs);
    LOG_DEBUG(sLogger, ("raw_args", rawArgs)("args", args)("filename", filename));
    std::string cwd;
    uint32_t flags = static_cast<uint32_t>(ApiEventFlag::RootCWD);

    std::pair<std::string, uint32_t> cwdRes = mProcParser.GetPIDCWD(proc->pid);
    cwd = cwdRes.first;
    flags = cwdRes.second;

    auto event = std::make_unique<MsgExecveEventUnix>();
    if (event == nullptr) {
        LOG_ERROR(sLogger, ("failed to alloc MsgExecveEventUnix", ""));
        return 1;
    }

    if (proc->kernel_thread) {
        if (proc == nullptr) {
            LOG_ERROR(sLogger, ("kernel thread, proc is null", ""));
            return 1;
        }

        event->kernel_thread = true;
        event->msg = std::make_unique<MsgExecveEvent>();
        event->msg->parent.pid = proc->ppid;
        event->msg->parent.ktime = proc->ktime;

        event->process.size = proc->size;
        event->process.pid = proc->pid;
        event->process.tid = proc->tid;
        event->process.nspid = proc->nspid;
        event->process.uid = 0;
        event->process.auid = mProcParser.invalid_uid_;
        event->process.flags = static_cast<uint32_t>(ApiEventFlag::ProcFS);
        event->process.ktime = proc->ktime;
        event->process.args = "";
        event->process.cmdline = proc->cmdline;
        event->process.cwd = cwd;
        //    event->process.filename = filename;
    } else {
        //    std::unique_ptr<MsgExecveEventUnix> event = std::make_unique<MsgExecveEventUnix>();
        event->kernel_thread = false;
        event->msg = std::make_unique<MsgExecveEvent>();
        event->msg->common.op = MSG_OP_EXECVE;
        if (proc == nullptr) {
            LOG_ERROR(sLogger, ("user thread, proc is null", ""));
            return 1;
        }
        event->msg->common.size = MSG_UNIX_SIZE + proc->psize + proc->size;

        if (proc->pid) {
            std::string dockerId = mProcParser.GetPIDDockerId(proc->pid);
            if (dockerId != "") {
                event->kube.docker = dockerId;
            }
        }
        event->msg->parent.pid = proc->ppid;
        event->msg->parent.ktime = proc->pktime;
        event->msg->ns.uts_inum = proc->uts_ns;
        event->msg->ns.ipc_inum = proc->ipc_ns;
        event->msg->ns.mnt_inum = proc->mnt_ns;
        event->msg->ns.pid_inum = proc->pid_ns;
        event->msg->ns.pid_for_children_inum = proc->pid_for_children_ns;
        event->msg->ns.net_inum = proc->net_ns;
        event->msg->ns.time_inum = proc->time_ns;
        event->msg->ns.time_for_children_inum = proc->time_for_children_ns;
        event->msg->ns.cgroup_inum = proc->cgroup_ns;
        event->msg->ns.user_inum = proc->user_ns;
        event->process.size = proc->size;
        event->process.pid = proc->pid;
        event->process.tid = proc->tid;
        event->process.nspid = proc->nspid;
        event->process.uid = proc->uids[1];
        event->process.auid = proc->auid;
        event->msg->creds.uid = proc->uids[0];
        event->msg->creds.gid = proc->uids[1];
        event->msg->creds.suid = proc->uids[2];
        event->msg->creds.sgid = proc->uids[3];
        event->msg->creds.euid = proc->gids[0];
        event->msg->creds.egid = proc->gids[1];
        event->msg->creds.fsuid = proc->gids[2];
        event->msg->creds.fsgid = proc->gids[3];
        event->msg->creds.caps.permitted = proc->permitted;
        event->msg->creds.caps.effective = proc->effective;
        event->msg->creds.caps.inheritable = proc->inheritable;
        event->process.flags = proc->flags | flags;
        event->process.ktime = proc->ktime;
        event->msg->common.ktime = proc->ktime;
        event->process.filename = proc->exe;
        //    event->process.args = raw_args;
        event->process.args = args;
        event->process.cmdline = proc->cmdline;
        event->process.cwd = cwd;
    }

    mRecordQueue.enqueue(std::move(event));
    return 0;
}

std::string ProcessCacheManager::GenerateParentExecId(const std::shared_ptr<MsgExecveEventUnix>& event) {
    if (!event->msg) {
        return "";
    }
    if (event->msg->cleanup_key.ktime == 0 || event->process.flags & EVENT_CLONE) {
        return GenerateExecId(event->msg->parent.pid, event->msg->parent.ktime);
    }
    return GenerateExecId(event->msg->cleanup_key.pid, event->msg->cleanup_key.ktime);
}

std::string ProcessCacheManager::GenerateExecId(uint32_t pid, uint64_t ktime) {
    // /proc/sys/kernel/pid_max is usually 7 digits 4194304
    // nano timestamp is usually 19 digits
    std::string execid;
    execid.reserve(mHostName.size() + 1 + 7 + 1 + 19);
    execid.assign(mHostName).append(":").append(std::to_string(pid)).append(":").append(std::to_string(ktime));
    return Base64Enconde(execid);
}

// consume record queue
void ProcessCacheManager::HandleCacheUpdate() {
    std::vector<std::unique_ptr<MsgExecveEventUnix>> items(mMaxBatchConsumeSize);

    while (mFlag) {
        size_t count = mRecordQueue.wait_dequeue_bulk_timed(
            items.data(), mMaxBatchConsumeSize, std::chrono::milliseconds(mMaxWaitTimeMS));

        if (!count) {
            continue;
        }

        for (size_t i = 0; i < count; ++i) {
            // set args
            std::shared_ptr<MsgExecveEventUnix> event(std::move(items[i]));

            event->process.user.name = mProcParser.GetUserNameByUid(event->process.uid);
            std::string execKey = GenerateExecId(event->process.pid, event->process.ktime);

            std::string parentKey = GenerateParentExecId(event);
            event->exec_id = execKey;
            event->parent_exec_id = parentKey;
            LOG_DEBUG(
                sLogger,
                ("[RecordExecveEvent][DUMP] begin update cache pid", event->process.pid)("ktime", event->process.ktime)(
                    "execId", execKey)("cmdline", mProcParser.GetPIDCmdline(event->process.pid))(
                    "filename", mProcParser.GetPIDExePath(event->process.pid))("args", event->process.args));

            UpdateCache(execKey, event);
        }

        items.clear();
        items.resize(mMaxBatchConsumeSize);
    }
}

SizedMap ProcessCacheManager::FinalizeProcessTags(std::shared_ptr<SourceBuffer>& sb, uint32_t pid, uint64_t ktime) {
    SizedMap res;
    auto execId = GenerateExecId(pid, ktime);
    auto proc = LookupCache(execId);
    if (!proc) {
        LOG_WARNING(sLogger,
                    ("cannot find proc in cache, execId",
                     execId)("pid", pid)("ktime", ktime)("contains", proc.get() != nullptr)("size", mCache.size()));
        return res;
    }

    auto parentExecId = GenerateParentExecId(proc);
    auto parentProc = LookupCache(parentExecId);

    // finalize proc tags
    auto execIdSb = sb->CopyString(proc->exec_id);
    res.Insert(kExecId.log_key(), StringView(execIdSb.data, execIdSb.size));

    auto pExecIdSb = sb->CopyString(proc->parent_exec_id);
    res.Insert(kParentExecId.log_key(), StringView(pExecIdSb.data, pExecIdSb.size));

    // finalize parent tags

    std::string args = proc->process.args; // TODO
    std::string binary = proc->process.filename; // TODO
    std::string permitted = GetCapabilities(proc->msg->creds.caps.permitted);
    std::string effective = GetCapabilities(proc->msg->creds.caps.effective);
    std::string inheritable = GetCapabilities(proc->msg->creds.caps.inheritable);

    rapidjson::Document::AllocatorType allocator;
    rapidjson::Value cap(rapidjson::kObjectType);

    cap.AddMember("permitted", rapidjson::Value().SetString(permitted.c_str(), allocator), allocator);
    cap.AddMember("effective", rapidjson::Value().SetString(effective.c_str(), allocator), allocator);
    cap.AddMember("inheritable", rapidjson::Value().SetString(inheritable.c_str(), allocator), allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    cap.Accept(writer);

    std::string capStr = buffer.GetString();

    // event_type, added by xxx_security_manager
    // call_name, added by xxx_security_manager
    // event_time, added by xxx_security_manager
    auto pidSb = sb->CopyString(std::to_string(proc->process.pid));
    res.Insert(kPid.log_key(), StringView(pidSb.data, pidSb.size));

    auto uidSb = sb->CopyString(std::to_string(proc->process.uid));
    res.Insert(kUid.log_key(), StringView(uidSb.data, uidSb.size));

    auto userSb = sb->CopyString(proc->process.user.name);
    res.Insert(kUser.log_key(), StringView(userSb.data, userSb.size));

    auto binarySb = sb->CopyString(binary);
    res.Insert(kBinary.log_key(), StringView(binarySb.data, binarySb.size));

    auto argsSb = sb->CopyString(args);
    res.Insert(kArguments.log_key(), StringView(argsSb.data, argsSb.size));

    auto cwdSb = sb->CopyString(proc->process.cwd);
    res.Insert(kCWD.log_key(), StringView(cwdSb.data, cwdSb.size));

    auto ktimeSb = sb->CopyString(std::to_string(proc->process.ktime));
    res.Insert(kKtime.log_key(), StringView(ktimeSb.data, ktimeSb.size));

    auto capSb = sb->CopyString(capStr);
    res.Insert(kCap.log_key(), StringView(capSb.data, capSb.size));

    // for parent
    if (!parentProc) {
        res.Insert(kParentProcess.log_key(), StringView(UNKOWN_STR));
        return res;
    }
    { // finalize parent tags
        std::string permitted = GetCapabilities(parentProc->msg->creds.caps.permitted);
        std::string effective = GetCapabilities(parentProc->msg->creds.caps.effective);
        std::string inheritable = GetCapabilities(parentProc->msg->creds.caps.inheritable);

        rapidjson::Document d;
        d.SetObject();

        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

        d.AddMember("exec_id", rapidjson::Value().SetString(parentProc->exec_id.c_str(), allocator), allocator);
        d.AddMember(
            "parent_exec_id", rapidjson::Value().SetString(parentProc->parent_exec_id.c_str(), allocator), allocator);
        d.AddMember(
            "pid", rapidjson::Value().SetString(std::to_string(parentProc->process.pid).c_str(), allocator), allocator);
        d.AddMember(
            "uid", rapidjson::Value().SetString(std::to_string(parentProc->process.uid).c_str(), allocator), allocator);
        d.AddMember("user", rapidjson::Value().SetString(parentProc->process.user.name.c_str(), allocator), allocator);
        d.AddMember("binary", rapidjson::Value().SetString(binary.c_str(), allocator), allocator);
        d.AddMember("arguments", rapidjson::Value().SetString(args.c_str(), allocator), allocator);
        d.AddMember("cwd", rapidjson::Value().SetString(parentProc->process.cwd.c_str(), allocator), allocator);
        d.AddMember("ktime",
                    rapidjson::Value().SetString(std::to_string(parentProc->process.ktime).c_str(), allocator),
                    allocator);

        rapidjson::Value cap(rapidjson::kObjectType);

        cap.AddMember("permitted", rapidjson::Value().SetString(permitted.c_str(), allocator), allocator);
        cap.AddMember("effective", rapidjson::Value().SetString(effective.c_str(), allocator), allocator);
        cap.AddMember("inheritable", rapidjson::Value().SetString(inheritable.c_str(), allocator), allocator);

        d.AddMember("cap", cap, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        d.Accept(writer);

        std::string result = buffer.GetString();

        auto parentSb = sb->CopyString(result);
        res.Insert(kParentProcess.log_key(), StringView(parentSb.data, parentSb.size));
    }
    return res;
}

bool ProcessCacheManager::FinalizeProcessTags(PipelineEventGroup& eventGroup, uint32_t pid, uint64_t ktime) {
    auto execId = GenerateExecId(pid, ktime);
    auto proc = LookupCache(execId);
    if (!proc) {
        LOG_ERROR(sLogger, ("cannot find proc in cache, execId", execId)("pid", pid)("ktime", ktime));
        return false;
    }
    // finalize proc tags

    auto parentExecId = GenerateParentExecId(proc);
    auto parentProc = LookupCache(execId);
    // finalize proc tags
    std::string args = proc->process.args; // TODO
    std::string binary = proc->process.args; // TODO
    std::string permitted = GetCapabilities(proc->msg->creds.caps.permitted);
    std::string effective = GetCapabilities(proc->msg->creds.caps.effective);
    std::string inheritable = GetCapabilities(proc->msg->creds.caps.inheritable);

    rapidjson::Document::AllocatorType allocator;
    rapidjson::Value cap(rapidjson::kObjectType);

    cap.AddMember("permitted", rapidjson::Value().SetString(permitted.c_str(), allocator), allocator);
    cap.AddMember("effective", rapidjson::Value().SetString(effective.c_str(), allocator), allocator);
    cap.AddMember("inheritable", rapidjson::Value().SetString(inheritable.c_str(), allocator), allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    cap.Accept(writer);

    std::string capStr = buffer.GetString();

    // event_type, added by xxx_security_manager
    // call_name, added by xxx_security_manager
    // event_time, added by xxx_security_manager
    eventGroup.SetTag("exec_id", proc->exec_id);
    eventGroup.SetTag("parent_exec_id", proc->parent_exec_id);
    eventGroup.SetTag("pid", std::to_string(proc->process.pid));
    eventGroup.SetTag("uid", std::to_string(proc->process.uid));
    eventGroup.SetTag("user", proc->process.user.name);
    eventGroup.SetTag("binary", binary);
    eventGroup.SetTag("arguments", args);
    eventGroup.SetTag("cwd", proc->process.cwd);
    eventGroup.SetTag("ktime", std::to_string(proc->process.ktime));
    eventGroup.SetTag("cap", capStr);

    // for parent
    if (!parentProc) {
        eventGroup.SetTag("parent_process", UNKOWN_STR);
        return true;
    }
    { // finalize parent tags
        std::string permitted = GetCapabilities(parentProc->msg->creds.caps.permitted);
        std::string effective = GetCapabilities(parentProc->msg->creds.caps.effective);
        std::string inheritable = GetCapabilities(parentProc->msg->creds.caps.inheritable);

        rapidjson::Document d;
        d.SetObject();

        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

        d.AddMember("exec_id", rapidjson::Value().SetString(parentProc->exec_id.c_str(), allocator), allocator);
        d.AddMember(
            "parent_exec_id", rapidjson::Value().SetString(parentProc->parent_exec_id.c_str(), allocator), allocator);
        d.AddMember(
            "pid", rapidjson::Value().SetString(std::to_string(parentProc->process.pid).c_str(), allocator), allocator);
        d.AddMember(
            "uid", rapidjson::Value().SetString(std::to_string(parentProc->process.uid).c_str(), allocator), allocator);
        d.AddMember("user", rapidjson::Value().SetString(parentProc->process.user.name.c_str(), allocator), allocator);
        d.AddMember("binary", rapidjson::Value().SetString(binary.c_str(), allocator), allocator);
        d.AddMember("arguments", rapidjson::Value().SetString(args.c_str(), allocator), allocator);
        d.AddMember("cwd", rapidjson::Value().SetString(parentProc->process.cwd.c_str(), allocator), allocator);
        d.AddMember("ktime",
                    rapidjson::Value().SetString(std::to_string(parentProc->process.ktime).c_str(), allocator),
                    allocator);

        rapidjson::Value cap(rapidjson::kObjectType);

        cap.AddMember("permitted", rapidjson::Value().SetString(permitted.c_str(), allocator), allocator);
        cap.AddMember("effective", rapidjson::Value().SetString(effective.c_str(), allocator), allocator);
        cap.AddMember("inheritable", rapidjson::Value().SetString(inheritable.c_str(), allocator), allocator);

        d.AddMember("cap", cap, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        d.Accept(writer);

        std::string result = buffer.GetString();
        eventGroup.SetTag("parent_process", result);
    }
    return true;
}

} // namespace ebpf
} // namespace logtail
