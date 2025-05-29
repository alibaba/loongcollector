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

#include "ebpf/plugin/ProcessCleanupRetryableEvent.h"

#include "logger/Logger.h"
#include "security/data_msg.h"

namespace logtail::ebpf {

bool ProcessCleanupRetryableEvent::HandleMessage() {
    LOG_DEBUG(sLogger, ("pid", mKey.pid)("ktime", mKey.time)("event", "cleanup")("action", "HandleMessage"));
    mProcessCacheValue = mProcessCache.Lookup(mKey);
    if (!mProcessCacheValue) {
        return false;
    }
    if (decrementRef()) {
        CompleteTask(kDecrementRef);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

bool ProcessCleanupRetryableEvent::decrementRef() {
    if (mProcessCacheValue->mPPid > 0 || mProcessCacheValue->mPKtime > 0) {
        data_event_id parentKey{mProcessCacheValue->mPPid, mProcessCacheValue->mPKtime};
        auto parent = mProcessCache.Lookup(parentKey);
        if (!parent) {
            return false;
        }
        // dec parent's ref count
        mProcessCache.DecRef(parentKey, parent);
        LOG_DEBUG(sLogger,
                  ("pid", mKey.pid)("ktime", mKey.time)("event", "cleanup")("action", "DecRef parent")(
                      "ppid", mProcessCacheValue->mPPid)("pktime", mProcessCacheValue->mPKtime));
    }
    // dec self ref count
    mProcessCache.DecRef(mKey, mProcessCacheValue);
    LOG_DEBUG(sLogger, ("pid", mKey.pid)("ktime", mKey.time)("event", "cleanup")("action", "DecRef self"));
    return true;
}

bool ProcessCleanupRetryableEvent::OnRetry() {
    if (!mProcessCacheValue) {
        mProcessCacheValue = mProcessCache.Lookup(mKey);
        if (!mProcessCacheValue) {
            return false;
        }
    }
    if (!IsTaskCompleted(kDecrementRef) && decrementRef()) {
        CompleteTask(kDecrementRef);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

void ProcessCleanupRetryableEvent::OnDrop() {
    if (mProcessCacheValue && !IsTaskCompleted(kDecrementRef)) {
        // dec self ref count
        mProcessCache.DecRef(mKey, mProcessCacheValue);
        LOG_DEBUG(sLogger, ("pid", mKey.pid)("ktime", mKey.time)("event", "cleanup")("action", "DecRef self"));
    }
}

} // namespace logtail::ebpf
