// Copyright 2025 iLogtail Authors
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
#include <cstddef>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

#include "EBPFServer.h"
#include "Flags.h"
#include "ProcessCache.h"
#include "TimeKeeper.h"
#include "_thirdparty/coolbpf/src/security/bpf_process_event_type.h"
#include "common/ProcParser.h"
#include "common/StringTools.h"
#include "common/StringView.h"
#include "ebpf/plugin/ProcessCloneRetryableEvent.h"
#include "ebpf/plugin/ProcessExecveRetryableEvent.h"
#include "ebpf/plugin/ProcessExitRetryableEvent.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "type/table/BaseElements.h"

DEFINE_FLAG_INT32(max_ebpf_max_process_data_map_size, "Size of the data events cache", 2048);
DEFINE_FLAG_INT32(max_ebpf_process_cache_size, "Size of the process cache", 131072);
DEFINE_FLAG_INT32(ebpf_process_cache_gc_interval_sec,
                  "Time in seconds between checking the process cache for expired entries",
                  30);

namespace logtail::ebpf {

/////////// ================= for perfbuffer handlers ================= ///////////
void HandleKernelProcessEvent(void* ctx, [[maybe_unused]] int cpu, void* data, [[maybe_unused]] uint32_t data_sz) {
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
            auto* rawEvent = static_cast<struct msg_clone_event*>(data);
            std::unique_ptr<ProcessCloneRetryableEvent> event(
                processCacheMgr->CreateProcessCloneRetryableEvent(rawEvent));
            if (!event->HandleMessage()) {
                processCacheMgr->EventCache().AddEvent(std::move(event));
            }
            // processCacheMgr->RecordCloneEvent(rawEvent);
            break;
        }
        case MSG_OP_EXIT: {
            auto* rawEvent = static_cast<struct msg_exit*>(data);
            std::unique_ptr<ProcessExitRetryableEvent> event(
                processCacheMgr->CreateProcessExitRetryableEvent(rawEvent));
            if (!event->HandleMessage()) {
                processCacheMgr->EventCache().AddEvent(std::move(event));
            }
            // processCacheMgr->RecordExitEvent(rawEvent);
            break;
        }
        case MSG_OP_EXECVE: {
            auto* rawEvent = static_cast<struct msg_execve_event*>(data);
            std::unique_ptr<ProcessExecveRetryableEvent> event(
                processCacheMgr->CreateProcessExecveRetryableEvent(rawEvent));

            if (!event->HandleMessage()) {
                processCacheMgr->EventCache().AddEvent(std::move(event));
            }
            // processCacheMgr->RecordExecveEvent(rawEvent);
            break;
        }
        case MSG_OP_DATA: {
            auto* rawEvent = static_cast<msg_data*>(data);
            processCacheMgr->RecordDataEvent(rawEvent);
            break;
        }
        default: {
            LOG_WARNING(sLogger, ("Unknown event op", static_cast<int>(common->op)));
            break;
        }
    }
}

void HandleKernelProcessEventLost(void* ctx, int cpu, unsigned long long cnt) {
    LOG_WARNING(sLogger, ("kernel event loss, lost_count", cnt)("type", "process security"));
    auto* processCacheMgr = static_cast<ProcessCacheManager*>(ctx);
    if (!processCacheMgr) {
        LOG_ERROR(sLogger, ("ProcessCacheManager is null!", "")("lost events", cnt)("cpu", cpu));
        return;
    }
    processCacheMgr->UpdateLossEventTotal(cnt);
}

////////////////////////////////////////////////////////////////////////////////////////

ProcessCacheManager::ProcessCacheManager(std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                         const std::string& hostName,
                                         const std::string& hostPathPrefix,
                                         moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                         CounterPtr pollEventsTotal,
                                         CounterPtr lossEventsTotal,
                                         CounterPtr processCacheMissTotal,
                                         IntGaugePtr processCacheSize,
                                         IntGaugePtr processDataMapSize,
                                         RetryableEventCache& retryableEventCache)
    : mEBPFAdapter(eBPFAdapter),
      mHostPathPrefix(hostPathPrefix),
      mProcParser(hostPathPrefix),
      mProcessCache(INT32_FLAG(max_ebpf_process_cache_size), mProcParser),
      mProcessDataMap(INT32_FLAG(max_ebpf_max_process_data_map_size)),
      mRetryableEventCache(retryableEventCache),
      mHostName(hostName),
      mCommonEventQueue(queue),
      mPollProcessEventsTotal(std::move(pollEventsTotal)),
      mLossProcessEventsTotal(std::move(lossEventsTotal)),
      mProcessCacheMissTotal(std::move(processCacheMissTotal)),
      mProcessCacheSize(std::move(processCacheSize)),
      mProcessDataMapSize(std::move(processDataMapSize)) {
}

