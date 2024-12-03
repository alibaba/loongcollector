/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HostMonitorTimerEvent.h"

#include <utility>

#include "HostMonitorInputRunner.h"

namespace logtail {

bool HostMonitorTimerEvent::IsValid() const {
    return HostMonitorInputRunner::GetInstance()->IsCollectTaskValid(mCollectConfig->mConfigName,
                                                                     mCollectConfig->mCollectorName);
}

bool HostMonitorTimerEvent::Execute() {
    HostMonitorInputRunner::GetInstance()->ScheduleOnce(std::move(mCollectConfig));
    return true;
}

} // namespace logtail
