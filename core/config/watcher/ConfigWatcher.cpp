// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "config/watcher/ConfigWatcher.h"

#include <iostream>
#include <memory>
#include <unordered_set>

#include "logger/Logger.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/ProcessConfigManager.h"

using namespace std;

namespace logtail {

bool ReadFile(const string& filepath, string& content);

ConfigWatcher::ConfigWatcher()
    : mPipelineManager(PipelineManager::GetInstance()), mProcessConfigManager(ProcessConfigManager::GetInstance()) {
}

template <typename ConfigManagerType, typename ConfigType, typename ConfigDiffType, typename ManagerConfigType>
ConfigDiffType ConfigWatcher::CheckConfigDiff(
    const std::vector<std::filesystem::path>& configDir,
    const std::unordered_map<std::string, std::mutex*>& configDirMutexMap,
    std::map<std::string, std::pair<uintmax_t, std::filesystem::file_time_type>>& fileInfoMap,
    const ConfigManagerType* configManager,
    const std::string& configType) {
    ConfigDiffType diff;
    unordered_set<string> configSet;
    for (const auto& dir : configDir) {
        error_code ec;
        filesystem::file_status s = filesystem::status(dir, ec);
        if (ec) {
            LOG_WARNING(sLogger,
                        ("failed to get config dir path info", "skip current object")("dir path", dir.string())(
                            "error code", ec.value())("error msg", ec.message())("configType", configType));
            continue;
        }
        if (!filesystem::exists(s)) {
            LOG_WARNING(sLogger, ("config dir path not existed", "skip current object")("dir path", dir.string())("configType", configType));
            continue;
        }
        if (!filesystem::is_directory(s)) {
            LOG_WARNING(sLogger,
                        ("config dir path is not a directory", "skip current object")("dir path", dir.string())("configType", configType));
            continue;
        }
        for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
            // lock the dir if it is provided by config provider
            unique_lock<mutex> lock;
            auto itr = configDirMutexMap.find(dir.string());
            if (itr != configDirMutexMap.end()) {
                lock = unique_lock<mutex>(*itr->second, defer_lock);
                lock.lock();
            }

            const filesystem::path& path = entry.path();
            const string& configName = path.stem().string();
            const string& filepath = path.string();
            if (!filesystem::is_regular_file(entry.status(ec))) {
                LOG_DEBUG(sLogger, ("config file is not a regular file", "skip current object")("filepath", filepath)("configType", configType));
                continue;
            }
            if (configSet.find(configName) != configSet.end()) {
                LOG_WARNING(
                    sLogger,
                    ("more than 1 config with the same name is found", "skip current config")("filepath", filepath)("configType", configType));
                continue;
            }
            configSet.insert(configName);

            auto iter = fileInfoMap.find(filepath);
            uintmax_t size = filesystem::file_size(path, ec);
            filesystem::file_time_type mTime = filesystem::last_write_time(path, ec);
            if (iter == fileInfoMap.end()) {
                fileInfoMap[filepath] = make_pair(size, mTime);
                unique_ptr<Json::Value> detail = make_unique<Json::Value>();
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    LOG_INFO(sLogger, ("new config found and disabled", "skip current object")("config", configName)("configType", configType));
                    continue;
                }
                ConfigType config(configName, std::move(detail));
                if (!config.Parse()) {
                    LOG_ERROR(sLogger, ("new config found but invalid", "skip current object")("config", configName)("configType", configType));
                    LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                           "new config found but invalid: skip current object, config: "
                                                               + configName + ", configType: " + configType,
                                                           config.mProject,
                                                           config.mLogstore,
                                                           config.mRegion);
                    continue;
                }
                diff.mAdded.push_back(std::move(config));
                LOG_INFO(
                    sLogger,
                    ("new config found and passed topology check", "prepare to build config")("config", configName)("configType", configType));
            } else if (iter->second.first != size || iter->second.second != mTime) {
                // for config currently running, we leave it untouched if new config is invalid
                fileInfoMap[filepath] = make_pair(size, mTime);
                unique_ptr<Json::Value> detail = make_unique<Json::Value>();
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    if (configManager->FindConfigByName(configName)) {
                        diff.mUnchanged.push_back(configName);
                    }
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    if (configManager->FindConfigByName(configName)) {
                        diff.mRemoved.push_back(configName);
                        LOG_INFO(sLogger,
                                 ("existing valid config modified and disabled",
                                  "prepare to stop current running config")("config", configName)("configType", configType));
                    } else {
                        LOG_INFO(sLogger,
                                 ("existing invalid config modified and disabled", "skip current object")("config",
                                                                                                          configName)("configType", configType));
                    }
                    continue;
                }
                shared_ptr<ManagerConfigType> p = configManager->FindConfigByName(configName);
                if (!p) {
                    ConfigType config(configName, std::move(detail));
                    if (!config.Parse()) {
                        LOG_ERROR(sLogger,
                                  ("existing invalid config modified and remains invalid",
                                   "skip current object")("config", configName)("configType", configType));
                        LogtailAlarm::GetInstance()->SendAlarm(
                            CATEGORY_CONFIG_ALARM,
                            "existing invalid config modified and remains invalid: skip current object, config: "
                                + configName + ", configType: " + configType,
                            config.mProject,
                            config.mLogstore,
                            config.mRegion);
                        continue;
                    }
                    diff.mAdded.push_back(std::move(config));
                    LOG_INFO(sLogger,
                             ("existing invalid config modified and passed topology check",
                              "prepare to build config")("config", configName)("configType", configType));
                } else if (*detail != p->GetConfig()) {
                    ConfigType config(configName, std::move(detail));
                    if (!config.Parse()) {
                        diff.mUnchanged.push_back(configName);
                        LOG_ERROR(sLogger,
                                  ("existing valid config modified and becomes invalid",
                                   "keep current config running")("config", configName)("configType", configType));
                        LogtailAlarm::GetInstance()->SendAlarm(
                            CATEGORY_CONFIG_ALARM,
                            "existing valid config modified and becomes invalid: skip current object, config: "
                                + configName + ", configType: " + configType,
                            config.mProject,
                            config.mLogstore,
                            config.mRegion);
                        continue;
                    }
                    diff.mModified.push_back(std::move(config));
                    LOG_INFO(sLogger,
                             ("existing valid config modified and passed topology check",
                              "prepare to rebuild config")("config", configName)("configType", configType));
                } else {
                    diff.mUnchanged.push_back(configName);
                    LOG_DEBUG(sLogger,
                              ("existing valid config file modified, but no change found", "skip current object")("configType", configType));
                }
            } else {
                // 为了插件系统过渡使用
                if (configManager->FindConfigByName(configName)) {
                    diff.mUnchanged.push_back(configName);
                }
                LOG_DEBUG(sLogger, ("existing config file unchanged", "skip current object")("configType", configType));
            }
        }
    }
    for (const auto& name : configManager->GetAllConfigNames()) {
        if (configSet.find(name) == configSet.end()) {
            diff.mRemoved.push_back(name);
            LOG_INFO(sLogger,
                     ("existing valid config is removed", "prepare to stop current running config")("config", name)("configType", configType));
        }
    }
    for (const auto& item : fileInfoMap) {
        string configName = filesystem::path(item.first).stem().string();
        if (configSet.find(configName) == configSet.end()) {
            fileInfoMap.erase(item.first);
        }
    }

    if (!diff.IsEmpty()) {
        LOG_INFO(sLogger,
                 ("config files scan done", "got updates, begin to update configs")("added", diff.mAdded.size())(
                     "modified", diff.mModified.size())("removed", diff.mRemoved.size())("configType", configType));
    } else {
        LOG_DEBUG(sLogger, ("config files scan done", "no update")("configType", configType));
    }

    return diff;
}

