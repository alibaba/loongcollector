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

#include <memory>
#include "ebpf/plugin/RetryableEvent.h"
#include "ebpf/plugin/network_observer/Connection.h"
#include "ebpf/plugin/network_observer/Type.h"

#pragma once

namespace logtail::ebpf {

class ConnStatsRetryableEvent : public RetryableEvent {
public:
    // enum TaskId { kIncrementParentRef, kAttachContainerMeta, kAttachK8sPodMeta, kFlushEvent, kDone };
    ConnStatsRetryableEvent(int retryLimit)
        : RetryableEvent(retryLimit) {}

    virtual ~ConnStatsRetryableEvent() = default;

    bool HandleMessage() override;

    bool OnRetry() override;

    void OnDrop() override;

private:
    bool attachContainerMeta(bool isRetry);
    bool attachK8sPodMeta(bool isRetry);
    
    // timestamp
};

// auto* connStatsRecord = static_cast<ConnStatsRecord*>(record.get());
//             if (!connStatsRecord || !connStatsRecord->GetConnection()) {
//                 // should not happen
//                 return;
//             }
//             if (!connStatsRecord->GetConnection()->IsMetaAttachReadyForNetRecord()) {
//                 // rollback
//                 handleRollback(record, isDrop);
//                 if (isDrop) {
//                     ADD_COUNTER(mNetMetaAttachFailedTotal, 1);
//                 } else {
//                     ADD_COUNTER(mNetMetaAttachRollbackTotal, 1);
//                 }
//                 return;
//             }
//             ADD_COUNTER(mNetMetaAttachSuccessTotal, 1);

//             // handle record
//             // do aggregate
//             {
//                 WriteLock lk(mNetAggLock);
//                 auto res = mNetAggregator.Aggregate(record, GenerateAggKeyForNetMetric(record));
//                 LOG_DEBUG(sLogger, ("agg res", res)("node count", mNetAggregator.NodeCount()));
//             }

//             break;

} // namespace logtail::ebpf
