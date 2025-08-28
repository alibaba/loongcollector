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
#include "../checkpoint/JournalCheckpointManager.h"
#include "logger/Logger.h"
#include <sstream>

namespace logtail {

//==============================================================================
// JournalConnectionInfo Implementation
//==============================================================================

JournalConnectionInfo::JournalConnectionInfo(const std::string& configName, 
                                           size_t idx, 
                                           const JournalConfig& config)
    : mConfigName(configName)
    , mIndex(idx)
    , mConfig(config)
    , mCreateTime(std::chrono::steady_clock::now())
    , mLastResetTime(std::chrono::steady_clock::now())
    , mIsValid(false) {
    
    // Connection created
    
    // 加载已保存的checkpoint  
    JournalCheckpointManager::GetInstance().LoadCheckpointFromDisk(mConfigName, mIndex);
    
    initializeConnection();
}

JournalConnectionInfo::~JournalConnectionInfo() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReader) {
        mReader->Close();
    }
    // Connection destroyed
}

std::shared_ptr<SystemdJournalReader> JournalConnectionInfo::GetReader() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 如果连接无效、reader为空，或者reader已关闭，尝试重新初始化
    if (!mIsValid || !mReader || !mReader->IsOpen()) {
        // Reinitializing connection due to invalid state
        initializeConnection();
    }
    
    return (mIsValid && mReader && mReader->IsOpen()) ? mReader : nullptr;
}

bool JournalConnectionInfo::ShouldReset(int resetIntervalSec) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mLastResetTime);
    return elapsed.count() >= resetIntervalSec;
}

bool JournalConnectionInfo::ResetConnection() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 检查连接是否正在使用中，如果是则跳过重置
    if (IsInUse()) {
        return false; // 返回false表示重置被推迟，但连接仍然有效
    }
    
    // 关闭旧连接
    if (mReader) {
        mReader->Close();
        mReader.reset();
    }
    
    // 重新初始化
    bool success = initializeConnection();
    mLastResetTime = std::chrono::steady_clock::now();
    
    if (success) {
        LOG_INFO(sLogger, ("journal connection reset successfully", "")("config", mConfigName)("idx", mIndex));
    } else {
        LOG_ERROR(sLogger, ("journal connection reset failed", "")("config", mConfigName)("idx", mIndex));
    }
    
    return success;
}

bool JournalConnectionInfo::IsValid() const {
    std::lock_guard<std::mutex> lock(mMutex);
    bool valid = mIsValid && mReader && mReader->IsOpen();
    // Connection validity check
    return valid;
}

