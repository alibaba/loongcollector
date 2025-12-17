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

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "common/timer/TimerEvent.h"
#include "host_monitor/entity/ProcessEntityRunner.h"

namespace logtail {

// Use forward declaration from ProcessEntityRunner.h
using ProcessEntityCollectContextPtr = std::shared_ptr<ProcessEntityCollectContext>;

class ProcessEntityTimerEvent : public TimerEvent {
public:
    explicit ProcessEntityTimerEvent(ProcessEntityCollectContextPtr context)
        : TimerEvent(context->GetScheduleTime()), mContext(context) {}

    bool IsValid() const override;
    bool Execute() override;

private:
    ProcessEntityCollectContextPtr mContext;
};

} // namespace logtail