bool ProcessCacheManager::Init() {
    if (mInited) {
        return true;
    }
    auto ebpfConfig = std::make_unique<PluginConfig>();
    ebpfConfig->mPluginType = PluginType::PROCESS_SECURITY;
    ProcessConfig pconfig;

    pconfig.mPerfBufferSpec = {{"tcpmon_map", 128, this, HandleKernelProcessEvent, HandleKernelProcessEventLost}};
    ebpfConfig->mConfig = pconfig;
    bool status = mEBPFAdapter->StartPlugin(PluginType::PROCESS_SECURITY, std::move(ebpfConfig));
    if (!status) {
        LOG_ERROR(sLogger, ("start process security plugin", "failed"));
        return false;
    }
    LOG_INFO(sLogger, ("start process security plugin", "success"));
    ebpf::EBPFServer::GetInstance()->RegisterPluginPerfBuffers(PluginType::PROCESS_SECURITY);
    mInited = true;
    auto ret = syncAllProc(); // write process cache contention with pollPerfBuffers
    if (ret) {
        LOG_WARNING(sLogger, ("failed to sync all proc, ret", ret));
    }
    return true;
}

void ProcessCacheManager::Stop() {
    if (!mInited) {
        return;
    }
    ebpf::EBPFServer::GetInstance()->UnregisterPluginPerfBuffers(PluginType::PROCESS_SECURITY);
    mInited = false;
    waitForConsumeFinished();
    auto res = mEBPFAdapter->StopPlugin(PluginType::PROCESS_SECURITY);
    if (res) {
        LOG_INFO(sLogger, ("stop process security plugin", "success"));
    } else {
        LOG_WARNING(sLogger, ("stop process security plugin", "failed"));
    }
    mProcessCache.Clear();
    mProcessDataMap.Clear();
}

void ProcessCacheManager::waitForConsumeFinished() {
    int64_t startTime = TimeKeeper::GetInstance()->NowSec();
    bool alarmOnce = false;
    while (mIsConsuming) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms
        int64_t duration = TimeKeeper::GetInstance()->NowSec() - startTime;
        if (!alarmOnce && duration > 10) { // 10s
            LOG_ERROR(sLogger, ("ProcessCacheManager stop", "too slow")("cost", duration));
            AlarmManager::GetInstance()->SendAlarmError(
                CONFIG_UPDATE_ALARM, std::string("ProcessCacheManager stop too slow; cost:" + ToString(duration)));
            alarmOnce = true;
        }
    }
}

void ProcessCacheManager::UpdateRecvEventTotal(uint64_t count) {
    ADD_COUNTER(mPollProcessEventsTotal, count);
}
void ProcessCacheManager::UpdateLossEventTotal(uint64_t count) {
    ADD_COUNTER(mLossProcessEventsTotal, count);
}

ProcessSyncRetryableEvent* ProcessCacheManager::CreateProcessSyncRetryableEvent(const Proc& proc) {
    return new ProcessSyncRetryableEvent(
        INT32_FLAG(ebpf_event_retry_limit), proc, mProcessCache, mHostName, mProcParser);
}

ProcessExecveRetryableEvent* ProcessCacheManager::CreateProcessExecveRetryableEvent(msg_execve_event* eventPtr) {
    return new ProcessExecveRetryableEvent(std::max(1, INT32_FLAG(ebpf_event_retry_limit)),
                                           *eventPtr,
                                           mHostName,
                                           mProcParser,
                                           mProcessDataMap,
                                           mProcessCache,
                                           mRetryableEventCache,
                                           mCommonEventQueue,
                                           mFlushProcessEvent);
}

