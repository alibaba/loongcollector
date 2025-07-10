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

#include "AbstractManager.h"

#include <coolbpf/security/type.h>

#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {
AbstractManager::AbstractManager(const std::shared_ptr<ProcessCacheManager>& processCacheMgr,
                                 const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                 moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue)
    : mProcessCacheManager(processCacheMgr), mEBPFAdapter(eBPFAdapter), mCommonEventQueue(queue) {
}

AbstractManager::~AbstractManager() {
}

} // namespace logtail::ebpf
