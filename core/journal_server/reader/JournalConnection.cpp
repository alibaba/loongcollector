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
#include "JournalFilter.h"
#include "JournalReader.h"
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

    // chear all readers of configs
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

    if (mConfigs.find(configName) != mConfigs.end()) {
        LOG_WARNING(sLogger,
                    ("journal connection config already exists, will be replaced", "")("config", configName));
        // close old reader
        if (mConfigs[configName].reader) {
            mConfigs[configName].reader->Close();
        }
    }

    auto reader = std::make_shared<JournalReader>();

    // initialize reader (set paths, open, apply filters)
    if (!InitializeReader(reader, config, configName)) {
        return false;
    }

    // set read position
    SetupReaderPosition(reader, config, configName);

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
        LOG_INFO(sLogger, ("journal connection removing config", "")("config", configName));

        if (it->second.reader) {
            it->second.reader->Close();
        }

        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection config removed", "")("config", configName)("remaining_configs",
                                                                                         mConfigs.size()));
    } else {
        LOG_WARNING(sLogger, ("journal connection config not found for removal", "")("config", configName));
    }
}

void JournalConnection::RemoveConfigWithEpollCleanup(const std::string& configName, int epollFD) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        return;
    }

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        LOG_INFO(sLogger, ("journal connection removing config with epoll cleanup", "")("config", configName));

        auto& configInfo = it->second;
        
        // clean epoll monitoring (if epollFD is valid and reader exists)
        if (epollFD >= 0 && configInfo.reader && configInfo.reader->IsOpen()) {
            configInfo.reader->RemoveFromEpoll(epollFD);
            LOG_DEBUG(sLogger,
                      ("journal connection removed reader from epoll", "")("config", configName)("fd", epollFD));
        }

        // close connection
        if (configInfo.reader) {
            configInfo.reader->Close();
        }

        mConfigs.erase(it);
        LOG_INFO(sLogger,
                 ("journal connection config removed with epoll cleanup", "")("config", configName)(
                     "remaining_configs", mConfigs.size()));
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

size_t JournalConnection::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfigs.size();
}

JournalFilter::FilterConfig
JournalConnection::BuildFilterConfig(const JournalConfig& config, const std::string& configName) {
    JournalFilter::FilterConfig filterConfig;
    filterConfig.mUnits = config.mUnits;
    filterConfig.mIdentifiers = config.mIdentifiers;
    filterConfig.mMatchPatterns = config.mMatchPatterns;
    filterConfig.mEnableKernel = config.mKernel;
    filterConfig.mConfigName = configName;
    filterConfig.mConfigIndex = 0; // Always 0 for singleton journal input
    return filterConfig;
}

bool JournalConnection::InitializeReader(const std::shared_ptr<JournalReader>& reader,
                                                const JournalConfig& config,
                                                const std::string& configName) {
    if (!reader) {
        LOG_ERROR(sLogger,
                  ("journal connection reader is null, cannot initialize", "")("config", configName));
        return false;
    }

    if (!config.mJournalPaths.empty()) {
        reader->SetJournalPaths(config.mJournalPaths);
        LOG_INFO(sLogger,
                 ("journal connection setting journal paths",
                  "")("config", configName)("paths_count", config.mJournalPaths.size()));
        for (const auto& path : config.mJournalPaths) {
            LOG_INFO(sLogger, ("journal path", path));
        }
    }

#ifdef APSARA_UNIT_TEST_MAIN
    // in test environment, even if Open fails, continue to add config
    reader->Open(); // try to open, but do not check result
#else
    if (!reader->Open()) {
        LOG_ERROR(sLogger,
                  ("journal connection failed to open journal connection", "")("config", configName));
        return false;
    }
#endif

    auto filterConfig = BuildFilterConfig(config, configName);
    if (!JournalFilter::ApplyAllFilters(reader.get(), filterConfig)) {
        LOG_ERROR(sLogger,
                  ("journal connection failed to apply filters to connection", "")("config", configName));
        reader->Close();
        return false;
    }

    return true;
}

