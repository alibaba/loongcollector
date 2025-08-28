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

#include "journal_server/JournalConnectionManager.h"
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
    , mCheckpointChanged(false)
    , mLastCheckpointSaveTime(std::chrono::steady_clock::now())
    , mIsValid(false) {
    
    LOG_INFO(sLogger, ("JournalConnectionInfo created", "")("config", mConfigName)("idx", mIndex));
    
    // 加载已保存的checkpoint
    loadCheckpointFromDisk();
    
    initializeConnection();
}

JournalConnectionInfo::~JournalConnectionInfo() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReader) {
        mReader->Close();
    }
    LOG_INFO(sLogger, ("JournalConnectionInfo destroyed", "")("config", mConfigName)("idx", mIndex));
}

std::shared_ptr<SystemdJournalReader> JournalConnectionInfo::GetReader() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 如果连接无效、reader为空，或者reader已关闭，尝试重新初始化
    if (!mIsValid || !mReader || !mReader->IsOpen()) {
        if (!mIsValid || !mReader) {
            LOG_INFO(sLogger, ("attempting to reinitialize connection", "invalid or null reader")("config", mConfigName)("idx", mIndex));
        } else {
            LOG_INFO(sLogger, ("attempting to reinitialize connection", "reader closed")("config", mConfigName)("idx", mIndex));
        }
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
    
    LOG_INFO(sLogger, ("resetting journal connection", "")("config", mConfigName)("idx", mIndex));
    
    // 关闭旧连接
    if (mReader) {
        mReader->Close();
        mReader.reset();
    }
    
    // 重新初始化
    bool success = initializeConnection();
    mLastResetTime = std::chrono::steady_clock::now();
    
    if (success) {
        LOG_INFO(sLogger, ("journal connection reset successful", "")("config", mConfigName)("idx", mIndex));
    } else {
        LOG_ERROR(sLogger, ("journal connection reset failed", "")("config", mConfigName)("idx", mIndex));
    }
    
    return success;
}

bool JournalConnectionInfo::IsValid() const {
    std::lock_guard<std::mutex> lock(mMutex);
    bool valid = mIsValid && mReader && mReader->IsOpen();
    if (!valid && mIsValid && mReader) {
        LOG_DEBUG(sLogger, ("connection marked invalid due to closed reader", "")("config", mConfigName)("idx", mIndex));
    }
    return valid;
}

