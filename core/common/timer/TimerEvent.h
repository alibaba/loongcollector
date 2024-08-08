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

#include <chrono>

namespace logtail {

class TimerEvent {
public:
    TimerEvent(std::chrono::steady_clock::time_point execTime) : mExecTime(execTime) {}
    virtual ~TimerEvent() = default;

    virtual bool IsValid() const = 0;
    virtual bool Execute() = 0;

    std::chrono::steady_clock::time_point GetExecTime() const { return mExecTime; }

private:
    std::chrono::steady_clock::time_point mExecTime;
};

} // namespace logtail