ProcessCloneRetryableEvent* ProcessCacheManager::CreateProcessCloneRetryableEvent(msg_clone_event* eventPtr) {
    return new ProcessCloneRetryableEvent(
        INT32_FLAG(ebpf_event_retry_limit), *eventPtr, mProcessCache, mHostName, mFlushProcessEvent, mCommonEventQueue);
}

ProcessExitRetryableEvent* ProcessCacheManager::CreateProcessExitRetryableEvent(msg_exit* eventPtr) {
    return new ProcessExitRetryableEvent(
        INT32_FLAG(ebpf_event_retry_limit), *eventPtr, mProcessCache, mFlushProcessEvent, mCommonEventQueue);
}

void ProcessCacheManager::RecordDataEvent(msg_data* eventPtr) {
    LOG_DEBUG(sLogger,
              ("[receive_data_event] size", eventPtr->common.size)("pid", eventPtr->id.pid)("time", eventPtr->id.time)(
                  "data", std::string(eventPtr->arg, eventPtr->common.size - offsetof(msg_data, arg))));
    mProcessDataMap.DataAdd(eventPtr);
}

bool ProcessCacheManager::AttachProcessData(uint32_t pid,
                                            uint64_t ktime,
                                            LogEvent& logEvent,
                                            PipelineEventGroup& eventGroup) {
    auto procPtr = mProcessCache.Lookup({pid, ktime});
    if (!procPtr) {
        ADD_COUNTER(mProcessCacheMissTotal, 1);
        return false;
    }

    auto& proc = *procPtr;
    eventGroup.AddSourceBuffer(proc.GetSourceBuffer());
    if (proc.mParent) {
        eventGroup.AddSourceBuffer(proc.mParent->GetSourceBuffer());
    }

    logEvent.SetContentNoCopy(kExecId.LogKey(), proc.Get<kExecId>());
    logEvent.SetContentNoCopy(kProcessId.LogKey(), proc.Get<kProcessId>());
    logEvent.SetContentNoCopy(kUid.LogKey(), proc.Get<kUid>());
    logEvent.SetContentNoCopy(kUser.LogKey(), proc.Get<kUser>());
    logEvent.SetContentNoCopy(kBinary.LogKey(), proc.Get<kBinary>());
    logEvent.SetContentNoCopy(kArguments.LogKey(), proc.Get<kArguments>());
    logEvent.SetContentNoCopy(kCWD.LogKey(), proc.Get<kCWD>());
    logEvent.SetContentNoCopy(kKtime.LogKey(), proc.Get<kKtime>());

    logEvent.SetContentNoCopy(kCapPermitted.LogKey(), proc.Get<kCapPermitted>());
    logEvent.SetContentNoCopy(kCapEffective.LogKey(), proc.Get<kCapEffective>());
    logEvent.SetContentNoCopy(kCapInheritable.LogKey(), proc.Get<kCapInheritable>());

    if (!proc.Get<kContainerId>().empty()) {
        logEvent.SetContentNoCopy(kContainerId.LogKey(), proc.Get<kContainerId>());
    }

    auto& sb = logEvent.GetSourceBuffer();
    auto containerInfo = proc.LoadContainerInfo();
    if (containerInfo) {
        auto containerName = sb->CopyString(containerInfo->mContainerName);
        logEvent.SetContentNoCopy(kContainerName.LogKey(), StringView(containerName.data, containerName.size));
        auto imageName = sb->CopyString(containerInfo->mImageName);
        logEvent.SetContentNoCopy(kContainerImageName.LogKey(), StringView(imageName.data, imageName.size));
    }

    auto podInfo = proc.LoadK8sPodInfo();
    if (podInfo) {
        auto workloadKind = sb->CopyString(podInfo->mWorkloadKind);
        logEvent.SetContentNoCopy(kWorkloadKind.LogKey(), StringView(workloadKind.data, workloadKind.size));
        auto workloadName = sb->CopyString(podInfo->mWorkloadName);
        logEvent.SetContentNoCopy(kWorkloadName.LogKey(), StringView(workloadName.data, workloadName.size));
        auto namespaceStr = sb->CopyString(podInfo->mNamespace);
        logEvent.SetContentNoCopy(kNamespace.LogKey(), StringView(namespaceStr.data, namespaceStr.size));
        auto podName = sb->CopyString(podInfo->mPodName);
        logEvent.SetContentNoCopy(kPodName.LogKey(), StringView(podName.data, podName.size));
    }

    auto& parentProcPtr = proc.mParent;
    if (parentProcPtr) {
        auto& parentProc = *parentProcPtr;
        logEvent.SetContentNoCopy(kParentExecId.LogKey(), parentProc.Get<kExecId>());
        logEvent.SetContentNoCopy(kParentProcessId.LogKey(), parentProc.Get<kProcessId>());
        logEvent.SetContentNoCopy(kParentUid.LogKey(), parentProc.Get<kUid>());
        logEvent.SetContentNoCopy(kParentUser.LogKey(), parentProc.Get<kUser>());
        logEvent.SetContentNoCopy(kParentBinary.LogKey(), parentProc.Get<kBinary>());
        logEvent.SetContentNoCopy(kParentArguments.LogKey(), parentProc.Get<kArguments>());
        logEvent.SetContentNoCopy(kParentCWD.LogKey(), parentProc.Get<kCWD>());
        logEvent.SetContentNoCopy(kParentKtime.LogKey(), parentProc.Get<kKtime>());

        if (!parentProc.Get<kContainerId>().empty()) {
            logEvent.SetContentNoCopy(kParentContainerId.LogKey(), parentProc.Get<kContainerId>());
        }
    }

    return true;
}

