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

bool JournalConnectionManager::AddConfig(const std::string& configName, const JournalConfig& config) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        LOG_ERROR(sLogger, ("journal connection manager not initialized", ""));
        return false;
    }

    // 检查配置是否已存在
    if (mConfigs.find(configName) != mConfigs.end()) {
        LOG_WARNING(sLogger,
                    ("journal connection manager config already exists, will be replaced", "")("config", configName));
        // 关闭旧的连接
        if (mConfigs[configName].reader) {
            mConfigs[configName].reader->Close();
        }
    }

    LOG_INFO(sLogger,
             ("journal connection manager adding config with independent connection", "")("config", configName));

    // 创建独立的journal连接（reader）
    auto reader = std::make_shared<SystemdJournalReader>();

#ifdef APSARA_UNIT_TEST_MAIN
    // 在测试环境中，即使Open失败也继续添加配置
    reader->Open(); // 尝试打开，但不检查结果
#else
    if (!reader->Open()) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to create journal connection", "")("config", configName));
        return false;
    }
#endif

    // 应用过滤器
    JournalFilter::FilterConfig filterConfig;
    filterConfig.units = config.units;
    filterConfig.identifiers = config.identifiers;
    filterConfig.matchPatterns = config.matchPatterns;
    filterConfig.enableKernel = config.kernel;
    filterConfig.configName = configName;
    filterConfig.configIndex = 0; // Always 0 for singleton journal input

    if (!JournalFilter::ApplyAllFilters(reader.get(), filterConfig)) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to apply filters to connection", "")("config", configName));
        reader->Close();
        return false;
    }

    // 设置seek位置
    if (!config.seekPosition.empty()) {
        bool seekSuccess = false;
        if (config.seekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger,
                     ("journal connection manager seek to tail", "")("config", configName)("success", seekSuccess));

            // SeekTail()后需要调用Previous()才能读取到实际的日志条目
            if (seekSuccess) {
                reader->Previous();
            }
        } else {
            seekSuccess = reader->SeekHead();
            LOG_INFO(sLogger,
                     ("journal connection manager seek to head", "")("config", configName)("success", seekSuccess));
        }

        if (!seekSuccess) {
            LOG_WARNING(sLogger,
                        ("journal connection manager failed to seek to position",
                         "")("config", configName)("position", config.seekPosition));
        }
    }

    // 保存配置信息
    ConfigInfo configInfo;
    configInfo.configName = configName;
    configInfo.config = config;
    configInfo.reader = reader;

    mConfigs[configName] = std::move(configInfo);

    LOG_INFO(sLogger,
             ("journal connection manager config added with independent connection",
              "")("config", configName)("total_configs", mConfigs.size()));

    return true;
}

void JournalConnectionManager::RemoveConfig(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("journal connection manager removing config", "")("config", configName));

        // 关闭连接
        if (it->second.reader) {
            it->second.reader->Close();
        }

        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection manager config removed", "")("config", configName)("remaining_configs",
                                                                                         mConfigs.size()));
    } else {
        LOG_WARNING(sLogger, ("journal connection manager config not found for removal", "")("config", configName));
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

std::shared_ptr<SystemdJournalReader> JournalConnectionManager::GetConnection(const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        return it->second.reader;
    }

    return nullptr;
}

JournalConfig JournalConnectionManager::GetConfig(const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        return it->second.config;
    }

    // 返回空配置
    return JournalConfig();
}

std::map<std::string, JournalConfig> JournalConnectionManager::GetAllConfigs() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::string, JournalConfig> result;
    for (const auto& [configKey, configInfo] : mConfigs) {
        result[configInfo.configName] = configInfo.config;
    }

    return result;
}

std::vector<std::string>
JournalConnectionManager::GetConfigsUsingConnection(const std::shared_ptr<SystemdJournalReader>& reader) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::string> configs;

    // 由于每个连接只对应一个配置，查找这个连接对应的配置
    for (const auto& [configKey, configInfo] : mConfigs) {
        if (configInfo.reader == reader) {
            configs.emplace_back(configInfo.configName);
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
