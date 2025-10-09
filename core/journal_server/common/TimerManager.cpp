/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TimerManager.h"
#include "logger/Logger.h"

namespace logtail {

void TimerManager::AddTimer(const std::string& name, TimerCallback callback, std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(mMutex);
    mTimers[name] = TimerInfo(std::move(callback), interval);
    LOG_INFO(sLogger, ("timer added", "")("name", name)("interval_ms", interval.count()));
}

void TimerManager::ProcessTimers() {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& pair : mTimers) {
        TimerInfo& timer = pair.second;
        if (now >= timer.nextRun) {
            try {
                timer.callback();
                timer.nextRun = now + timer.interval;
            } catch (const std::exception& e) {
                LOG_ERROR(sLogger, ("timer callback exception", e.what())("timer", pair.first));
            } catch (...) {
                LOG_ERROR(sLogger, ("timer callback unknown exception", "")("timer", pair.first));
            }
        }
    }
}

std::chrono::milliseconds TimerManager::GetNextTimerInterval() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (mTimers.empty()) {
        return std::chrono::milliseconds(100); // 默认100ms
    }
    
    auto now = std::chrono::steady_clock::now();
    std::chrono::milliseconds minInterval = std::chrono::milliseconds::max();
    
    for (const auto& pair : mTimers) {
        const TimerInfo& timer = pair.second;
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(timer.nextRun - now);
        if (remaining.count() <= 0) {
            return std::chrono::milliseconds(0); // 有定时器已过期
        }
        if (remaining < minInterval) {
            minInterval = remaining;
        }
    }
    
    return minInterval;
}

} // namespace logtail