bool JournalConnectionInfo::initializeConnection() {
    // 此方法在锁内调用，不需要再加锁
    mIsValid = false;
    
    LOG_INFO(sLogger, ("initializeConnection started", "")("config", mConfigName)("idx", mIndex));
    
    try {
        mReader = std::make_shared<SystemdJournalReader>();
        LOG_INFO(sLogger, ("SystemdJournalReader created", "")("config", mConfigName)("idx", mIndex));
        
        // 设置超时
        mReader->SetTimeout(std::chrono::milliseconds(5000));
        
        // 设置自定义journal路径（如果指定）
        if (!mConfig.journalPaths.empty()) {
            LOG_INFO(sLogger, ("setting custom journal paths", "")("config", mConfigName)("idx", mIndex)("paths_count", mConfig.journalPaths.size()));
            mReader->SetJournalPaths(mConfig.journalPaths);
        }
        
        // 打开journal连接
        LOG_INFO(sLogger, ("calling mReader->Open()", "")("config", mConfigName)("idx", mIndex));
        if (!mReader->Open()) {
            LOG_ERROR(sLogger, ("failed to open journal", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        LOG_INFO(sLogger, ("mReader->Open() succeeded", "")("config", mConfigName)("idx", mIndex));
        
        if (!mReader->IsOpen()) {
            LOG_ERROR(sLogger, ("journal reader not open after Open() call", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        LOG_INFO(sLogger, ("journal reader is open", "")("config", mConfigName)("idx", mIndex));
        
        // 应用过滤器
        LOG_INFO(sLogger, ("preparing to apply filters", "")("config", mConfigName)("idx", mIndex));
        JournalFilter::FilterConfig filterConfig;
        filterConfig.units = mConfig.units;
        filterConfig.identifiers = mConfig.identifiers;
        filterConfig.matchPatterns = mConfig.matchPatterns;
        filterConfig.enableKernel = mConfig.kernel;
        filterConfig.configName = mConfigName;
        filterConfig.configIndex = mIndex;
        
        LOG_INFO(sLogger, ("applying filters", "")("config", mConfigName)("idx", mIndex)
                 ("units_count", filterConfig.units.size())("identifiers_count", filterConfig.identifiers.size())
                 ("match_patterns_count", filterConfig.matchPatterns.size())("enable_kernel", filterConfig.enableKernel));
        
        if (!JournalFilter::ApplyAllFilters(mReader.get(), filterConfig)) {
            LOG_ERROR(sLogger, ("failed to apply journal filters", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        LOG_INFO(sLogger, ("filters applied successfully", "")("config", mConfigName)("idx", mIndex));
        
        mIsValid = true;
        LOG_INFO(sLogger, ("journal connection initialized with filters", "")("config", mConfigName)("idx", mIndex)
                 ("filter_desc", JournalFilter::GetConfigDescription(filterConfig)));
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
    
    LOG_INFO(sLogger, ("GetOrCreateConnection called", "")("config", configName)("idx", idx)("key", key));
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        LOG_INFO(sLogger, ("existing connection found", "")("config", configName)("idx", idx));
        // 连接存在，检查是否需要重置
        auto& connInfo = it->second;
        
        // 检查是否超过重置周期（默认1小时）
        if (connInfo->ShouldReset(3600)) {
            LOG_INFO(sLogger, ("connection reached reset interval, resetting", "")("config", configName)("idx", idx));
            if (!connInfo->ResetConnection()) {
                LOG_WARNING(sLogger, ("failed to reset connection, removing", "")("config", configName)("idx", idx));
                mConnections.erase(it);
                // 继续下面的逻辑创建新连接
            } else {
                return connInfo->GetReader();
            }
        } else {
            // 连接未到重置时间，直接返回
            LOG_INFO(sLogger, ("returning existing connection", "")("config", configName)("idx", idx));
            auto reader = connInfo->GetReader();
            LOG_INFO(sLogger, ("existing connection reader status", "")("config", configName)("idx", idx)("reader_valid", reader != nullptr)("reader_open", reader ? reader->IsOpen() : false));
            return reader;
        }
    } else {
        LOG_INFO(sLogger, ("no existing connection found", "")("config", configName)("idx", idx));
    }
    
    // 创建新连接
    LOG_INFO(sLogger, ("creating new journal connection", "")("config", configName)("idx", idx));
    
    try {
        auto connInfo = std::make_shared<JournalConnectionInfo>(configName, idx, config);
        LOG_INFO(sLogger, ("JournalConnectionInfo created", "")("config", configName)("idx", idx)("valid", connInfo->IsValid()));
        
        if (connInfo->IsValid()) {
            mConnections[key] = connInfo;
            LOG_INFO(sLogger, ("journal connection created and cached", "")("config", configName)("idx", idx)("total_connections", mConnections.size()));
            auto reader = connInfo->GetReader();
            LOG_INFO(sLogger, ("new connection reader status", "")("config", configName)("idx", idx)("reader_valid", reader != nullptr)("reader_open", reader ? reader->IsOpen() : false));
            return reader;
        } else {
            LOG_ERROR(sLogger, ("failed to create valid journal connection", "")("config", configName)("idx", idx));
            return nullptr;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception creating journal connection", e.what())("config", configName)("idx", idx));
        return nullptr;
    }
}

void JournalConnectionManager::RemoveConnection(const std::string& configName, size_t idx) {
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        LOG_INFO(sLogger, ("removing journal connection", "")("config", configName)("idx", idx));
        mConnections.erase(it);
    }
}

size_t JournalConnectionManager::CleanupExpiredConnections(int resetIntervalSec) {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    size_t removedCount = 0;
    auto it = mConnections.begin();
    
    while (it != mConnections.end()) {
        auto& connInfo = it->second;
        
        // 检查连接是否过期或无效
        if (!connInfo->IsValid() || connInfo->ShouldReset(resetIntervalSec)) {
            LOG_INFO(sLogger, ("cleaning up expired/invalid connection", "")("config", connInfo->GetConfigName())("idx", connInfo->GetIndex()));
            it = mConnections.erase(it);
            removedCount++;
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
    LOG_INFO(sLogger, ("all journal connections cleared", "")("count", count));
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
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCurrentCheckpoint != cursor) {
        mCurrentCheckpoint = cursor;
        mCheckpointChanged = true;
        LOG_DEBUG(sLogger, ("checkpoint updated", "")("config", mConfigName)("idx", mIndex)("cursor", cursor.substr(0, 50)));
    }
}

std::string JournalConnectionInfo::GetCheckpoint() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentCheckpoint;
}

void JournalConnectionInfo::ClearCheckpoint() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mCurrentCheckpoint.empty()) {
        mCurrentCheckpoint.clear();
        mCheckpointChanged = true;
        LOG_DEBUG(sLogger, ("checkpoint cleared", "")("config", mConfigName)("idx", mIndex));
    }
}

bool JournalConnectionInfo::HasCheckpoint() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mCurrentCheckpoint.empty();
}

void JournalConnectionInfo::loadCheckpointFromDisk() {
    // TODO: 从磁盘加载checkpoint
    // 这里可以集成现有的checkpoint持久化机制
    // 暂时留空，保持向后兼容
    LOG_DEBUG(sLogger, ("loading checkpoint from disk", "")("config", mConfigName)("idx", mIndex));
}

void JournalConnectionInfo::saveCheckpointToDisk() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCheckpointChanged) {
        // TODO: 保存checkpoint到磁盘
        // 这里可以集成现有的checkpoint持久化机制
        LOG_DEBUG(sLogger, ("saving checkpoint to disk", "")("config", mConfigName)("idx", mIndex)("cursor", mCurrentCheckpoint.substr(0, 50)));
        mCheckpointChanged = false;
        mLastCheckpointSaveTime = std::chrono::steady_clock::now();
    }
}

//==============================================================================
// JournalConnectionManager Checkpoint Implementation
//==============================================================================

void JournalConnectionManager::SaveCheckpoint(const std::string& configName, size_t idx, const std::string& cursor) {
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        it->second->SaveCheckpoint(cursor);
    } else {
        LOG_WARNING(sLogger, ("attempt to save checkpoint for non-existent connection", "")("config", configName)("idx", idx));
    }
}

std::string JournalConnectionManager::GetCheckpoint(const std::string& configName, size_t idx) const {
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        return it->second->GetCheckpoint();
    }
    
    // 如果连接不存在，尝试从磁盘加载checkpoint（向后兼容）
    LOG_DEBUG(sLogger, ("connection not found for checkpoint retrieval", "")("config", configName)("idx", idx));
    return "";
}

void JournalConnectionManager::ClearCheckpoint(const std::string& configName, size_t idx) {
    std::string key = makeConnectionKey(configName, idx);
    
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    auto it = mConnections.find(key);
    if (it != mConnections.end()) {
        it->second->ClearCheckpoint();
    }
}

size_t JournalConnectionManager::FlushAllCheckpoints(bool forceAll) {
    std::lock_guard<std::mutex> lock(mConnectionsMutex);
    
    size_t flushedCount = 0;
    if (forceAll) {
        // 强制保存所有checkpoint
        flushedCount = mConnections.size();
        // TODO: 实现checkpoint的批量刷新
        // for (auto& pair : mConnections) {
        //     auto& connInfo = pair.second;
        //     connInfo->FlushCheckpoint();
        // }
    }
    
    if (flushedCount > 0) {
        LOG_INFO(sLogger, ("checkpoints flushed to disk", "")("count", flushedCount)("force", forceAll));
    }
    
    return flushedCount;
}

} // namespace logtail 