bool JournalConnection::SetupReaderPosition(const std::shared_ptr<JournalReader>& reader,
                                                    const JournalConfig& config,
                                                    const std::string& configName,
                                                    const std::string& savedCursor) {
    if (!reader || !reader->IsOpen()) {
        LOG_WARNING(sLogger,
                    ("journal connection reader is not open, cannot setup position", "")("config",
                                                                                                   configName));
        return false;
    }

    // use saved cursor first
    if (!savedCursor.empty()) {
        if (reader->SeekCursor(savedCursor)) {
            reader->Next();
            LOG_DEBUG(sLogger,
                      ("journal connection restored cursor", "")("config", configName)("cursor", savedCursor));
            return true;
        }
        LOG_WARNING(sLogger,
                    ("journal connection failed to restore cursor", "")("config", configName)("cursor",
                                                                                                        savedCursor));
    }

    // if no saved cursor or cursor recovery failed, use seek position in config
    if (!config.mSeekPosition.empty()) {
        bool seekSuccess = false;
        if (config.mSeekPosition == "tail") {
            seekSuccess = reader->SeekTail();
            LOG_INFO(sLogger,
                     ("journal connection seek to tail", "")("config", configName)("success", seekSuccess));

            if (seekSuccess) {
                reader->Previous();
            }
        } else {
            seekSuccess = reader->SeekHead();
            LOG_INFO(sLogger,
                     ("journal connection seek to head", "")("config", configName)("success", seekSuccess));
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

bool JournalConnection::RefreshConnection(const std::string& configName, int epollFD) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mInitialized) {
        LOG_WARNING(sLogger,
                    ("journal connection cannot refresh connection", "not initialized")("config",
                                                                                                  configName));
        return false;
    }

    auto it = mConfigs.find(configName);
    if (it == mConfigs.end()) {
        LOG_WARNING(sLogger, ("journal connection config not found for refresh", "")("config", configName));
        return false;
    }

    auto& configInfo = it->second;
    if (!configInfo.reader) {
        LOG_WARNING(sLogger,
                    ("journal connection reader is null, cannot refresh", "")("config", configName));
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
                  ("journal connection failed to reopen connection after refresh", "")("config", configName));
        return false;
    }

    auto filterConfig = BuildFilterConfig(configInfo.config, configName);
    if (!JournalFilter::ApplyAllFilters(configInfo.reader.get(), filterConfig)) {
        LOG_ERROR(sLogger,
                  ("journal connection failed to reapply filters after refresh", "")("config", configName));
        configInfo.reader->Close();
        return false;
    }

    SetupReaderPosition(configInfo.reader, configInfo.config, configName, currentCursor);

    if (epollFD >= 0) {
        bool success = configInfo.reader->AddToEpoll(epollFD);
        if (!success) {
            LOG_WARNING(sLogger,
                        ("journal connection failed to re-add reader to epoll after refresh", "")("config",
                                                                                                             configName));
        }
    }

    configInfo.lastOpenTime = std::chrono::steady_clock::now();

    LOG_INFO(sLogger,
             ("journal connection connection refreshed successfully", "")("config", configName)("interval",
                                                                                                        configInfo.config.mResetIntervalSecond));

    return true;
}

std::set<std::string> JournalConnection::GetValidConfigNames() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::set<std::string> validConfigNames;
    for (const auto& [configName, configInfo] : mConfigs) {
        validConfigNames.insert(configName);
    }

    return validConfigNames;
}

std::map<std::string, std::shared_ptr<JournalReader>> JournalConnection::GetOpenConnections() const {
    std::lock_guard<std::mutex> lock(mMutex);

    std::map<std::string, std::shared_ptr<JournalReader>> openConnections;
    for (const auto& [configName, configInfo] : mConfigs) {
        if (configInfo.reader && configInfo.reader->IsOpen()) {
            openConnections[configName] = configInfo.reader;
        }
    }

    return openConnections;
}

int JournalConnection::CleanupRemovedReadersFromEpoll(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) const {
    std::set<std::string> validConfigNames = GetValidConfigNames();
    int cleanedCount = 0;

    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end();) {
        auto& monitoredReader = it->second;

        // Check if configuration still exists
        if (validConfigNames.find(monitoredReader.configName) == validConfigNames.end()) {
            LOG_DEBUG(sLogger,
                      ("journal connection removing reader from monitoring",
                       "config no longer exists")("config", monitoredReader.configName)("fd", it->first));

            // Remove from epoll
            if (monitoredReader.reader && monitoredReader.reader->IsOpen()) {
                monitoredReader.reader->RemoveFromEpoll(epollFD);
            }

            it = monitoredReaders.erase(it);
            cleanedCount++;
        } else {
            ++it;
        }
    }

    return cleanedCount;
}

} // namespace logtail
