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

#include "JournalConfigGroupManager.h"
#include "../reader/JournalReader.h"
#include "../reader/JournalFilter.h"
#include "../checkpoint/JournalCheckpointManager.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

//==============================================================================
// JournalConfigGroupManager 实现
//==============================================================================

JournalConfigGroupManager& JournalConfigGroupManager::GetInstance() {
    static JournalConfigGroupManager sInstance;
    return sInstance;
}

bool JournalConfigGroupManager::Initialize() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (mInitialized) {
        return true;
    }
    
    LOG_INFO(sLogger, ("initializing journal config group manager", ""));
    mInitialized = true;
    
    return true;
}

void JournalConfigGroupManager::Cleanup() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        return;
    }
    
    LOG_INFO(sLogger, ("cleaning up journal config group manager", "")("groups", mGroups.size()));
    
    // 清理所有分组
    for (auto& [filterHash, groupInfo] : mGroups) {
        if (groupInfo->sharedReader) {
            groupInfo->sharedReader->Close();
        }
    }
    
    mGroups.clear();
    mInitialized = false;
}

bool JournalConfigGroupManager::AddConfig(const std::string& configName, size_t idx, const JournalConfig& config, const ConfigHandler& handler) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        LOG_ERROR(sLogger, ("group manager not initialized", ""));
        return false;
    }
    
    // 计算filter hash
    std::string filterHash = calculateFilterHash(config);
    std::string configKey = configName + ":" + std::to_string(idx);
    
    LOG_INFO(sLogger, ("adding config to group", "")("config", configName)("idx", idx)("filterHash", filterHash));
    
    // 查找或创建分组
    auto groupIt = mGroups.find(filterHash);
    if (groupIt == mGroups.end()) {
        // 创建新分组
        auto groupInfo = std::make_shared<GroupInfo>();
        groupInfo->filterHash = filterHash;
        
        // 创建共享的journal reader
        groupInfo->sharedReader = std::make_shared<SystemdJournalReader>();
        if (!groupInfo->sharedReader->Open()) {
            LOG_ERROR(sLogger, ("failed to create shared journal reader", "")("filterHash", filterHash));
            return false;
        }
        
        // 应用过滤器
        JournalFilter::FilterConfig filterConfig;
        filterConfig.units = config.units;
        filterConfig.identifiers = config.identifiers;
        filterConfig.matchPatterns = config.matchPatterns;
        filterConfig.enableKernel = config.kernel;
        filterConfig.configName = configName;
        filterConfig.configIndex = idx;
        
        if (!JournalFilter::ApplyAllFilters(groupInfo->sharedReader.get(), filterConfig)) {
            LOG_ERROR(sLogger, ("failed to apply filters to shared reader", "")("filterHash", filterHash));
            groupInfo->sharedReader->Close();
            return false;
        }
        
        // 设置seek位置
        if (!config.seekPosition.empty()) {
            bool seekSuccess = false;
            if (config.seekPosition == "tail") {
                seekSuccess = groupInfo->sharedReader->SeekTail();
            } else if (config.seekPosition == "head") {
                seekSuccess = groupInfo->sharedReader->SeekHead();
            } else if (config.seekPosition == "cursor") {
                // 对于cursor，需要从checkpoint获取实际的cursor值
                std::string checkpointCursor = JournalCheckpointManager::GetInstance().GetCheckpoint(configName, idx);
                if (!checkpointCursor.empty()) {
                    seekSuccess = groupInfo->sharedReader->SeekCursor(checkpointCursor);
                } else {
                    // 没有checkpoint，fallback到head
                    seekSuccess = groupInfo->sharedReader->SeekHead();
                }
            }
            
            if (!seekSuccess) {
                LOG_WARNING(sLogger, ("failed to seek to position", "")("position", config.seekPosition)("filterHash", filterHash));
            } else {
                LOG_INFO(sLogger, ("successfully seeked to position", "")("position", config.seekPosition)("filterHash", filterHash));
            }
        }
        
        mGroups[filterHash] = groupInfo;
        LOG_INFO(sLogger, ("created new group", "")("filterHash", filterHash)("totalGroups", mGroups.size()));
    }
    
    // 添加配置到分组
    auto& groupInfo = mGroups[filterHash];
    ConfigInfo configInfo;
    configInfo.configName = configName;
    configInfo.idx = idx;
    configInfo.config = config;
    configInfo.handler = handler;
    
    groupInfo->configs[configKey] = configInfo;
    
    LOG_INFO(sLogger, ("config added to group", "")("config", configName)("idx", idx)("filterHash", filterHash)("groupSize", groupInfo->configs.size()));
    
    return true;
}

