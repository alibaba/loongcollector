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

#include "ebpf/plugin/ProcessSyncRetryableEvent.h"

#include <memory>

#include "common/ProcParser.h"
#include "logger/Logger.h"
#include "metadata/ContainerMetadata.h"
#include "metadata/K8sMetadata.h"
#include "security/data_msg.h"

namespace logtail::ebpf {

bool ProcessSyncRetryableEvent::HandleMessage() {
    LOG_DEBUG(sLogger, ("pid", mRawEvent.pid)("ktime", mRawEvent.ktime)("event", "sync")("action", "HandleMessage"));
    mProcessCacheValue = procToProcessCacheValue(mRawEvent);

    if (incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    if (attachContainerMeta(false)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (attachK8sPodMeta(false)) {
        CompleteTask(kAttachK8sPodMeta);
    }

    mProcessCache.AddCache({mRawEvent.pid, mRawEvent.ktime}, mProcessCacheValue);
    LOG_DEBUG(sLogger, ("push proc event. AddCache pid", mRawEvent.pid)("ktime", mRawEvent.ktime));

    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

std::shared_ptr<ProcessCacheValue> ProcessSyncRetryableEvent::procToProcessCacheValue(const Proc& proc) {
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    auto execId = GenerateExecId(mHostname, proc.pid, proc.ktime);
    StringView rawArgs = proc.cmdline;
    auto nullPos = rawArgs.find('\0');
    if (nullPos != std::string::npos) {
        rawArgs = rawArgs.substr(nullPos + 1);
    } else {
        rawArgs = rawArgs.substr(rawArgs.size(), 0);
    }

    cacheValue->mPPid = proc.ppid;
    cacheValue->mPKtime = proc.pktime;
    cacheValue->SetContent<kArguments>(DecodeArgs(rawArgs));
    LOG_DEBUG(sLogger, ("raw_args", rawArgs)("args", cacheValue->Get<kArguments>()));
    cacheValue->SetContent<kExecId>(execId);
    cacheValue->SetContent<kProcessId>(proc.pid);
    cacheValue->SetContent<kCWD>(proc.cwd);
    cacheValue->SetContent<kKtime>(proc.ktime);

    if (proc.cmdline.empty()) {
        cacheValue->SetContent<kBinary>(proc.comm);
        // event.process.nspid = proc.nspid;
        cacheValue->SetContent<kUid>(0);
        auto userName = mProcParser.GetUserNameByUid(0);
        cacheValue->SetContent<kUser>(userName);
        // event.process.auid = std::numeric_limits<uint32_t>::max();
        // event.process.flags = static_cast<uint32_t>(EVENT_PROC_FS);
    } else {
        cacheValue->SetContent<kBinary>(proc.exe);
        cacheValue->SetContent<kUid>(proc.effectiveUid);
        auto userName = mProcParser.GetUserNameByUid(proc.effectiveUid);
        auto permitted = GetCapabilities(proc.permitted, *cacheValue->GetSourceBuffer());
        auto effective = GetCapabilities(proc.effective, *cacheValue->GetSourceBuffer());
        auto inheritable = GetCapabilities(proc.inheritable, *cacheValue->GetSourceBuffer());
        cacheValue->SetContentNoCopy<kUser>(userName);
        cacheValue->SetContentNoCopy<kCapPermitted>(permitted);
        cacheValue->SetContentNoCopy<kCapEffective>(effective);
        cacheValue->SetContentNoCopy<kCapInheritable>(inheritable);

        // event.process.nspid = proc.nspid;
        // event.process.auid = proc.auid;
        // event.process.flags = proc.flags;
        // event.process.cmdline = proc.cmdline;
        cacheValue->SetContent<kContainerId>(proc.container_id);
    }
    return cacheValue;
}

bool ProcessSyncRetryableEvent::incrementParentRef() {
    if (mProcessCacheValue->mPPid > 0 || mProcessCacheValue->mPKtime > 0) {
        data_event_id key{mProcessCacheValue->mPPid, mProcessCacheValue->mPKtime};
        auto value = mProcessCache.Lookup(key);
        if (value == nullptr) {
            return false;
        }
        mProcessCache.IncRef(key, value);
        LOG_DEBUG(sLogger,
                  ("push clone event. IncRef pid", mProcessCacheValue->mPPid)("ktime", mProcessCacheValue->mPKtime));
    }
    return true;
}

bool ProcessSyncRetryableEvent::attachContainerMeta(bool isRetry) {
    if (!mProcessCacheValue) {
        return false;
    }
    const auto& containerId = mProcessCacheValue->Get<kContainerId>();
    if (containerId.empty() || !ContainerMetadata::GetInstance().Enable()) {
        return true;
    }
    auto containerMeta = ContainerMetadata::GetInstance().GetInfoByContainerId(containerId);
    if (containerMeta) {
        if (!isRetry) {
            mProcessCacheValue->StoreContainerInfoUnsafe(containerMeta);
        } else {
            mProcessCacheValue->StoreContainerInfo(containerMeta);
        }
        return true;
    }
    return false;
}
bool ProcessSyncRetryableEvent::attachK8sPodMeta(bool isRetry) {
    if (!mProcessCacheValue) {
        return false;
    }
    const auto& containerId = mProcessCacheValue->Get<kContainerId>();
    if (containerId.empty() || !K8sMetadata::GetInstance().Enable()) {
        return true;
    }
    auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(containerId);
    if (info) {
        if (!isRetry) {
            mProcessCacheValue->StoreK8sPodInfoUnsafe(info);
        } else {
            mProcessCacheValue->StoreK8sPodInfo(info);
        }
        return true;
    }
    K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::ContainerIdInfo, containerId);
    return false;
}

bool ProcessSyncRetryableEvent::OnRetry() {
    if (!IsTaskCompleted(kIncrementParentRef) && incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    if (!IsTaskCompleted(kAttachContainerMeta) && attachContainerMeta(true)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (!IsTaskCompleted(kAttachK8sPodMeta) && attachK8sPodMeta(false)) {
        CompleteTask(kAttachK8sPodMeta);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

void ProcessSyncRetryableEvent::OnDrop() {
}

} // namespace logtail::ebpf
