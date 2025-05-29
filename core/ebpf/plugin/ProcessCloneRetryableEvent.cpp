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

#include "ebpf/plugin/ProcessCloneRetryableEvent.h"

#include <memory>

#include "ProcessEvent.h"
#include "common/ProcParser.h"
#include "logger/Logger.h"
#include "metadata/ContainerMetadata.h"
#include "metadata/K8sMetadata.h"
#include "security/bpf_process_event_type.h"
#include "security/data_msg.h"

namespace logtail::ebpf {

bool ProcessCloneRetryableEvent::HandleMessage() {
    LOG_DEBUG(sLogger,
              ("pid", mRawEvent->tgid)("ktime", mRawEvent->ktime)("event", "clone")("action", "HandleMessage"));
    if (mFlushProcessEvent) {
        mProcessEvent = std::make_shared<ProcessEvent>(static_cast<uint32_t>(mRawEvent->tgid),
                                                       static_cast<uint64_t>(mRawEvent->ktime),
                                                       KernelEventType::PROCESS_CLONE_EVENT,
                                                       static_cast<uint64_t>(mRawEvent->common.ktime));
    }
    auto* cacheValue = cloneProcessCacheValue(*mRawEvent);
    if (!cacheValue) {
        mOwnedRawEvent = std::make_unique<msg_clone_event>(*mRawEvent);
        mRawEvent = mOwnedRawEvent.get();
        return false;
    }
    mProcessCacheValue.reset(cacheValue);
    mProcessCache.AddCache({mRawEvent->tgid, mRawEvent->ktime}, mProcessCacheValue);
    LOG_DEBUG(sLogger, ("pid", mRawEvent->tgid)("ktime", mRawEvent->ktime)("event", "clone")("action", "AddCache"));
    if (incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    if (attachContainerMeta(false)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (attachK8sPodMeta(false)) {
        CompleteTask(kAttachK8sPodMeta);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    mOwnedRawEvent = std::make_unique<msg_clone_event>(*mRawEvent);
    mRawEvent = mOwnedRawEvent.get();
    return false;
}

ProcessCacheValue* ProcessCloneRetryableEvent::cloneProcessCacheValue(const msg_clone_event& event) {
    auto parent = mProcessCache.Lookup({event.parent.pid, event.parent.ktime});
    if (!parent) {
        LOG_DEBUG(sLogger,
                  ("parent process not found. ppid",
                   event.parent.pid)("pktime", event.parent.ktime)("pid", event.tgid)("ktime", event.ktime));
        return nullptr;
    }
    auto execId = GenerateExecId(mHostname, event.tgid, event.ktime);
    auto* cacheValue = parent->CloneContents();
    cacheValue->mPPid = event.parent.pid;
    cacheValue->mPKtime = event.parent.ktime;
    cacheValue->SetContent<kExecId>(execId);
    cacheValue->SetContent<kProcessId>(event.tgid);
    cacheValue->SetContent<kKtime>(event.ktime);
    return cacheValue;
}

bool ProcessCloneRetryableEvent::incrementParentRef() {
    if (mRawEvent->parent.pid > 0 || mRawEvent->parent.ktime > 0) {
        data_event_id key{mRawEvent->parent.pid, mRawEvent->parent.ktime};
        auto value = mProcessCache.Lookup(key);
        if (value == nullptr) {
            return false;
        }
        mProcessCache.IncRef(key, value);
        LOG_DEBUG(sLogger,
                  ("pid", mRawEvent->tgid)("ktime", mRawEvent->ktime)("event", "clone")("action", "IncRef parent")(
                      "ppid", mRawEvent->parent.pid)("pktime", mRawEvent->parent.ktime));
    }
    return true;
}

bool ProcessCloneRetryableEvent::attachContainerMeta(bool isRetry) {
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
bool ProcessCloneRetryableEvent::attachK8sPodMeta(bool isRetry) {
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

bool ProcessCloneRetryableEvent::flushEvent() {
    if (!mFlushProcessEvent) {
        return true;
    }
    if (!mCommonEventQueue.try_enqueue(std::move(mProcessEvent))) {
        LOG_ERROR(
            sLogger,
            ("event", "Failed to enqueue process clone event")("pid", mRawEvent->tgid)("ktime", mRawEvent->ktime));
        // TODO: Alarm discard event if it is called by OnDrop
        return false;
    }
    return true;
}

bool ProcessCloneRetryableEvent::OnRetry() {
    if (!mProcessCacheValue) {
        auto* cacheValue = cloneProcessCacheValue(*mRawEvent);
        if (cacheValue == nullptr) {
            return false;
        }
        mProcessCacheValue.reset(cacheValue);
        mProcessCache.AddCache({mRawEvent->tgid, mRawEvent->ktime}, mProcessCacheValue);
        LOG_DEBUG(sLogger, ("pid", mRawEvent->tgid)("ktime", mRawEvent->ktime)("event", "clone")("action", "AddCache"));
    }
    if (!IsTaskCompleted(kIncrementParentRef) && incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    if (!IsTaskCompleted(kAttachContainerMeta) && attachContainerMeta(true)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (!IsTaskCompleted(kAttachK8sPodMeta) && attachK8sPodMeta(true)) {
        CompleteTask(kAttachK8sPodMeta);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && !IsTaskCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

void ProcessCloneRetryableEvent::OnDrop() {
    if (mProcessCacheValue && !IsTaskCompleted(kFlushEvent)) {
        flushEvent();
    }
}

} // namespace logtail::ebpf
