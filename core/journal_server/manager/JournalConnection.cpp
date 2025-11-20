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

#include "JournalConnection.h"

#include "../JournalServer.h"
#include "../reader/JournalFilter.h"
#include "../reader/JournalReader.h"
#include "JournalMonitor.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {


JournalConnection& JournalConnection::GetInstance() {
    static JournalConnection sInstance;
    return sInstance;
}

bool JournalConnection::Initialize() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mInitialized) {
        return true;
    }

    LOG_INFO(sLogger, ("journal connection initializing", ""));
    mInitialized = true;

    return true;
}

void JournalConnection::Cleanup() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    LOG_INFO(sLogger, ("journal connection cleaning up", "")("total_configs", mConfigs.size()));

    // Close all readers of configs
    for (auto& [key, configInfo] : mConfigs) {
        if (configInfo.reader) {
            configInfo.reader->Close();
        }
    }

    mConfigs.clear();
    mInitialized = false;
}

bool JournalConnection::AddConfig(const std::string& configName, const JournalConfig& config) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        LOG_ERROR(sLogger, ("journal connection not initialized", ""));
        return false;
    }

    // Note: In normal flow, config should have been removed by RemoveConfig() before AddConfig() is called
    // However, we allow replacement here for backward compatibility (e.g., unit tests).
    if (mConfigs.find(configName) != mConfigs.end()) {
        LOG_WARNING(sLogger, ("journal connection config already exists, will be replaced", "")("config", configName));
        if (mConfigs[configName].reader) {
            mConfigs[configName].reader->Close();
        }
    }

    auto reader = std::make_shared<JournalReader>();

    // Initialize reader (set paths, open, apply filters)
    if (!initializeReader(reader, config, configName)) {
        return false;
    }

    // Set read position
    setupReaderPosition(reader, config, configName);

    ConfigInfo configInfo;
    configInfo.mConfigName = configName;
    configInfo.config = config;
    configInfo.reader = reader;
    configInfo.lastOpenTime = std::chrono::steady_clock::now(); // record open time

    mConfigs[configName] = std::move(configInfo);

    LOG_INFO(sLogger,
             ("journal connection config added with independent connection",
              "")("config", configName)("total_configs", mConfigs.size()));

    return true;
}


void JournalConnection::RemoveConfig(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection config removed, reader will be closed in CleanupClosedReaders",
                  "")("config", configName)("remaining_configs", mConfigs.size()));
    } else {
        LOG_WARNING(sLogger, ("journal connection config not found for removal", "")("config", configName));
    }
}

JournalConnection::Stats JournalConnection::GetStats() const {
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

std::shared_ptr<JournalReader> JournalConnection::GetConnection(const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        return it->second.reader;
    }

    return nullptr;
}

size_t JournalConnection::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfigs.size();
}

JournalConfig JournalConnection::GetConfig(const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        return it->second.config;
    }

    return JournalConfig();
}

std::map<std::string, JournalConfig> JournalConnection::GetAllConfigs() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::string, JournalConfig> result;
    for (const auto& [configKey, configInfo] : mConfigs) {
        result[configInfo.mConfigName] = configInfo.config;
    }

    return result;
}

std::vector<std::string> JournalConnection::GetAllConfigNames() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::vector<std::string> configNames;
    configNames.reserve(mConfigs.size());
    for (const auto& [configKey, configInfo] : mConfigs) {
        configNames.push_back(configInfo.mConfigName);
    }

    return configNames;
}

void JournalConnection::RefreshConnectionsByInterval(const std::vector<std::string>& configNames,
                                                     JournalMonitor& monitor) {
    for (const auto& configName : configNames) {
        if (ShouldRefreshConnection(configName)) {
            monitor.RemoveReaderFromMonitoring(configName);

            if (RefreshConnection(configName)) {
                monitor.RefreshReaderFDMapping(configName);
            } else {
                LOG_ERROR(sLogger, ("journal connection refresh failed", "")("config", configName));
            }
        }
    }
}