void JournalConfigGroupManager::RemoveConfig(const std::string& configName, size_t idx) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        return;
    }
    
    std::string configKey = configName + ":" + std::to_string(idx);
    
    LOG_INFO(sLogger, ("removing config from groups", "")("config", configName)("idx", idx));
    
    // 遍历所有分组查找配置
    for (auto groupIt = mGroups.begin(); groupIt != mGroups.end();) {
        auto& groupInfo = groupIt->second;
        auto configIt = groupInfo->configs.find(configKey);
        
        if (configIt != groupInfo->configs.end()) {
            groupInfo->configs.erase(configIt);
            LOG_INFO(sLogger, ("config removed from group", "")("config", configName)("idx", idx)("filterHash", groupInfo->filterHash)("remainingConfigs", groupInfo->configs.size()));
            
            // 如果分组中没有配置了，清理分组
            if (groupInfo->configs.empty()) {
                LOG_INFO(sLogger, ("group is empty, cleaning up", "")("filterHash", groupInfo->filterHash));
                if (groupInfo->sharedReader) {
                    groupInfo->sharedReader->Close();
                }
                groupIt = mGroups.erase(groupIt);
                break;
            }
            ++groupIt;
            break;
        }
        ++groupIt;
    }
}

JournalConfigGroupManager::Stats JournalConfigGroupManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    Stats stats;
    stats.totalGroups = mGroups.size();
    stats.totalConfigs = 0;
    stats.sharedInotifyInstances = 0;
    stats.totalConnections = 0;
    stats.activeConnections = 0;
    stats.invalidConnections = 0;
    
    for (const auto& [filterHash, groupInfo] : mGroups) {
        stats.totalConfigs += groupInfo->configs.size();
        stats.totalConnections += groupInfo->configs.size();
        
        if (groupInfo->sharedReader && groupInfo->sharedReader->IsOpen()) {
            stats.sharedInotifyInstances++;
            stats.activeConnections += groupInfo->configs.size();
        } else {
            stats.invalidConnections += groupInfo->configs.size();
        }
        
        stats.groupKeys.push_back(filterHash);
        
        // 添加连接键
        for (const auto& [configKey, configInfo] : groupInfo->configs) {
            stats.connectionKeys.push_back(configKey);
        }
    }
    
    return stats;
}

std::string JournalConfigGroupManager::calculateFilterHash(const JournalConfig& config) const {
    // 构建hash字符串，包含所有影响过滤的配置
    std::string hashStr;
    
    // 添加units
    for (const auto& unit : config.units) {
        hashStr += "unit:" + unit + ";";
    }
    
    // 添加identifiers
    for (const auto& identifier : config.identifiers) {
        hashStr += "id:" + identifier + ";";
    }
    
    // 添加matchPatterns
    for (const auto& pattern : config.matchPatterns) {
        hashStr += "pattern:" + pattern + ";";
    }
    
    // 添加kernel设置
    hashStr += "kernel:" + std::to_string(config.kernel) + ";";
    
    // 添加journal路径
    for (const auto& path : config.journalPaths) {
        hashStr += "path:" + path + ";";
    }
    
    // 计算hash值
    std::hash<std::string> hasher;
    size_t hashValue = hasher(hashStr);
    
    return std::to_string(hashValue);
}

//==============================================================================
// 新增功能实现
//==============================================================================

std::shared_ptr<SystemdJournalReader> JournalConfigGroupManager::GetConnectionInfo(const std::string& configName, size_t idx) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 遍历所有分组查找指定配置
    for (const auto& [groupKey, groupInfo] : mGroups) {
        auto configKey = configName + ":" + std::to_string(idx);
        auto it = groupInfo->configs.find(configKey);
        if (it != groupInfo->configs.end()) {
            return groupInfo->sharedReader;
        }
    }
    
    return nullptr;
}

std::vector<std::pair<std::string, size_t>> JournalConfigGroupManager::GetConfigsUsingReader(const std::shared_ptr<SystemdJournalReader>& reader) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::pair<std::string, size_t>> configs;
    
    // 遍历所有分组查找使用指定reader的配置
    for (const auto& [groupKey, groupInfo] : mGroups) {
        if (groupInfo->sharedReader == reader) {
            // 找到使用这个reader的分组，添加所有配置
            for (const auto& [configKey, configInfo] : groupInfo->configs) {
                configs.emplace_back(configInfo.configName, configInfo.idx);
            }
        }
    }
    
    return configs;
}

bool JournalConfigGroupManager::ForceResetConnection(const std::string& configName, size_t idx) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 查找配置所在的分组
    for (auto& [groupKey, groupInfo] : mGroups) {
        auto configKey = configName + "_" + std::to_string(idx);
        auto it = groupInfo->configs.find(configKey);
        if (it != groupInfo->configs.end()) {
            // 找到配置，移除它
            groupInfo->configs.erase(it);
            LOG_INFO(sLogger, ("connection force reset", "")("config", configName)("idx", idx)("group", groupKey));
            
            // 如果分组为空，清理整个分组
            if (groupInfo->configs.empty()) {
                if (groupInfo->sharedReader) {
                    groupInfo->sharedReader->Close();
                }
                if (groupInfo->epollFD >= 0) {
                    close(groupInfo->epollFD);
                }
                mGroups.erase(groupKey);
                LOG_INFO(sLogger, ("empty group cleaned up", "")("group", groupKey));
            }
            
            return true;
        }
    }
    
    return false;
}

size_t JournalConfigGroupManager::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    size_t totalConnections = 0;
    for (const auto& [groupKey, groupInfo] : mGroups) {
        totalConnections += groupInfo->configs.size();
    }
    
    return totalConnections;
}

} // namespace logtail