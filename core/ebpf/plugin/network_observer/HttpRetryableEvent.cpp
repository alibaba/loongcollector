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

#include "ebpf/plugin/network_observer/HttpRetryableEvent.h"

namespace logtail::ebpf {

bool HttpRetryableEvent::HandleMessage() {
    if (!mRecord || !mRecord->GetConnection() || !mRecord->GetAppDetail()) {
        // should not happen
        LOG_WARNING(sLogger, ("no record or connection or no app detail", ""));
        return true;
    }

    if (!mRecord->GetConnection()->IsMetaAttachReadyForAppRecord()) {
        // try attach
        mRecord->GetConnection()->TryAttachPeerMeta();
        mRecord->GetConnection()->TryAttachSelfMeta();
    }

    if (!mRecord->GetConnection()->IsMetaAttachReadyForAppRecord()) {
        ADD_COUNTER(mRecord->GetAppDetail()->mAppMetaAttachRollbackTotal, 1);
        return false;
    }

    // success
    ADD_COUNTER(mRecord->GetAppDetail()->mAppMetaAttachSuccessTotal, 1);
    return true;
}

bool HttpRetryableEvent::flushEvent() {
    if (!mCommonEventQueue.try_enqueue(mRecord)) {
        // don't use move as it will set mProcessEvent to nullptr even
        // if enqueue failed, this is unexpected but don't know why
        LOG_WARNING(sLogger,
                    ("event", "Failed to enqueue http record")("pid", mRecord->GetPath()));
        // TODO: Alarm discard event if it is called by OnDrop
        return false;
    }
    return true;
}

bool HttpRetryableEvent::OnRetry() {
    return HandleMessage();
}

void HttpRetryableEvent::OnDrop() {
    if (mRecord && mRecord->GetAppDetail()) {
        ADD_COUNTER(mRecord->GetAppDetail()->mAppMetaAttachFailedTotal, 1);
    }
    
}

}
