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

#include <memory>
#include <unordered_set>

#include "PipelineConfig.h"
#include "logger/Logger.h"
#include "pipeline/PipelineManager.h"

using namespace std;

namespace logtail {

bool ReadFile(const string& filepath, string& content);

ConfigWatcher::ConfigWatcher() : mPipelineManager(PipelineManager::GetInstance()) {
}

PipelineConfigDiff ConfigWatcher::CheckConfigDiff() {
    PipelineConfigDiff diff;
    unordered_set<string> configSet;
    for (const auto& dir : mSourceDir) {
        error_code ec;
        filesystem::file_status s = filesystem::status(dir, ec);
        if (ec) {
            LOG_WARNING(sLogger,
                        ("failed to get config dir path info", "skip current object")("dir path", dir.string())(
                            "error code", ec.value())("error msg", ec.message()));
            continue;
        }
        if (!filesystem::exists(s)) {
            LOG_WARNING(sLogger, ("config dir path not existed", "skip current object")("dir path", dir.string()));
            continue;
        }
        if (!filesystem::is_directory(s)) {
            LOG_WARNING(sLogger,
                        ("config dir path is not a directory", "skip current object")("dir path", dir.string()));
            continue;
        }
        for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
            // lock the dir if it is provided by config provider
            unique_lock<mutex> lock;
            auto itr = mDirMutexMap.find(dir.string());
            if (itr != mDirMutexMap.end()) {
                lock = unique_lock<mutex>(*itr->second, defer_lock);
                lock.lock();
            }

            const filesystem::path& path = entry.path();
            const string& configName = path.stem().string();
            const string& filepath = path.string();
            if (!filesystem::is_regular_file(entry.status(ec))) {
                LOG_DEBUG(sLogger, ("config file is not a regular file", "skip current object")("filepath", filepath));
                continue;
            }
            if (configSet.find(configName) != configSet.end()) {
                LOG_WARNING(
                    sLogger,
                    ("more than 1 config with the same name is found", "skip current config")("filepath", filepath));
                continue;
            }
            configSet.insert(configName);

            auto iter = mFileInfoMap.find(filepath);
            uintmax_t size = filesystem::file_size(path, ec);
            filesystem::file_time_type mTime = filesystem::last_write_time(path, ec);
            if (iter == mFileInfoMap.end()) {
                mFileInfoMap[filepath] = make_pair(size, mTime);
                unique_ptr<Json::Value> detail = make_unique<Json::Value>(new Json::Value());
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    LOG_INFO(sLogger, ("new config found and disabled", "skip current object")("config", configName));
                    continue;
                }
                PipelineConfig config(configName, std::move(detail));
                if (!config.Parse()) {
                    LOG_ERROR(sLogger, ("new config found but invalid", "skip current object")("config", configName));
                    AlarmManager::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                           "new config found but invalid: skip current object, config: "
                                                               + configName,
                                                           config.mProject,
                                                           config.mLogstore,
                                                           config.mRegion);
                    continue;
                }
                diff.mAdded.push_back(std::move(config));
                LOG_INFO(
                    sLogger,
                    ("new config found and passed topology check", "prepare to build pipeline")("config", configName));
            } else if (iter->second.first != size || iter->second.second != mTime) {
                // for config currently running, we leave it untouched if new config is invalid
                mFileInfoMap[filepath] = make_pair(size, mTime);
                unique_ptr<Json::Value> detail = make_unique<Json::Value>(new Json::Value());
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    if (mPipelineManager->FindConfigByName(configName)) {
                        diff.mRemoved.push_back(configName);
                        LOG_INFO(sLogger,
                                 ("existing valid config modified and disabled",
                                  "prepare to stop current running pipeline")("config", configName));
                    } else {
                        LOG_INFO(sLogger,
                                 ("existing invalid config modified and disabled", "skip current object")("config",
                                                                                                          configName));
                    }
                    continue;
                }
                shared_ptr<Pipeline> p = mPipelineManager->FindConfigByName(configName);
                if (!p) {
                    PipelineConfig config(configName, std::move(detail));
                    if (!config.Parse()) {
                        LOG_ERROR(sLogger,
                                  ("existing invalid config modified and remains invalid",
                                   "skip current object")("config", configName));
                        AlarmManager::GetInstance()->SendAlarm(
                            CATEGORY_CONFIG_ALARM,
                            "existing invalid config modified and remains invalid: skip current object, config: "
                                + configName,
                            config.mProject,
                            config.mLogstore,
                            config.mRegion);
                        continue;
                    }
                    diff.mAdded.push_back(std::move(config));
                    LOG_INFO(sLogger,
                             ("existing invalid config modified and passed topology check",
                              "prepare to build pipeline")("config", configName));
                } else if (*detail != p->GetConfig()) {
                    PipelineConfig config(configName, std::move(detail));
                    if (!config.Parse()) {
                        LOG_ERROR(sLogger,
                                  ("existing valid config modified and becomes invalid",
                                   "keep current pipeline running")("config", configName));
                        AlarmManager::GetInstance()->SendAlarm(
                            CATEGORY_CONFIG_ALARM,
                            "existing valid config modified and becomes invalid: skip current object, config: "
                                + configName,
                            config.mProject,
                            config.mLogstore,
                            config.mRegion);
                        continue;
                    }
                    diff.mModified.push_back(std::move(config));
                    LOG_INFO(sLogger,
                             ("existing valid config modified and passed topology check",
                              "prepare to rebuild pipeline")("config", configName));
                } else {
                    LOG_DEBUG(sLogger,
                              ("existing valid config file modified, but no change found", "skip current object"));
                }
            } else {
                LOG_DEBUG(sLogger, ("existing config file unchanged", "skip current object"));
            }
        }
    }
    for (const auto& name : mPipelineManager->GetAllConfigNames()) {
        if (configSet.find(name) == configSet.end()) {
            diff.mRemoved.push_back(name);
            LOG_INFO(sLogger,
                     ("existing valid config is removed", "prepare to stop current running pipeline")("config", name));
        }
    }
    for (const auto& item : mFileInfoMap) {
        string configName = filesystem::path(item.first).stem().string();
        if (configSet.find(configName) == configSet.end()) {
            mFileInfoMap.erase(item.first);
        }
    }

    if (!diff.IsEmpty()) {
        LOG_INFO(sLogger,
                 ("config files scan done", "got updates, begin to update pipelines")("added", diff.mAdded.size())(
                     "modified", diff.mModified.size())("removed", diff.mRemoved.size()));
    } else {
        LOG_DEBUG(sLogger, ("config files scan done", "no update"));
    }

    return diff;
}

void ConfigWatcher::AddSource(const string& dir, mutex* mux) {
    mSourceDir.emplace_back(dir);
    if (mux != nullptr) {
        mDirMutexMap[dir] = mux;
    }
}

void ConfigWatcher::ClearEnvironment() {
    mSourceDir.clear();
    mFileInfoMap.clear();
}

} // namespace logtail
