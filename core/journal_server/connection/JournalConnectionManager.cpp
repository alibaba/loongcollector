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

#include "JournalConnectionManager.h"
#include "logger/Logger.h"
#include <sstream>

namespace logtail {



//==============================================================================
// JournalConnectionManager 实现  
//==============================================================================

std::shared_ptr<SystemdJournalReader> JournalConnectionManager::GetOrCreateConnection(
    const std::string& configName,
    size_t idx,
    const JournalConfig& config) {
    
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        auto& connInfo = it->second;
        
        // 检查连接状态：有效且未待重置的连接可以使用
        if (connInfo->IsValid() && !connInfo->IsPendingReset()) {
            return connInfo->GetReader();
        } else {
            // 连接无效或待重置，移除并创建新连接
            std::string reason = !connInfo->IsValid() ? "invalid" : "pending reset";
            LOG_WARNING(sLogger, ("existing connection unusable, creating new one", "")("config", configName)("idx", idx)("reason", reason));
            mConnections.erase(it);
        }
    }
    
    // 创建新连接
    try {
        auto connInfo = std::make_shared<JournalConnectionInstance>(configName, idx, config);
        
        if (connInfo->IsValid()) {
            mConnections[key] = connInfo;
            LOG_INFO(sLogger, ("new journal connection created", "")("config", configName)("idx", idx)("total_connections", mConnections.size()));
            return connInfo->GetReader();
        }             
        LOG_ERROR(sLogger, ("failed to create valid journal connection", "")("config", configName)("idx", idx));
        return nullptr;
       
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception creating journal connection", e.what())("config", configName)("idx", idx));
        return nullptr;
    }
}

std::unique_ptr<JournalConnectionGuard> JournalConnectionManager::GetGuardedConnection(
    const std::string& configName,
    size_t idx,
    const JournalConfig& config) {
    
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    auto it = mConnections.find(key);
    std::shared_ptr<JournalConnectionInstance> connInfo;
    
    if (it != mConnections.end()) {
        connInfo = it->second;
        
        // 检查连接状态：无效或待重置的连接都需要重新创建
        if (!connInfo->IsValid()) {
            LOG_WARNING(sLogger, ("existing connection invalid, creating new one", "")("config", configName)("idx", idx));
            mConnections.erase(it);
            connInfo = nullptr;
        } else if (connInfo->IsPendingReset()) {
            LOG_INFO(sLogger, ("connection pending reset, creating new one", "resetInterval reached")("config", configName)("idx", idx));
            mConnections.erase(it);
            connInfo = nullptr;
        }
    }
    
    // 如果连接不存在、已无效或待重置，创建新连接
    if (!connInfo) {
        try {
            connInfo = std::make_shared<JournalConnectionInstance>(configName, idx, config);
            
            if (connInfo->IsValid()) {
                mConnections[key] = connInfo;
                LOG_INFO(sLogger, ("new guarded journal connection created", "")("config", configName)("idx", idx)("total_connections", mConnections.size()));
            } else {
                LOG_ERROR(sLogger, ("failed to create valid journal connection for guarded access", "")("config", configName)("idx", idx));
                return nullptr;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger, ("exception creating journal connection for guarded access", e.what())("config", configName)("idx", idx));
            return nullptr;
        }
    }
    
    // 创建守护对象
    return std::make_unique<JournalConnectionGuard>(connInfo);
}

void JournalConnectionManager::RemoveConnection(const std::string& configName, size_t idx) {
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        mConnections.erase(it);
    }
}

size_t JournalConnectionManager::CleanupExpiredConnections(int resetIntervalSec) {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    size_t removedCount = 0;
    auto it = mConnections.begin();
    
    while (it != mConnections.end()) {
        auto& connInfo = it->second;
        
        // 检查连接是否过期或无效，但不清理正在使用中的连接
        if (!connInfo->IsValid()) {
            // 连接已无效，可以安全清理
            it = mConnections.erase(it);
            removedCount++;
        } else if (connInfo->ShouldReset(resetIntervalSec)) {
            // 连接需要重置
            if (!connInfo->IsInUse()) {
                // 连接未在使用中，可以清理
                it = mConnections.erase(it);
                removedCount++;
            } else {
                // 连接正在使用中，跳过清理
                ++it;
            }
        } else {
            ++it;
        }
    }
    
    if (removedCount > 0) {
        LOG_INFO(sLogger, ("cleanup completed", "")("removed_count", removedCount)("remaining_connections", mConnections.size()));
    }
    
    return removedCount;
}

size_t JournalConnectionManager::ResetExpiredConnections() {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    size_t resetCount = 0;
    auto it = mConnections.begin();
    
    while (it != mConnections.end()) {
        auto& connInfo = it->second;
        
        // 检查是否需要重置
        if (connInfo->ShouldReset(3600)) {
            if (connInfo->IsPendingReset()) {
                // 连接已标记为待重置，强制重置策略
                if (!connInfo->IsInUse()) {
                    // 连接空闲，立即重置
                    if (connInfo->ResetConnection()) {
                        LOG_INFO(sLogger, ("pending connection reset completed", "")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
                        resetCount++;
                    } else {
                        // 重置失败，移除连接，下次使用时会创建新连接
                        LOG_WARNING(sLogger, ("pending connection reset failed, removing", "")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
                        it = mConnections.erase(it);
                        resetCount++;
                        continue;
                    }
                } else {
                    // 连接仍在使用中，但已标记为待重置，移除它让其自然过期
                    // 新的请求会创建新连接，旧连接会在Guard析构后自动回收
                    LOG_INFO(sLogger, ("removing pending reset connection in use", "new connection will be created")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
                    it = mConnections.erase(it);
                    resetCount++;
                    continue;
                }
            } else {
                // 未标记但需要重置的连接（正常重置流程）
                if (!connInfo->IsInUse()) {
                    if (connInfo->ResetConnection()) {
                        LOG_INFO(sLogger, ("connection reset during periodic maintenance", "")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
                        resetCount++;
                    } else {
                        // 重置失败，移除连接
                        LOG_WARNING(sLogger, ("connection reset failed during maintenance, removing", "")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
                        it = mConnections.erase(it);
                        resetCount++;
                        continue;
                    }
                }
                // 正在使用的连接保持等待下次检查
            }
        }
        
        ++it;
    }
    
    if (resetCount > 0) {
        LOG_INFO(sLogger, ("periodic connection maintenance completed", "")("reset_count", resetCount)("remaining_connections", mConnections.size()));
    }
    
    return resetCount;
}

size_t JournalConnectionManager::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    return mConnections.size();
}

void JournalConnectionManager::Clear() {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    size_t count = mConnections.size();
    mConnections.clear();
    if (count > 0) {
        LOG_INFO(sLogger, ("journal connections cleared", "")("count", count));
    }
}

std::string JournalConnectionManager::makeConnectionKey(const std::string& configName, size_t idx) const {
    std::ostringstream oss;
    oss << configName << "_" << idx;
    return oss.str();
}

} // namespace logtail 