bool JournalConnectionInfo::initializeConnection() {
    // 此方法在锁内调用，不需要再加锁
    mIsValid = false;
    
    try {
        mReader = std::make_shared<SystemdJournalReader>();
        
        // 设置超时
        mReader->SetTimeout(std::chrono::milliseconds(5000));
        
        // 设置自定义journal路径（如果指定）
        if (!mConfig.journalPaths.empty()) {
            mReader->SetJournalPaths(mConfig.journalPaths);
        }
        
        // 打开journal连接
        if (!mReader->Open()) {
            LOG_ERROR(sLogger, ("failed to open journal", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        
        if (!mReader->IsOpen()) {
            LOG_ERROR(sLogger, ("journal reader not open after Open() call", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        
        // 应用过滤器
        JournalFilter::FilterConfig filterConfig;
        filterConfig.units = mConfig.units;
        filterConfig.identifiers = mConfig.identifiers;
        filterConfig.matchPatterns = mConfig.matchPatterns;
        filterConfig.enableKernel = mConfig.kernel;
        filterConfig.configName = mConfigName;
        filterConfig.configIndex = mIndex;
        
        if (!JournalFilter::ApplyAllFilters(mReader.get(), filterConfig)) {
            LOG_ERROR(sLogger, ("failed to apply journal filters", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        
        mIsValid = true;
        LOG_INFO(sLogger, ("journal connection initialized successfully", "")("config", mConfigName)("idx", mIndex));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal connection initialization", e.what())("config", mConfigName)("idx", mIndex));
        mReader.reset();
        return false;
    }
}

//==============================================================================
// JournalConnectionManager Implementation  
//==============================================================================

std::shared_ptr<SystemdJournalReader> JournalConnectionManager::GetOrCreateConnection(
    const std::string& configName,
    size_t idx,
    const JournalConfig& config) {
    
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        // 连接存在，检查是否需要重置
        auto& connInfo = it->second;
        
        // 检查是否超过重置周期（默认1小时）
        if (connInfo->ShouldReset(3600)) {
            if (!connInfo->ResetConnection()) {
                LOG_WARNING(sLogger, ("failed to reset connection, removing", "")("config", configName)("idx", idx));
                mConnections.erase(it);
                // 继续下面的逻辑创建新连接
            } else {
                return connInfo->GetReader();
            }
        } else {
            // 连接未到重置时间，直接返回
            return connInfo->GetReader();
        }
    }
    
    // 创建新连接
    try {
        auto connInfo = std::make_shared<JournalConnectionInfo>(configName, idx, config);
        
        if (connInfo->IsValid()) {
            mConnections[key] = connInfo;
            LOG_INFO(sLogger, ("new journal connection created", "")("config", configName)("idx", idx)("total_connections", mConnections.size()));
            return connInfo->GetReader();
        } else {
            LOG_ERROR(sLogger, ("failed to create valid journal connection", "")("config", configName)("idx", idx));
            return nullptr;
        }
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
    std::shared_ptr<JournalConnectionInfo> connInfo;
    
    if (it != mConnections.end()) {
        connInfo = it->second;
        
        // 检查是否需要重置，但不强制重置正在使用的连接
        if (connInfo->ShouldReset(3600) && !connInfo->IsInUse()) {
            if (!connInfo->ResetConnection()) {
                LOG_WARNING(sLogger, ("failed to reset connection, creating new one", "")("config", configName)("idx", idx));
                mConnections.erase(it);
                connInfo = nullptr;
            }
        }
    }
    
    // 如果连接不存在或重置失败，创建新连接
    if (!connInfo) {
        try {
            connInfo = std::make_shared<JournalConnectionInfo>(configName, idx, config);
            
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

//==============================================================================
// JournalConnectionInfo Checkpoint Implementation
//==============================================================================

void JournalConnectionInfo::SaveCheckpoint(const std::string& cursor) {
    JournalCheckpointManager::GetInstance().SaveCheckpoint(mConfigName, mIndex, cursor);
}

std::string JournalConnectionInfo::GetCheckpoint() const {
    return JournalCheckpointManager::GetInstance().GetCheckpoint(mConfigName, mIndex);
}

void JournalConnectionInfo::ClearCheckpoint() {
    JournalCheckpointManager::GetInstance().ClearCheckpoint(mConfigName, mIndex);
}

bool JournalConnectionInfo::HasCheckpoint() const {
    return JournalCheckpointManager::GetInstance().HasCheckpoint(mConfigName, mIndex);
}

void JournalConnectionInfo::IncrementUsageCount() {
    mUsageCount.fetch_add(1);
}

void JournalConnectionInfo::DecrementUsageCount() {
    mUsageCount.fetch_sub(1);
}

bool JournalConnectionInfo::IsInUse() const {
    return mUsageCount.load() > 0;
}

// Checkpoint处理已迁移到JournalCheckpointManager

//==============================================================================
// JournalConnectionManager Checkpoint Implementation
//==============================================================================

void JournalConnectionManager::SaveCheckpoint(const std::string& configName, size_t idx, const std::string& cursor) {
    // 直接委托给JournalCheckpointManager，不需要检查连接是否存在
    JournalCheckpointManager::GetInstance().SaveCheckpoint(configName, idx, cursor);
}

std::string JournalConnectionManager::GetCheckpoint(const std::string& configName, size_t idx) const {
    return JournalCheckpointManager::GetInstance().GetCheckpoint(configName, idx);
}

void JournalConnectionManager::ClearCheckpoint(const std::string& configName, size_t idx) {
    JournalCheckpointManager::GetInstance().ClearCheckpoint(configName, idx);
}

size_t JournalConnectionManager::FlushAllCheckpoints(bool forceAll) {
    return JournalCheckpointManager::GetInstance().FlushAllCheckpoints(forceAll);
}

} // namespace logtail 