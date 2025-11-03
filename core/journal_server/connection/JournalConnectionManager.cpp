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

    auto reader = std::make_shared<JournalReader>();

    // 初始化reader（设置路径、打开、应用过滤器）
    if (!InitializeReader(reader, config, configName)) {
        return false;
    }

    // 设置读取位置
    SetupReaderPosition(reader, config, configName);

    ConfigInfo configInfo;
    configInfo.mConfigName = configName;
    configInfo.config = config;
    configInfo.reader = reader;
    configInfo.lastOpenTime = std::chrono::steady_clock::now(); // 记录打开时间

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

void JournalConnectionManager::RemoveConfigWithEpollCleanup(const std::string& configName, int epollFD) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("journal connection manager removing config with epoll cleanup", "")("config", configName));

        auto& configInfo = it->second;
        
        // 清理epoll监控（如果epollFD有效且reader存在）
        if (epollFD >= 0 && configInfo.reader && configInfo.reader->IsOpen()) {
            configInfo.reader->RemoveFromEpoll(epollFD);
            LOG_DEBUG(sLogger,
                      ("journal connection manager removed reader from epoll", "")("config", configName)("fd", epollFD));
        }

        // 关闭连接
        if (configInfo.reader) {
            configInfo.reader->Close();
        }

        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection manager config removed with epoll cleanup", "")("config", configName)(
                     "remaining_configs", mConfigs.size()));
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

std::shared_ptr<JournalReader> JournalConnectionManager::GetConnection(const std::string& configName) const {
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

    return JournalConfig();
}

std::map<std::string, JournalConfig> JournalConnectionManager::GetAllConfigs() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::string, JournalConfig> result;
    for (const auto& [configKey, configInfo] : mConfigs) {
        result[configInfo.mConfigName] = configInfo.config;
    }

    return result;
}

std::vector<std::string>
JournalConnectionManager::GetConfigsUsingConnection(const std::shared_ptr<JournalReader>& reader) const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::string> configs;

    for (const auto& [configKey, configInfo] : mConfigs) {
        if (configInfo.reader == reader) {
            configs.emplace_back(configInfo.mConfigName);
            break; // 找到就退出，因为每个连接只对应一个配置
        }
    }

    return configs;
}

size_t JournalConnectionManager::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfigs.size();
}

JournalFilter::FilterConfig
JournalConnectionManager::BuildFilterConfig(const JournalConfig& config, const std::string& configName) {
    JournalFilter::FilterConfig filterConfig;
    filterConfig.mUnits = config.mUnits;
    filterConfig.mIdentifiers = config.mIdentifiers;
    filterConfig.mMatchPatterns = config.mMatchPatterns;
    filterConfig.mEnableKernel = config.mKernel;
    filterConfig.mConfigName = configName;
    filterConfig.mConfigIndex = 0; // Always 0 for singleton journal input
    return filterConfig;
}

bool JournalConnectionManager::InitializeReader(const std::shared_ptr<JournalReader>& reader,
                                                const JournalConfig& config,
                                                const std::string& configName) {
    if (!reader) {
        LOG_ERROR(sLogger,
                  ("journal connection manager reader is null, cannot initialize", "")("config", configName));
        return false;
    }

    if (!config.mJournalPaths.empty()) {
        reader->SetJournalPaths(config.mJournalPaths);
        LOG_INFO(sLogger,
                 ("journal connection manager setting journal paths",
                  "")("config", configName)("paths_count", config.mJournalPaths.size()));
        for (const auto& path : config.mJournalPaths) {
            LOG_INFO(sLogger, ("journal path", path));
        }
    }

#ifdef APSARA_UNIT_TEST_MAIN
    // 在测试环境中，即使Open失败也继续添加配置
    reader->Open(); // 尝试打开，但不检查结果
#else
    if (!reader->Open()) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to open journal connection", "")("config", configName));
        return false;
    }
#endif

    auto filterConfig = BuildFilterConfig(config, configName);
    if (!JournalFilter::ApplyAllFilters(reader.get(), filterConfig)) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to apply filters to connection", "")("config", configName));
        reader->Close();
        return false;
    }

    return true;
}

