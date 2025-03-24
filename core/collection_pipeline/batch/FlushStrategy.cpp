// Copyright 2024 iLogtail Authors
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

#include "collection_pipeline/batch/FlushStrategy.h"

#include <cstdlib>

using namespace std;

namespace logtail {

template <>
bool EventFlushStrategy<SLSEventBatchStatus>::NeedFlushByTime(const SLSEventBatchStatus& status,
                                                              const PipelineEventPtr& e) {
    if (e.Is<MetricEvent>()) {
        // It is necessary to flush, if the event timestamp and the batch creation time differ by more than 300 seconds.
        // The 300 seconds is to avoid frequent batching to reduce the flusher traffic, because metrics such as cAdvisor
        // has out-of-order situations.
        return time(nullptr) - status.GetCreateTime() > mTimeoutSecs
            || abs(status.GetCreateTime() - e->GetTimestamp()) > 300;
    }
    return time(nullptr) - status.GetCreateTime() > mTimeoutSecs
        || status.GetCreateTimeMinute() != e->GetTimestamp() / 60;
}

} // namespace logtail
