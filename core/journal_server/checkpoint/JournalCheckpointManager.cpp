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

#include "JournalCheckpointManager.h"
#include "logger/Logger.h"
#include <sstream>
#include <algorithm>
#include <chrono>

namespace logtail {

JournalCheckpointManager& JournalCheckpointManager::GetInstance() {
    static JournalCheckpointManager instance;
    return instance;
}

void JournalCheckpointManager::SaveCheckpoint(const std::string& configName, size_t configIndex, const std::string& cursor) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        // 更新existing checkpoint
        auto& checkpoint = it->second;
        if (checkpoint->cursor != cursor) {
            checkpoint->cursor = cursor;
            checkpoint->updateTime = std::chrono::steady_clock::now();
            checkpoint->changed = true;
        }
    } else {
        // 创建新checkpoint
        auto checkpoint = std::make_shared<JournalCheckpoint>(cursor);
        checkpoint->changed = true;
        mCheckpoints[key] = checkpoint;
    }
}

std::string JournalCheckpointManager::GetCheckpoint(const std::string& configName, size_t configIndex) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        return it->second->cursor;
    }
    
    return "";  // 如果不存在返回空字符串
}

void JournalCheckpointManager::ClearCheckpoint(const std::string& configName, size_t configIndex) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    auto it = mCheckpoints.find(key);
    
    if (it != mCheckpoints.end()) {
        // 从磁盘删除
        removeCheckpointFromDisk(key);
        // 从内存删除
        mCheckpoints.erase(it);
    }
}

bool JournalCheckpointManager::HasCheckpoint(const std::string& configName, size_t configIndex) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    return mCheckpoints.find(key) != mCheckpoints.end();
}

size_t JournalCheckpointManager::FlushAllCheckpoints(bool forceAll) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    size_t flushedCount = 0;
    
    for (auto& pair : mCheckpoints) {
        const std::string& key = pair.first;
        auto& checkpoint = pair.second;
        
        if (forceAll || checkpoint->changed) {
            if (saveCheckpointToDisk(key, *checkpoint)) {
                checkpoint->changed = false;
                flushedCount++;
            }
        }
    }
    
    mLastFlushTime = std::chrono::steady_clock::now();
    
    if (flushedCount > 0) {
        LOG_INFO(sLogger, ("journal checkpoints flushed to disk", "")("count", flushedCount)("force_all", forceAll));
    }
    
    return flushedCount;
}

size_t JournalCheckpointManager::CleanupExpiredCheckpoints(int maxAgeHours) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto maxAge = std::chrono::hours(maxAgeHours);
    
    size_t cleanedCount = 0;
    auto it = mCheckpoints.begin();
    
    while (it != mCheckpoints.end()) {
        const std::string& key = it->first;
        const auto& checkpoint = it->second;
        
        if (now - checkpoint->updateTime > maxAge) {
            // 从磁盘删除
            removeCheckpointFromDisk(key);
            // 从内存删除
            it = mCheckpoints.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        LOG_INFO(sLogger, ("journal checkpoints cleanup completed", "")("cleaned_count", cleanedCount)("max_age_hours", maxAgeHours));
    }
    
    return cleanedCount;
}

size_t JournalCheckpointManager::ClearConfigCheckpoints(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    size_t cleanedCount = 0;
    auto it = mCheckpoints.begin();
    
    std::string configPrefix = configName + "_";
    
    while (it != mCheckpoints.end()) {
        const std::string& key = it->first;
        
        if (key.substr(0, configPrefix.length()) == configPrefix) {
            // 从磁盘删除
            removeCheckpointFromDisk(key);
            // 从内存删除
            it = mCheckpoints.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }
    
    if (cleanedCount > 0) {
        LOG_INFO(sLogger, ("journal config checkpoints cleared", "")("config", configName)("cleared_count", cleanedCount));
    }
    
    return cleanedCount;
}

size_t JournalCheckpointManager::GetCheckpointCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCheckpoints.size();
}

size_t JournalCheckpointManager::GetChangedCheckpointCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    return std::count_if(mCheckpoints.begin(), mCheckpoints.end(),
                        [](const auto& pair) { return pair.second->changed; });
}

bool JournalCheckpointManager::LoadCheckpointFromDisk(const std::string& configName, size_t configIndex) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string key = makeCheckpointKey(configName, configIndex);
    
    // 如果内存中已存在，跳过加载
    if (mCheckpoints.find(key) != mCheckpoints.end()) {
        return true;
    }
    
    JournalCheckpoint checkpoint;
    if (loadCheckpointFromDisk(key, checkpoint)) {
        mCheckpoints[key] = std::make_shared<JournalCheckpoint>(std::move(checkpoint));
        return true;
    }
    
    return false;
}

std::string JournalCheckpointManager::makeCheckpointKey(const std::string& configName, size_t configIndex) const {
    std::ostringstream oss;
    oss << configName << "_" << configIndex;
    return oss.str();
}

bool JournalCheckpointManager::saveCheckpointToDisk(const std::string& key, const JournalCheckpoint& checkpoint) {
    (void)key;        // TODO: 使用key作为存储标识
    (void)checkpoint; // TODO: 序列化checkpoint数据
    
    // TODO: 与现有的checkpoint持久化机制集成
    // 可以使用类似file_server/checkpoint/CheckPointManager的方式
    // 或者集成Go版本的leveldb-based checkpoint manager
    
    // 目前先返回true，表示"保存成功"
    // 实际实现时需要：
    // 1. 序列化checkpoint数据（JSON格式）
    // 2. 写入到持久化存储（文件或数据库）
    // 3. 处理错误情况
    
    return true;
}

bool JournalCheckpointManager::loadCheckpointFromDisk(const std::string& key, JournalCheckpoint& checkpoint) {
    (void)key;        // TODO: 使用key从存储中读取
    (void)checkpoint; // TODO: 反序列化到checkpoint对象
    
    // TODO: 从磁盘加载checkpoint数据
    // 对应saveCheckpointToDisk的反向操作：
    // 1. 从持久化存储读取数据
    // 2. 反序列化为JournalCheckpoint对象
    // 3. 处理文件不存在或损坏的情况
    
    return false;  // 暂时返回false，表示没有找到
}

bool JournalCheckpointManager::removeCheckpointFromDisk(const std::string& key) {
    (void)key; // TODO: 使用key删除对应的存储记录
    
    // TODO: 从磁盘删除checkpoint数据
    // 删除对应的持久化文件或数据库记录
    
    return true;  // 暂时返回true
}

} // namespace logtail 