PipelineConfigDiff ConfigWatcher::CheckPipelineConfigDiff() {
    const static std::string configType = "pipelineConfig";
    return CheckConfigDiff<PipelineManager, PipelineConfig, PipelineConfigDiff, Pipeline>(
        mPipelineConfigDir, mPipelineConfigDirMutexMap, mPipelineFileInfoMap, mPipelineManager, configType);
}

ProcessConfigDiff ConfigWatcher::CheckProcessConfigDiff() {
    const static std::string configType = "processConfig";
    return CheckConfigDiff<ProcessConfigManager, ProcessConfig, ProcessConfigDiff, ProcessConfig>(
        mProcessConfigDir, mProcessConfigDirMutexMap, mProcessFileInfoMap, mProcessConfigManager, configType);
}

void ConfigWatcher::AddPipelineSource(const string& dir, mutex* mux) {
    mPipelineConfigDir.emplace_back(dir);
    if (mux != nullptr) {
        mPipelineConfigDirMutexMap[dir] = mux;
    }
}

void ConfigWatcher::AddProcessSource(const string& dir, mutex* mux) {
    mProcessConfigDir.emplace_back(dir);
    if (mux != nullptr) {
        mProcessConfigDirMutexMap[dir] = mux;
    }
}

void ConfigWatcher::AddCommandSource(const string& dir, mutex* mux) {
    mCommandConfigDir.emplace_back(dir);
    if (mux != nullptr) {
        mCommandConfigDirMutexMap[dir] = mux;
    }
}

void ConfigWatcher::ClearEnvironment() {
    mPipelineConfigDir.clear();
    mPipelineFileInfoMap.clear();

    mProcessConfigDir.clear();
    mProcessFileInfoMap.clear();

    mCommandConfigDir.clear();
}

} // namespace logtail
