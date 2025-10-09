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

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <mutex>

namespace logtail {

/**
 * @brief 简化的定时任务管理器
 * 
 * 负责管理定时任务，如checkpoint刷新、连接清理等
 * 在事件驱动模式下替代原来的轮询定时操作
 */
class TimerManager {
public:
    using TimerCallback = std::function<void()>;
    
    TimerManager() = default;
    ~TimerManager() = default;
    
    // Delete copy and move operations
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;
    
    /**
     * @brief 添加定时任务
     * @param name 任务名称
     * @param callback 回调函数
     * @param interval 执行间隔
     */
    void AddTimer(const std::string& name, TimerCallback callback, std::chrono::milliseconds interval);
    
    /**
     * @brief 处理到期的定时任务
     */
    void ProcessTimers();
    
    /**
     * @brief 获取下一个任务到期时间
     * @return 距离下一个任务到期的时间，如果没有任务返回100ms
     */
    std::chrono::milliseconds GetNextTimerInterval() const;

private:
    struct TimerInfo {
        TimerCallback callback;
        std::chrono::steady_clock::time_point nextRun;
        std::chrono::milliseconds interval;
        
        // 默认构造函数
        TimerInfo() = default;
        
        TimerInfo(TimerCallback cb, std::chrono::milliseconds ivl)
            : callback(std::move(cb)), interval(ivl) {
            nextRun = std::chrono::steady_clock::now() + interval;
        }
    };
    
    mutable std::mutex mMutex;
    std::map<std::string, TimerInfo> mTimers;
};

} // namespace logtail
