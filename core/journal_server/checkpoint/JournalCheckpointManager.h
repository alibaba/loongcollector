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

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>

namespace logtail {

/**
 * @brief Journal checkpoint数据结构
 * 
 * 包含journal读取进度的完整状态信息
 */
struct JournalCheckpoint {
    std::string cursor;                                    // journal cursor位置
    std::chrono::steady_clock::time_point createTime;     // checkpoint创建时间
    std::chrono::steady_clock::time_point updateTime;     // 最后更新时间
    bool changed = false;                                  // 是否有变更（用于批量持久化）
    
    JournalCheckpoint() 
        : createTime(std::chrono::steady_clock::now())
        , updateTime(std::chrono::steady_clock::now()) {}
        
    explicit JournalCheckpoint(const std::string& cursor_)
        : cursor(cursor_)
        , createTime(std::chrono::steady_clock::now()) 
        , updateTime(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Journal checkpoint管理器
 * 
 * 负责管理journal读取进度的checkpoint，提供：
 * - 内存中的checkpoint缓存
 * - 批量持久化到磁盘
 * - 自动过期清理
 * - 线程安全操作
 */
class JournalCheckpointManager {
public:
    static JournalCheckpointManager& GetInstance();
    
    ~JournalCheckpointManager() = default;
    
    // Delete copy and move operations (singleton pattern)
    JournalCheckpointManager(const JournalCheckpointManager&) = delete;
    JournalCheckpointManager& operator=(const JournalCheckpointManager&) = delete;
    JournalCheckpointManager(JournalCheckpointManager&&) = delete;
    JournalCheckpointManager& operator=(JournalCheckpointManager&&) = delete;
    
    /**
     * @brief 保存checkpoint
     * @param configName 配置名称
     * @param configIndex 配置索引
     * @param cursor journal cursor位置
     */
    void SaveCheckpoint(const std::string& configName, size_t configIndex, const std::string& cursor);
    
    /**
     * @brief 获取checkpoint  
     * @param configName 配置名称
     * @param configIndex 配置索引
     * @return checkpoint cursor，如果不存在返回空字符串
     */
    std::string GetCheckpoint(const std::string& configName, size_t configIndex) const;
    
    /**
     * @brief 清除checkpoint
     * @param configName 配置名称
     * @param configIndex 配置索引
     */
    void ClearCheckpoint(const std::string& configName, size_t configIndex);
    
    /**
     * @brief 检查是否存在checkpoint
     * @param configName 配置名称
     * @param configIndex 配置索引
     * @return 是否存在
     */
    bool HasCheckpoint(const std::string& configName, size_t configIndex) const;
    
    /**
     * @brief 批量保存所有变更的checkpoint到磁盘
     * @param forceAll 是否强制保存所有checkpoint（忽略changed标志）
     * @return 保存的checkpoint数量
     */
    size_t FlushAllCheckpoints(bool forceAll = false);
    
    /**
     * @brief 清理过期的checkpoint
     * @param maxAgeHours checkpoint最大保留时间（小时）
     * @return 清理的checkpoint数量
     */
    size_t CleanupExpiredCheckpoints(int maxAgeHours = 24);
    
    /**
     * @brief 清理指定配置的所有checkpoint
     * @param configName 配置名称
     * @return 清理的checkpoint数量
     */
    size_t ClearConfigCheckpoints(const std::string& configName);
    
    /**
     * @brief 获取当前缓存的checkpoint数量
     */
    size_t GetCheckpointCount() const;
    
    /**
     * @brief 获取变更的checkpoint数量（需要持久化）
     */
    size_t GetChangedCheckpointCount() const;

    /**
     * @brief 从磁盘加载checkpoint（初始化时调用）
     * @param configName 配置名称  
     * @param configIndex 配置索引
     * @return 是否成功加载
     */
    bool LoadCheckpointFromDisk(const std::string& configName, size_t configIndex);

private:
    JournalCheckpointManager() : mLastFlushTime(std::chrono::steady_clock::now()) {}
    
    // 生成checkpoint key
    std::string makeCheckpointKey(const std::string& configName, size_t configIndex) const;
    
    // 持久化单个checkpoint到磁盘  
    bool saveCheckpointToDisk(const std::string& key, const JournalCheckpoint& checkpoint);
    
    // 从磁盘加载单个checkpoint
    bool loadCheckpointFromDisk(const std::string& key, JournalCheckpoint& checkpoint);
    
    // 从磁盘删除checkpoint
    bool removeCheckpointFromDisk(const std::string& key);
    
    mutable std::mutex mMutex;
    std::unordered_map<std::string, std::shared_ptr<JournalCheckpoint>> mCheckpoints;
    
    // 持久化相关配置
    std::chrono::steady_clock::time_point mLastFlushTime;
};

} // namespace logtail 