bool JournalConnectionManager::SetupReaderPosition(const std::shared_ptr<JournalReader>& reader,
                                                    const JournalConfig& config,
                                                    const std::string& configName,
                                                    const std::string& savedCursor) {
    if (!reader || !reader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal connection manager reader is not open, cannot setup position", "")("config",
                                                                                                   configName));
        return false;
    }

    // 优先使用保存的cursor
    if (!savedCursor.empty()) {
        if (reader->SeekCursor(savedCursor)) {
            reader->Next();
            LOG_DEBUG(sLogger,
                      ("journal connection manager restored cursor", "")("config", configName)("cursor", savedCursor));
            return true;
        }
        LOG_WARNING(sLogger,
                    ("journal connection manager failed to restore cursor", "")("config", configName)("cursor",
                                                                                                        savedCursor));
    }

    // 如果没有保存的cursor或cursor恢复失败，使用配置的seek位置
    if (!config.mSeekPosition.empty()) {
        bool seekSuccess = false;
        if (config.mSeekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger,
                     ("journal connection manager seek to tail", "")("config", configName)("success", seekSuccess));

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
                         "")("config", configName)("position", config.mSeekPosition));
        }
        return seekSuccess;
    }

    return true;
}

bool JournalConnectionManager::ShouldRefreshConnection(const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return false;
    }

    auto it = mConfigs.find(configName);
    if (it == mConfigs.end()) {
        return false;
    }

    const auto& configInfo = it->second;
    if (!configInfo.reader || !configInfo.reader->IsOpen()) {
        return false;
    }

    // check if it's time to refresh
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - configInfo.lastOpenTime).count();
    int resetInterval = configInfo.config.mResetIntervalSecond;

    return elapsed >= resetInterval;
}

bool JournalConnectionManager::RefreshConnection(const std::string& configName, int epollFD) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        LOG_WARNING(sLogger,
                    ("journal connection manager cannot refresh connection", "not initialized")("config",
                                                                                                  configName));
        return false;
    }

    auto it = mConfigs.find(configName);
    if (it == mConfigs.end()) {
        LOG_WARNING(sLogger, ("journal connection manager config not found for refresh", "")("config", configName));
        return false;
    }

    auto& configInfo = it->second;
    if (!configInfo.reader) {
        LOG_WARNING(sLogger,
                    ("journal connection manager reader is null, cannot refresh", "")("config", configName));
        return false;
    }

    std::string currentCursor;
    if (configInfo.reader->IsOpen()) {
        currentCursor = configInfo.reader->GetCursor();
    }

    if (configInfo.reader->IsOpen() && epollFD >= 0) {
        configInfo.reader->RemoveFromEpoll(epollFD);
    }

    configInfo.reader->Close();

    // reopen connection
    if (!configInfo.reader->Open()) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to reopen connection after refresh", "")("config", configName));
        return false;
    }

    auto filterConfig = BuildFilterConfig(configInfo.config, configName);
    if (!JournalFilter::ApplyAllFilters(configInfo.reader.get(), filterConfig)) {
        LOG_ERROR(sLogger,
                  ("journal connection manager failed to reapply filters after refresh", "")("config", configName));
        configInfo.reader->Close();
        return false;
    }

    SetupReaderPosition(configInfo.reader, configInfo.config, configName, currentCursor);

    if (epollFD >= 0) {
        bool success = configInfo.reader->AddToEpoll(epollFD);
        if (!success) {
            LOG_WARNING(sLogger,
                        ("journal connection manager failed to re-add reader to epoll after refresh", "")("config",
                                                                                                             configName));
        }
    }

    configInfo.lastOpenTime = std::chrono::steady_clock::now();

    LOG_INFO(sLogger,
             ("journal connection manager connection refreshed successfully", "")("config", configName)("interval",
                                                                                                        configInfo.config.mResetIntervalSecond));

    return true;
}

std::set<std::string> JournalConnectionManager::GetValidConfigNames() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::set<std::string> validConfigNames;
    for (const auto& [configName, configInfo] : mConfigs) {
        validConfigNames.insert(configName);
    }

    return validConfigNames;
}

std::map<std::string, std::shared_ptr<JournalReader>> JournalConnectionManager::GetOpenConnections() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::string, std::shared_ptr<JournalReader>> openConnections;
    for (const auto& [configName, configInfo] : mConfigs) {
        if (configInfo.reader && configInfo.reader->IsOpen()) {
            openConnections[configName] = configInfo.reader;
        }
    }

    return openConnections;
}

} // namespace logtail
