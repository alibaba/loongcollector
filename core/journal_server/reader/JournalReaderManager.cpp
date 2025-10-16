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

#include "JournalReaderManager.h"
#include "JournalReader.h"
#include "JournalFilter.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

//==============================================================================
// JournalReaderManager 实现
//==============================================================================

JournalReaderManager& JournalReaderManager::GetInstance() {
    static JournalReaderManager sInstance;
    return sInstance;
}

bool JournalReaderManager::Initialize() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (mInitialized) {
        return true;
    }
    
    LOG_INFO(sLogger, ("initializing journal config manager", ""));
    mInitialized = true;
    
    return true;
}

void JournalReaderManager::Cleanup() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        return;
    }
    
    LOG_INFO(sLogger, ("cleaning up journal config manager", "")("total_configs", mConfigs.size()));
    
    // 清理所有配置和reader
    for (auto& [key, configInfo] : mConfigs) {
        if (configInfo.reader) {
            configInfo.reader->Close();
        }
    }
    
    mConfigs.clear();
    mInitialized = false;
}

std::string JournalReaderManager::makeConfigKey(const std::string& configName, size_t idx) const {
    return configName + ":" + std::to_string(idx);
}

bool JournalReaderManager::AddConfig(const std::string& configName, size_t idx, const JournalConfig& config, const ConfigHandler& handler) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        LOG_ERROR(sLogger, ("config manager not initialized", ""));
        return false;
    }
    
    std::string configKey = makeConfigKey(configName, idx);
    
    // 检查配置是否已存在
    if (mConfigs.find(configKey) != mConfigs.end()) {
        LOG_WARNING(sLogger, ("config already exists, will be replaced", "")("config", configName)("idx", idx));
        // 关闭旧的reader
        if (mConfigs[configKey].reader) {
            mConfigs[configKey].reader->Close();
        }
    }
    
    LOG_INFO(sLogger, ("adding config with independent reader", "")("config", configName)("idx", idx));
    
    // 创建独立的journal reader
    auto reader = std::make_shared<SystemdJournalReader>();
    if (!reader->Open()) {
        LOG_ERROR(sLogger, ("failed to create journal reader", "")("config", configName)("idx", idx));
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
    
    if (!JournalFilter::ApplyAllFilters(reader.get(), filterConfig)) {
        LOG_ERROR(sLogger, ("failed to apply filters to reader", "")("config", configName)("idx", idx));
        reader->Close();
        return false;
    }
    
    // 设置seek位置
    if (!config.seekPosition.empty()) {
        bool seekSuccess = false;
        if (config.seekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger, ("seek to tail", "")("config", configName)("idx", idx)("success", seekSuccess));
            
            // SeekTail()后需要调用Previous()才能读取到实际的日志条目
            if (seekSuccess) {
                reader->Previous();
            }
        } else {
            seekSuccess = reader->SeekHead();
            LOG_INFO(sLogger, ("seek to head", "")("config", configName)("idx", idx)("success", seekSuccess));
        }
        
        if (!seekSuccess) {
            LOG_WARNING(sLogger, ("failed to seek to position", "")("config", configName)("idx", idx)("position", config.seekPosition));
        }
    }
    
    // 保存配置信息
    ConfigInfo configInfo;
    configInfo.configName = configName;
    configInfo.idx = idx;
    configInfo.config = config;
    configInfo.handler = handler;
    configInfo.reader = reader;
    
    mConfigs[configKey] = std::move(configInfo);
    
    LOG_INFO(sLogger, ("config added with independent reader", "")("config", configName)("idx", idx)("total_configs", mConfigs.size()));
    
    return true;
}

void JournalReaderManager::RemoveConfig(const std::string& configName, size_t idx) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    if (!mInitialized) {
        return;
    }
    
    std::string configKey = makeConfigKey(configName, idx);
    
    auto it = mConfigs.find(configKey);
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("removing config", "")("config", configName)("idx", idx));
        
        // 关闭reader
        if (it->second.reader) {
            it->second.reader->Close();
        }
        
        mConfigs.erase(it);
        LOG_INFO(sLogger, ("config removed", "")("config", configName)("idx", idx)("remaining_configs", mConfigs.size()));
    } else {
        LOG_WARNING(sLogger, ("config not found for removal", "")("config", configName)("idx", idx));
    }
}

JournalReaderManager::Stats JournalReaderManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    Stats stats;
    stats.totalConfigs = mConfigs.size();
    stats.activeConnections = 0;
    stats.invalidConnections = 0;
    
    // 兼容性字段
    stats.totalGroups = 0;  // 不再使用分组
    stats.sharedInotifyInstances = 0;  // 不再共享
    stats.totalConnections = mConfigs.size();
    
    for (const auto& [configKey, configInfo] : mConfigs) {
        stats.connectionKeys.push_back(configKey);
        
        if (configInfo.reader && configInfo.reader->IsOpen()) {
            stats.activeConnections++;
        } else {
            stats.invalidConnections++;
        }
    }
    
    return stats;
}

std::shared_ptr<SystemdJournalReader> JournalReaderManager::GetConnectionInfo(const std::string& configName, size_t idx) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string configKey = makeConfigKey(configName, idx);
    auto it = mConfigs.find(configKey);
    if (it != mConfigs.end()) {
        return it->second.reader;
    }
    
    return nullptr;
}

std::vector<std::pair<std::string, size_t>> JournalReaderManager::GetConfigsUsingReader(const std::shared_ptr<SystemdJournalReader>& reader) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::pair<std::string, size_t>> configs;
    
    // 由于每个reader只对应一个配置，查找这个reader对应的配置
    for (const auto& [configKey, configInfo] : mConfigs) {
        if (configInfo.reader == reader) {
            configs.emplace_back(configInfo.configName, configInfo.idx);
            break;  // 找到就退出，因为每个reader只对应一个配置
        }
    }
    
    return configs;
}

bool JournalReaderManager::ForceResetConnection(const std::string& configName, size_t idx) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    std::string configKey = makeConfigKey(configName, idx);
    auto it = mConfigs.find(configKey);
    
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("forcing connection reset", "")("config", configName)("idx", idx));
        
        // 关闭并删除reader
        if (it->second.reader) {
            it->second.reader->Close();
        }
        
        mConfigs.erase(it);
        LOG_INFO(sLogger, ("connection reset completed", "")("config", configName)("idx", idx));
        
        return true;
    }
    
    LOG_WARNING(sLogger, ("config not found for reset", "")("config", configName)("idx", idx));
    return false;
}

size_t JournalReaderManager::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfigs.size();
}

} // namespace logtail