bool JournalConnection::ShouldRefreshConnection(const std::string& configName) const {
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

bool JournalConnection::RefreshConnection(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = mConfigs.find(configName);
    if (it == mConfigs.end()) {
        LOG_WARNING(sLogger, ("journal connection config not found for refresh", "")("config", configName));
        return false;
    }

    auto& configInfo = it->second;
    if (!configInfo.reader) {
        LOG_WARNING(sLogger, ("journal connection reader is null, cannot refresh", "")("config", configName));
        return false;
    }

    std::string currentCursor;
    if (configInfo.reader->IsOpen()) {
        currentCursor = configInfo.reader->GetCursor();
    }

    configInfo.reader->Close();

    // mJournalPaths is stored in the reader object and doesn't need to be reset,
    if (!configInfo.reader->Open()) {
        LOG_ERROR(sLogger, ("journal connection failed to reopen connection after refresh", "")("config", configName));
        return false;
    }

    // Reapply filters: necessary because Close() clears all filters
    auto filterConfig = buildFilterConfig(configInfo.config, configName);
    if (!JournalFilter::ApplyAllFilters(configInfo.reader.get(), filterConfig)) {
        LOG_ERROR(sLogger, ("journal connection failed to reapply filters after refresh", "")("config", configName));
        configInfo.reader->Close();
        return false;
    }

    setupReaderPosition(configInfo.reader, configInfo.config, configName, currentCursor);

    // Epoll re-registration should be handled by JournalMonitor
    configInfo.lastOpenTime = std::chrono::steady_clock::now();

    LOG_INFO(sLogger,
             ("journal connection connection refreshed successfully",
              "")("config", configName)("interval", configInfo.config.mResetIntervalSecond));

    return true;
}


JournalFilter::FilterConfig JournalConnection::buildFilterConfig(const JournalConfig& config,
                                                                 const std::string& configName) {
    JournalFilter::FilterConfig filterConfig;
    filterConfig.mUnits = config.mUnits;
    filterConfig.mIdentifiers = config.mIdentifiers;
    filterConfig.mMatchPatterns = config.mMatchPatterns;
    filterConfig.mEnableKernel = config.mKernel;
    filterConfig.mConfigName = configName;
    filterConfig.mConfigIndex = 0; // Always 0 for singleton journal input
    return filterConfig;
}

bool JournalConnection::initializeReader(const std::shared_ptr<JournalReader>& reader,
                                         const JournalConfig& config,
                                         const std::string& configName) {
    if (!reader) {
        LOG_ERROR(sLogger, ("journal connection reader is null, cannot initialize", "")("config", configName));
        return false;
    }

    if (!config.mJournalPaths.empty()) {
        reader->SetJournalPaths(config.mJournalPaths);
        LOG_INFO(sLogger,
                 ("journal connection setting journal paths", "")("config", configName)("paths_count",
                                                                                        config.mJournalPaths.size()));
        for (const auto& path : config.mJournalPaths) {
            LOG_INFO(sLogger, ("journal path", path));
        }
    }

#ifdef APSARA_UNIT_TEST_MAIN
    // in test environment, even if Open fails, continue to add config
    reader->Open(); // try to open, but do not check result
#else
    if (!reader->Open()) {
        LOG_ERROR(sLogger, ("journal connection failed to open journal connection", "")("config", configName));
        return false;
    }
#endif

    auto filterConfig = buildFilterConfig(config, configName);
    if (!JournalFilter::ApplyAllFilters(reader.get(), filterConfig)) {
        LOG_ERROR(sLogger, ("journal connection failed to apply filters to connection", "")("config", configName));
        reader->Close();
        return false;
    }

    return true;
}

bool JournalConnection::setupReaderPosition(const std::shared_ptr<JournalReader>& reader,
                                            const JournalConfig& config,
                                            const std::string& configName,
                                            const std::string& savedCursor) {
    if (!reader || !reader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal connection reader is not open, cannot setup position", "")("config", configName));
        return false;
    }

    // use saved cursor first
    if (!savedCursor.empty()) {
        if (reader->SeekCursor(savedCursor)) {
            reader->Next();
            LOG_DEBUG(sLogger, ("journal connection restored cursor", "")("config", configName)("cursor", savedCursor));
            return true;
        }
        LOG_WARNING(sLogger,
                    ("journal connection failed to restore cursor", "")("config", configName)("cursor", savedCursor));
    }

    // if no saved cursor or cursor recovery failed, use seek position in config
    if (!config.mSeekPosition.empty()) {
        bool seekSuccess = false;
        if (config.mSeekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger, ("journal connection seek to tail", "")("config", configName)("success", seekSuccess));

            if (seekSuccess) {
                reader->Previous();
            }
        } else {
            seekSuccess = reader->SeekHead();
            LOG_INFO(sLogger, ("journal connection seek to head", "")("config", configName)("success", seekSuccess));
        }

        if (!seekSuccess) {
            LOG_WARNING(sLogger,
                        ("journal connection failed to seek to position",
                         "")("config", configName)("position", config.mSeekPosition));
        }
        return seekSuccess;
    }

    return true;
}

} // namespace logtail
