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

#include "AggregateEvent.h"

#include "ebpf/EBPFServer.h"

namespace logtail::ebpf {

bool AggregateEvent::Execute() {
    auto manager = EBPFServer::GetInstance()->GetPluginManager(mScheduleConfig->mType);
    if (manager == nullptr || !manager->IsExists()) {
        return false;
    }

    return manager->ScheduleNext(GetExecTime(), mScheduleConfig);
}

bool AggregateEvent::IsValid() const {
    auto manager = EBPFServer::GetInstance()->GetPluginManager(mScheduleConfig->mType);
    if (manager == nullptr) {
        return false;
    }
    return manager->IsExists();
}

} // namespace logtail::ebpf
