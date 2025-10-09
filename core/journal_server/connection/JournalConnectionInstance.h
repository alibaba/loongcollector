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
#include <mutex>
#include <string>
#include <atomic>

#include "../reader/JournalReader.h"
#include "../common/JournalConfig.h"

namespace logtail {

/**
 * @brief 管理单个config的journal连接实例
 * 
 * 参考Go版本的ServiceJournal设计，每个连接包含：
 * - 独立的journal reader实例
 * - 创建时间（用于重置周期判断）
 * - 配置信息的缓存
 * - 连接使用状态管理
 */
class JournalConnectionInstance {
public:
    JournalConnectionInstance(const std::string& configName, 
                         size_t idx, 
                         const JournalConfig& config);
    ~JournalConnectionInstance();

    // Delete copy and move operations
    JournalConnectionInstance(const JournalConnectionInstance&) = delete;
    JournalConnectionInstance& operator=(const JournalConnectionInstance&) = delete;
    JournalConnectionInstance(JournalConnectionInstance&&) = delete;
    JournalConnectionInstance& operator=(JournalConnectionInstance&&) = delete;

    // 获取journal reader，如果连接断开会尝试重连
    std::shared_ptr<SystemdJournalReader> GetReader();
    
    // 检查连接是否需要重置（基于时间）
    bool ShouldReset(int resetIntervalSec) const;
    
    // 强制重置连接
    bool ResetConnection();
    
    // 检查连接是否有效
    bool IsValid() const;
    
    // 强制重置管理
    void MarkForReset() const;
    bool IsPendingReset() const;
    void ClearResetFlag();
    
    // 获取连接信息
    const std::string& GetConfigName() const { return mConfigName; }
    size_t GetIndex() const { return mIndex; }
    std::chrono::steady_clock::time_point GetCreateTime() const { return mCreateTime; }

private:
    bool initializeConnection();
    
    std::string mConfigName;
    size_t mIndex;
    JournalConfig mConfig;
    
    std::shared_ptr<SystemdJournalReader> mReader;
    std::chrono::steady_clock::time_point mCreateTime;
    std::chrono::steady_clock::time_point mLastResetTime;
    
    // 强制重置标记 - 当resetInterval到达时标记，阻止新的使用
    mutable std::atomic<bool> mPendingReset{false};
    
    mutable std::mutex mMutex;
    bool mIsValid;
};

} // namespace logtail 