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

#pragma once

#include <memory>

#include "common/http/HttpRequest.h"
#include "common/timer/TimerEvent.h"

namespace logtail {

class HttpRequestTimerEvent : public TimerEvent {
public:
    HttpRequestTimerEvent(std::chrono::steady_clock::time_point execTime, std::unique_ptr<AsynHttpRequest>&& request)
        : TimerEvent(execTime), mRequest(std::move(request)) {}

    bool IsValid() const override;
    bool Execute() override;

private:
    std::unique_ptr<AsynHttpRequest> mRequest;
};

} // namespace logtail
