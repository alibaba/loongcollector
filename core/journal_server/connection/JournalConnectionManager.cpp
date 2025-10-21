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

#include "JournalConnectionManager.h"

#include "../reader/JournalFilter.h"
#include "../reader/JournalReader.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

//==============================================================================
// JournalConnectionManager 实现
//==============================================================================

JournalConnectionManager& JournalConnectionManager::GetInstance() {
    static JournalConnectionManager sInstance;
    return sInstance;
}

bool JournalConnectionManager::Initialize() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mInitialized) {
        return true;
    }

    LOG_INFO(sLogger, ("journal connection manager initializing", ""));
    mInitialized = true;

    return true;
}

void JournalConnectionManager::Cleanup() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    LOG_INFO(sLogger, ("journal connection manager cleaning up", "")("total_configs", mConfigs.size()));

    // 清理所有配置和连接
    for (auto& [key, configInfo] : mConfigs) {
        if (configInfo.reader) {
            configInfo.reader->Close();
        }
    }

    mConfigs.clear();
    mInitialized = false;
}

std::string JournalConnectionManager::makeConfigKey(const std::string& configName, size_t idx) const {
    return configName + ":" + std::to_string(idx);
}

bool JournalConnectionManager::AddConfig(const std::string& configName,
                                         size_t idx,
                                         const JournalConfig& config,
                                         const ConfigHandler& handler) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        LOG_ERROR(sLogger, ("journal connection manager not initialized", ""));
        return false;
    }

    std::string configKey = makeConfigKey(configName, idx);

    // 检查配置是否已存在
    if (mConfigs.find(configKey) != mConfigs.end()) {
        LOG_WARNING(sLogger,
                    ("journal connection manager config already exists, will be replaced", "")("config",
                                                                                               configName)("idx", idx));
        // 关闭旧的连接
        if (mConfigs[configKey].reader) {
            mConfigs[configKey].reader->Close();
        }
    }

    LOG_INFO(
        sLogger,
        ("journal connection manager adding config with independent connection", "")("config", configName)("idx", idx));

    // 创建独立的journal连接（reader）
    auto reader = std::make_shared<SystemdJournalReader>();
    if (!reader->Open()) {
        LOG_ERROR(
            sLogger,
            ("journal connection manager failed to create journal connection", "")("config", configName)("idx", idx));
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
        LOG_ERROR(
            sLogger,
            ("journal connection manager failed to apply filters to connection", "")("config", configName)("idx", idx));
        reader->Close();
        return false;
    }

    // 设置seek位置
    if (!config.seekPosition.empty()) {
        bool seekSuccess = false;
        if (config.seekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger,
                     ("journal connection manager seek to tail", "")("config", configName)("idx", idx)("success",
                                                                                                       seekSuccess));

            // SeekTail()后需要调用Previous()才能读取到实际的日志条目
            if (seekSuccess) {
                reader->Previous();
            }
        } else {
            seekSuccess = reader->SeekHead();
            LOG_INFO(sLogger,
                     ("journal connection manager seek to head", "")("config", configName)("idx", idx)("success",
                                                                                                       seekSuccess));
        }

        if (!seekSuccess) {
            LOG_WARNING(sLogger,
                        ("journal connection manager failed to seek to position",
                         "")("config", configName)("idx", idx)("position", config.seekPosition));
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

    LOG_INFO(sLogger,
             ("journal connection manager config added with independent connection",
              "")("config", configName)("idx", idx)("total_configs", mConfigs.size()));

    return true;
}

void JournalConnectionManager::RemoveConfig(const std::string& configName, size_t idx) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    std::string configKey = makeConfigKey(configName, idx);

    auto it = mConfigs.find(configKey);
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("journal connection manager removing config", "")("config", configName)("idx", idx));

        // 关闭连接
        if (it->second.reader) {
            it->second.reader->Close();
        }

        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection manager config removed",
                  "")("config", configName)("idx", idx)("remaining_configs", mConfigs.size()));
    } else {
        LOG_WARNING(sLogger,
                    ("journal connection manager config not found for removal", "")("config", configName)("idx", idx));
    }
}

JournalConnectionManager::Stats JournalConnectionManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mMutex);

    Stats stats;
    stats.totalConfigs = mConfigs.size();
    stats.activeConnections = 0;
    stats.invalidConnections = 0;
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

std::shared_ptr<SystemdJournalReader> JournalConnectionManager::GetConnection(const std::string& configName,
                                                                              size_t idx) const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::string configKey = makeConfigKey(configName, idx);
    auto it = mConfigs.find(configKey);
    if (it != mConfigs.end()) {
        return it->second.reader;
    }

    return nullptr;
}

JournalConfig JournalConnectionManager::GetConfig(const std::string& configName, size_t idx) const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::string configKey = makeConfigKey(configName, idx);
    auto it = mConfigs.find(configKey);
    if (it != mConfigs.end()) {
        return it->second.config;
    }

    // 返回空配置
    return JournalConfig();
}

std::map<std::pair<std::string, size_t>, JournalConfig> JournalConnectionManager::GetAllConfigs() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::pair<std::string, size_t>, JournalConfig> result;
    for (const auto& [configKey, configInfo] : mConfigs) {
        result[{configInfo.configName, configInfo.idx}] = configInfo.config;
    }

    return result;
}

std::vector<std::pair<std::string, size_t>>
JournalConnectionManager::GetConfigsUsingConnection(const std::shared_ptr<SystemdJournalReader>& reader) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::pair<std::string, size_t>> configs;

    // 由于每个连接只对应一个配置，查找这个连接对应的配置
    for (const auto& [configKey, configInfo] : mConfigs) {
        if (configInfo.reader == reader) {
            configs.emplace_back(configInfo.configName, configInfo.idx);
            break; // 找到就退出，因为每个连接只对应一个配置
        }
    }

    return configs;
}

size_t JournalConnectionManager::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfigs.size();
}

} // namespace logtail