int ProcessCacheManager::syncAllProc() {
    std::vector<std::shared_ptr<Proc>> procs = listRunningProcs();
    // update execve map
    for (auto& proc : procs) {
        writeProcToBPFMap(proc);
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
    mEBPFAdapter->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &key.pid, &value, 0);

    // generage execve event ...
    for (const auto& proc : procs) {
        std::unique_ptr<ProcessSyncRetryableEvent> event(CreateProcessSyncRetryableEvent(*proc));
        if (!event->HandleMessage()) {
            EventCache().AddEvent(std::move(event));
        }
    }
    return 0;
}

std::vector<std::shared_ptr<Proc>> ProcessCacheManager::listRunningProcs() {
    std::vector<std::shared_ptr<Proc>> processes;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(mHostPathPrefix / "proc", ec)) {
        if (ec) {
            continue;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const std::string& dirName = entry.path().filename().string();
        int32_t pid = 0;
        if (!StringTo(dirName, pid)) {
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

int ProcessCacheManager::writeProcToBPFMap(const std::shared_ptr<Proc>& proc) {
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
    value.bin.path_length = std::min(BINARY_PATH_MAX_LEN, static_cast<int>(proc->exe.size()));
    ::memcpy(value.bin.path, proc->exe.data(), value.bin.path_length);

    // update bpf map
    int res = mEBPFAdapter->BPFMapUpdateElem(PluginType::PROCESS_SECURITY, "execve_map", &proc->pid, &value, 0);
    LOG_DEBUG(sLogger, ("update bpf map, pid", proc->pid)("res", res));
    return res;
}

int ProcessCacheManager::ConsumePerfBufferData() {
    int ret = 0;
    mIsConsuming = true;
    if (mInited) {
        mEBPFAdapter->ConsumePerfBufferData(PluginType::PROCESS_SECURITY);
        LOG_DEBUG(sLogger, ("process cache consume buffer", "")("cnt", ret));
    }
    mIsConsuming = false;

    return ret;
}

void ProcessCacheManager::ClearProcessExpiredCache() {
    if (mInited) {
        auto now = TimeKeeper::GetInstance()->NowSec();
        if (now > mLastProcessCacheClearTime + INT32_FLAG(ebpf_process_cache_gc_interval_sec)) {
            mProcessCache.ClearExpiredCache();
            mLastProcessCacheClearTime = now;
            int processCacheSize = mProcessCache.Size();
            SET_GAUGE(mProcessCacheSize, processCacheSize);
            SET_GAUGE(mProcessDataMapSize, mProcessDataMap.Size());
            if (processCacheSize > INT32_FLAG(max_ebpf_process_cache_size)) {
                mProcessCache.ForceShrink();
                LOG_WARNING(sLogger,
                            ("process cache size exceed limit, current size",
                             processCacheSize)("after force shrink size", mProcessCache.Size()));
            }
        }
    }
}

} // namespace logtail::ebpf
