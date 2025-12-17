/*
 * Copyright 2025 iLogtail Authors
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

#include "host_monitor/entity/ProcessEntityTimerEvent.h"

#include "host_monitor/entity/ProcessEntityRunner.h"
#include "logger/Logger.h"

namespace logtail {

bool ProcessEntityTimerEvent::IsValid() const {
    return ProcessEntityRunner::GetInstance()->IsCollectTaskValid(mContext->mStartTime, mContext->mConfigName);
}

bool ProcessEntityTimerEvent::Execute() {
    ProcessEntityRunner::GetInstance()->ScheduleOnce(mContext);
    return true;
}

} // namespace logtail
