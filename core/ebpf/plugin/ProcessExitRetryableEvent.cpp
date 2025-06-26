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

#include "ebpf/plugin/ProcessExitRetryableEvent.h"

#include <memory>

#include "metadata/ContainerMetadata.h"
#include "metadata/K8sMetadata.h"
#include "security/bpf_process_event_type.h"
#include "security/data_msg.h"

namespace logtail::ebpf {

bool ProcessExitRetryableEvent::HandleMessage() {
    LOG_DEBUG(sLogger,
              ("pid", mRawEvent->current.pid)("ktime", mRawEvent->current.ktime)("event", "execve")("action",
                                                                                                    "HandleMessage"));
    if (mFlushProcessEvent) {
        mProcessExitEvent = std::make_shared<ProcessExitEvent>(mRawEvent->current.pid,
                                                               mRawEvent->current.ktime,
                                                               KernelEventType::PROCESS_EXIT_EVENT,
                                                               mRawEvent->common.ktime,
                                                               mRawEvent->info.code,
                                                               mRawEvent->info.tid);
    }
    mProcessCacheValue = mProcessCache.Lookup({mRawEvent->current.pid, mRawEvent->current.ktime});
    if (!mProcessCacheValue) {
        mOwnedRawEvent = std::make_unique<msg_exit>(*mRawEvent);
        mRawEvent = mOwnedRawEvent.get();
        return false;
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
    if (IsTaskCompleted(kFlushEvent) && decrementRef()) {
        CompleteTask(kDecrementRef);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    mOwnedRawEvent = std::make_unique<msg_exit>(*mRawEvent);
    mRawEvent = mOwnedRawEvent.get();
    return false;
}

bool ProcessExitRetryableEvent::attachContainerMeta(bool isRetry) {
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

bool ProcessExitRetryableEvent::attachK8sPodMeta(bool isRetry) {
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

bool ProcessExitRetryableEvent::flushEvent() {
    if (!mFlushProcessEvent) {
        return true;
    }
    if (!mCommonEventQueue.try_enqueue(mProcessExitEvent)) {
        // don't use move as it will set mProcessEvent to nullptr even
        // if enqueue failed, this is unexpected but don't know why
        LOG_WARNING(sLogger,
                    ("event", "Failed to enqueue process clone event")("pid", mRawEvent->current.pid)(
                        "ktime", mRawEvent->current.ktime));
        // TODO: Alarm discard event if it is called by OnDrop
        return false;
    }
    return true;
}

bool ProcessExitRetryableEvent::decrementRef() {
    if (mProcessCacheValue->mPPid > 0 || mProcessCacheValue->mPKtime > 0) {
        data_event_id key{mProcessCacheValue->mPPid, mProcessCacheValue->mPKtime};
        auto value = mProcessCache.Lookup(key);
        if (!value) {
            return false;
        }
        mProcessCache.DecRef(key, value);
        LOG_DEBUG(
            sLogger,
            ("pid", mRawEvent->current.pid)("ktime", mRawEvent->current.ktime)("event", "exit")(
                "action", "DecRef parent")("ppid", mProcessCacheValue->mPPid)("pktime", mProcessCacheValue->mPKtime));
    }
    mProcessCache.DecRef({mRawEvent->current.pid, mRawEvent->current.ktime}, mProcessCacheValue);
    LOG_DEBUG(
        sLogger,
        ("pid", mRawEvent->current.pid)("ktime", mRawEvent->current.ktime)("event", "exit")("action", "DecRef self"));
    return true;
}

bool ProcessExitRetryableEvent::OnRetry() {
    if (!mProcessCacheValue) {
        mProcessCacheValue = mProcessCache.Lookup({mRawEvent->current.pid, mRawEvent->current.ktime});
        if (!mProcessCacheValue) {
            return false;
        }
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

    if (IsTaskCompleted(kFlushEvent) && !IsTaskCompleted(kDecrementRef) && decrementRef()) {
        CompleteTask(kDecrementRef);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

void ProcessExitRetryableEvent::OnDrop() {
    if (mProcessCacheValue) {
        if (!IsTaskCompleted(kFlushEvent)) {
            flushEvent();
        }
        if (!IsTaskCompleted(kDecrementRef) && decrementRef()) {
            CompleteTask(kDecrementRef);
        }
        if (!IsTaskCompleted(kDecrementRef)) {
            // dec self ref
            mProcessCache.DecRef({mRawEvent->current.pid, mRawEvent->current.ktime}, mProcessCacheValue);
            LOG_DEBUG(sLogger,
                      ("pid", mRawEvent->current.pid)("ktime", mRawEvent->current.ktime)("event", "exit")(
                          "action", "DecRef self"));
        }
    }
}

} // namespace logtail::ebpf
