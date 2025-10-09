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

#include "JournalServer.h"

#include <chrono>
#include <utility>

#include "connection/JournalConnectionManager.h"
#include "checkpoint/JournalCheckpointManager.h"
#include "common/Flags.h"
#include "logger/Logger.h"
#include "impl/JournalServerCore.h"

// Journal checkpoint 清理配置
DEFINE_FLAG_INT32(journal_checkpoint_cleanup_interval_sec, "cleanup interval for journal checkpoints in seconds, default 1 hour", 3600); 
DEFINE_FLAG_INT32(journal_checkpoint_expired_threshold_hours, "expired threshold for journal checkpoints in hours, default 24 hours", 24);

using namespace std;

namespace logtail {

// =============================================================================
// 1. 基础生命周期和接口 - Basic Lifecycle and Interface
// =============================================================================

void JournalServer::Init() {
    lock_guard<mutex> lock(mInitMux);
    if (mIsInitialized) {
        LOG_INFO(sLogger, ("JournalServer already initialized", "skipping duplicate Init() call"));
        return;
    }
    
    mThreadRes = async(launch::async, &JournalServer::run, this);
    mStartTime = time(nullptr);
    mIsInitialized = true;
    LOG_INFO(sLogger, ("JournalServer initialized", ""));
}

void JournalServer::Stop() {
    lock_guard<mutex> initLock(mInitMux);
    if (!mIsInitialized || !mThreadRes.valid()) {
        return;
    }
    mIsThreadRunning.store(false);
    if (mThreadRes.valid()) {
        mThreadRes.get();
    }
    mIsInitialized = false;
    LOG_INFO(sLogger, ("JournalServer stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mPipelineNameJournalConfigsMap.empty();
}

void JournalServer::AddJournalInput(const string& configName, size_t idx, const JournalConfig& config) {
    // 首先验证配置
    QueueKey queueKey = 0;
    if (!impl::ValidateJournalConfig(configName, idx, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal input validation failed", "config not added")("config", configName)("idx", idx));
        return;
    }
    
    // 尝试从磁盘加载现有检查点（用于配置更新场景）
    bool hasExistingCheckpoint = JournalCheckpointManager::GetInstance().LoadCheckpointFromDisk(configName, idx);
    if (hasExistingCheckpoint) {
        LOG_INFO(sLogger, ("existing checkpoint loaded for config update", "")("config", configName)("idx", idx));
    }
    
    // 验证成功后，缓存queueKey并添加配置
    JournalConfig validatedConfig = config;
    validatedConfig.queueKey = queueKey;
    
    {
        lock_guard<mutex> lock(mUpdateMux);
        mPipelineNameJournalConfigsMap[configName][idx] = validatedConfig;
        
        LOG_INFO(sLogger, ("journal input added after validation", "")("config", configName)("idx", idx)("ctx_valid", config.ctx != nullptr)("queue_key", queueKey)("total_pipelines", mPipelineNameJournalConfigsMap.size()));
    }
}

void JournalServer::RemoveJournalInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        // 如果配置存在，则移除
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            // 如果配置为空，则移除整个配置
            if (configItr->second.empty()) {
                // 移除整个配置
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
    }
    
    // 移除config对应的连接
    JournalConnectionManager::GetInstance()->RemoveConnection(configName, idx);
    
    // 清理configName对应的所有checkpoints
    size_t clearedCheckpoints = JournalCheckpointManager::GetInstance().ClearConfigCheckpoints(configName);
    if (clearedCheckpoints > 0) {
        LOG_INFO(sLogger, ("config checkpoints cleared", "")("config", configName)("count", clearedCheckpoints));
    }
    
    LOG_INFO(sLogger, ("journal input removed with automatic connection and checkpoint cleanup", "")("config", configName)("idx", idx));
}

void JournalServer::RemoveJournalInputWithoutCleanup(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto configItr = mPipelineNameJournalConfigsMap.find(configName);
        // 如果配置存在，则移除
        if (configItr != mPipelineNameJournalConfigsMap.end()) {
            configItr->second.erase(idx);
            // 如果配置为空，则移除整个配置
            if (configItr->second.empty()) {
                // 移除整个配置
                mPipelineNameJournalConfigsMap.erase(configItr);
            }
        }
    }
    
    // 移除config对应的连接
    JournalConnectionManager::GetInstance()->RemoveConnection(configName, idx);
    
    // 注意：不清理检查点，保留给配置更新后的新实例使用
    
    LOG_INFO(sLogger, ("journal input removed without checkpoint cleanup", "checkpoints preserved for config update")("config", configName)("idx", idx));
}

JournalConfig JournalServer::GetJournalConfig(const string& name, size_t idx) const {
    lock_guard<mutex> lock(mUpdateMux);
    auto configItr = mPipelineNameJournalConfigsMap.find(name);
    if (configItr != mPipelineNameJournalConfigsMap.end()) {
        // 如果配置存在，则获取idx对应的配置
        auto idxItr = configItr->second.find(idx);
        if (idxItr != configItr->second.end()) {
            return idxItr->second;
        }
    }
    // 如果配置不存在，则返回空配置
    return JournalConfig();
}

// =============================================================================
// 3. 连接池管理接口 - Connection Pool Management Interface
// =============================================================================

JournalServer::ConnectionPoolStats JournalServer::GetConnectionPoolStats() const {
    auto stats = JournalConnectionManager::GetInstance()->GetConnectionPoolStats();
    ConnectionPoolStats result;
    result.totalConnections = stats.totalConnections;
    result.activeConnections = stats.activeConnections;
    result.invalidConnections = stats.invalidConnections;
    result.connectionKeys = stats.connectionKeys;
    return result;
}

std::shared_ptr<JournalConnectionInstance> JournalServer::GetConnectionInfo(const std::string& configName, size_t idx) const {
    return JournalConnectionManager::GetInstance()->GetConnectionInfo(configName, idx);
}

bool JournalServer::ForceResetConnection(const std::string& configName, size_t idx) {
    return JournalConnectionManager::GetInstance()->ForceResetConnection(configName, idx);
}

size_t JournalServer::GetConnectionCount() const {
    return JournalConnectionManager::GetInstance()->GetConnectionCount();
}

// =============================================================================
// 2. 主线程运行逻辑 - Main Thread Logic
// =============================================================================

void JournalServer::run() {
    LOG_INFO(sLogger, ("JournalServer thread", "started"));
    
    while (true) {
        try {
            if (!mIsThreadRunning.load()) {
                break;
            }
            
            // 【核心处理逻辑】：遍历所有配置并处理journal条目
            impl::ProcessJournalEntries(this);
            
            // 每次循环后短暂休眠，避免CPU占用过高
            this_thread::sleep_for(chrono::milliseconds(100));
            
            // 定期清理无效连接（移除自动重置逻辑，连接永远不重建）
            static time_t sLastConnectionReset = time(nullptr);
            time_t currentTime = time(nullptr);
            if (currentTime - sLastConnectionReset >= 300) {
                size_t cleanedConnections = JournalConnectionManager::GetInstance()->ResetExpiredConnections();
                if (cleanedConnections > 0) {
                    LOG_INFO(sLogger, ("periodic invalid connection cleanup completed", "")("cleaned_count", cleanedConnections));
                }
                sLastConnectionReset = currentTime;
            }
            
            // 定期清理过期的checkpoints（使用配置的间隔时间）
            static time_t sLastCleanup = time(nullptr);
            currentTime = time(nullptr);
            if (currentTime - sLastCleanup >= INT32_FLAG(journal_checkpoint_cleanup_interval_sec)) {
                // 清理过期的checkpoints（使用配置的过期时间）
                LOG_INFO(sLogger, ("cleaning up expired journal checkpoints", "")
                         ("cleanup_interval_sec", INT32_FLAG(journal_checkpoint_cleanup_interval_sec))
                         ("expired_threshold_hours", INT32_FLAG(journal_checkpoint_expired_threshold_hours)));
                size_t cleanedCheckpoints = JournalCheckpointManager::GetInstance().CleanupExpiredCheckpoints(
                    INT32_FLAG(journal_checkpoint_expired_threshold_hours));
                if (cleanedCheckpoints > 0) {
                    LOG_INFO(sLogger, ("expired checkpoints cleaned", "")("count", cleanedCheckpoints));
                }
                sLastCleanup = currentTime;
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR(sLogger, ("exception in main journal server loop", e.what()));
            // 短暂暂停后继续运行，避免异常导致的忙循环
            this_thread::sleep_for(chrono::seconds(1));
        } catch (...) {
            LOG_ERROR(sLogger, ("unknown exception in main journal server loop", ""));
            // 短暂暂停后继续运行
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
    
    LOG_INFO(sLogger, ("JournalServer thread", "stopped"));
}

// =============================================================================
// 3. 测试和调试支持 - Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    lock_guard<mutex> lock(mUpdateMux);
    mPipelineNameJournalConfigsMap.clear();
    // Note: Checkpoint cleanup is handled by JournalCheckpointManager
}
#endif

} // namespace logtail 