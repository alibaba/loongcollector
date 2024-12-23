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

#include "config/watcher/PipelineConfigWatcher.h"

#include <memory>

#include "common/FileSystemUtil.h"
#include "config/ConfigUtil.h"
#include "config/common_provider/CommonConfigProvider.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#include "pipeline/PipelineManager.h"
#include "task_pipeline/TaskPipelineManager.h"

using namespace std;

namespace logtail {

PipelineConfigWatcher::PipelineConfigWatcher()
    : ConfigWatcher(),
      mPipelineManager(PipelineManager::GetInstance()),
      mTaskPipelineManager(TaskPipelineManager::GetInstance()) {
}

pair<PipelineConfigDiff, TaskConfigDiff> PipelineConfigWatcher::CheckConfigDiff() {
    PipelineConfigDiff pDiff;
    TaskConfigDiff tDiff;
    unordered_set<string> configSet;
    SingletonConfigCache singletonCache;
    // builtin pipeline configs
    InsertBuiltInPipelines(pDiff, tDiff, configSet, singletonCache);
    // file pipeline configs
    InsertPipelines(pDiff, tDiff, configSet, singletonCache);

    CheckSingletonInput(pDiff, singletonCache);
    for (const auto& name : mPipelineManager->GetAllConfigNames()) {
        if (configSet.find(name) == configSet.end()) {
            pDiff.mRemoved.push_back(name);
            LOG_INFO(sLogger,
                     ("existing valid config is removed", "prepare to stop current running pipeline")("config", name));
        }
    }
    for (const auto& name : mTaskPipelineManager->GetAllPipelineNames()) {
        if (configSet.find(name) == configSet.end()) {
            tDiff.mRemoved.push_back(name);
            LOG_INFO(sLogger,
                     ("existing valid config is removed", "prepare to stop current running task")("config", name));
        }
    }
    for (auto it = mFileInfoMap.begin(); it != mFileInfoMap.end();) {
        string configName = filesystem::path(it->first).stem().string();
        if (configSet.find(configName) == configSet.end()) {
            it = mFileInfoMap.erase(it);
        } else {
            ++it;
        }
    }

    if (!pDiff.IsEmpty()) {
        LOG_INFO(sLogger,
                 ("config files scan done", "got updates, begin to update pipelines")("added", pDiff.mAdded.size())(
                     "modified", pDiff.mModified.size())("removed", pDiff.mRemoved.size()));
    } else {
        LOG_DEBUG(sLogger, ("config files scan done", "no pipeline update"));
    }
    if (!tDiff.IsEmpty()) {
        LOG_INFO(sLogger,
                 ("config files scan done", "got updates, begin to update tasks")("added", tDiff.mAdded.size())(
                     "modified", tDiff.mModified.size())("removed", tDiff.mRemoved.size()));
    } else {
        LOG_DEBUG(sLogger, ("config files scan done", "no task update"));
    }

    return make_pair(std::move(pDiff), std::move(tDiff));
}

void PipelineConfigWatcher::InsertBuiltInPipelines(PipelineConfigDiff& pDiff,
                                                   TaskConfigDiff& tDiff,
                                                   unordered_set<string>& configSet,
                                                   SingletonConfigCache& singletonCache) {
#ifdef __ENTERPRISE__
    const std::map<std::string, std::string>& builtInPipelines
        = EnterpriseConfigProvider::GetInstance()->GetAllBuiltInPipelineConfigs();

    for (const auto& pipeline : builtInPipelines) {
        const string& pipelineName = pipeline.first;
        const string& pipleineDetail = pipeline.second;
        if (configSet.find(pipelineName) != configSet.end()) {
            LOG_WARNING(sLogger,
                        ("more than 1 config with the same name is found", "skip current config")("inner pipeline",
                                                                                                  pipelineName));
            continue;
        }
        configSet.insert(pipelineName);

        string errorMsg;
        auto iter = mInnerConfigMap.find(pipelineName);
        if (iter == mInnerConfigMap.end()) {
            mInnerConfigMap[pipelineName] = pipleineDetail;
            unique_ptr<Json::Value> detail = make_unique<Json::Value>();
            if (!ParseConfigDetail(pipleineDetail, ".json", *detail, errorMsg)) {
                LOG_WARNING(sLogger,
                            ("config format error", "skip current object")("error msg", errorMsg)("inner pipeline",
                                                                                                  pipelineName));
                continue;
            }
            if (!IsConfigEnabled(pipelineName, *detail)) {
                LOG_INFO(sLogger, ("new config found and disabled", "skip current object")("config", pipelineName));
                continue;
            }
            if (!CheckAddedConfig(pipelineName, std::move(detail), pDiff, tDiff, singletonCache)) {
                continue;
            }
        } else if (pipleineDetail != iter->second) {
            mInnerConfigMap[pipelineName] = pipleineDetail;
            unique_ptr<Json::Value> detail = make_unique<Json::Value>();
            if (!ParseConfigDetail(pipleineDetail, ".json", *detail, errorMsg)) {
                LOG_WARNING(sLogger,
                            ("config format error", "skip current object")("error msg", errorMsg)("inner pipeline",
                                                                                                  pipelineName));
                continue;
            }
            if (!IsConfigEnabled(pipelineName, *detail)) {
                switch (GetConfigType(*detail)) {
                    case ConfigType::Pipeline:
                        if (mPipelineManager->FindConfigByName(pipelineName)) {
                            pDiff.mRemoved.push_back(pipelineName);
                            LOG_INFO(sLogger,
                                     ("existing valid config modified and disabled",
                                      "prepare to stop current running pipeline")("config", pipelineName));
                        } else {
                            LOG_INFO(sLogger,
                                     ("existing invalid config modified and disabled",
                                      "skip current object")("config", pipelineName));
                        }
                        break;
                    case ConfigType::Task:
                        if (mTaskPipelineManager->FindPipelineByName(pipelineName)) {
                            tDiff.mRemoved.push_back(pipelineName);
                            LOG_INFO(sLogger,
                                     ("existing valid config modified and disabled",
                                      "prepare to stop current running task")("config", pipelineName));
                        } else {
                            LOG_INFO(sLogger,
                                     ("existing invalid config modified and disabled",
                                      "skip current object")("config", pipelineName));
                        }
                        break;
                }
                continue;
            }
            if (!CheckModifiedConfig(pipelineName, std::move(detail), pDiff, tDiff, singletonCache)) {
                continue;
            }
        } else {
            LOG_DEBUG(sLogger, ("existing inner config unchanged", "skip current object"));
        }
    }
#else
    return;
#endif
}

void PipelineConfigWatcher::InsertPipelines(PipelineConfigDiff& pDiff,
                                            TaskConfigDiff& tDiff,
                                            std::unordered_set<std::string>& configSet,
                                            SingletonConfigCache& singletonCache) {
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
                unique_ptr<Json::Value> detail = make_unique<Json::Value>();
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    LOG_INFO(sLogger, ("new config found and disabled", "skip current object")("config", configName));
                    continue;
                }
                if (!CheckAddedConfig(configName, std::move(detail), pDiff, tDiff, singletonCache)) {
                    continue;
                }
            } else if (iter->second.first != size || iter->second.second != mTime) {
                // for config currently running, we leave it untouched if new config is invalid
                mFileInfoMap[filepath] = make_pair(size, mTime);
                unique_ptr<Json::Value> detail = make_unique<Json::Value>();
                if (!LoadConfigDetailFromFile(path, *detail)) {
                    continue;
                }
                if (!IsConfigEnabled(configName, *detail)) {
                    switch (GetConfigType(*detail)) {
                        case ConfigType::Pipeline:
                            if (mPipelineManager->FindConfigByName(configName)) {
                                pDiff.mRemoved.push_back(configName);
                                LOG_INFO(sLogger,
                                         ("existing valid config modified and disabled",
                                          "prepare to stop current running pipeline")("config", configName));
                            } else {
                                LOG_INFO(sLogger,
                                         ("existing invalid config modified and disabled",
                                          "skip current object")("config", configName));
                            }
                            break;
                        case ConfigType::Task:
                            if (mTaskPipelineManager->FindPipelineByName(configName)) {
                                tDiff.mRemoved.push_back(configName);
                                LOG_INFO(sLogger,
                                         ("existing valid config modified and disabled",
                                          "prepare to stop current running task")("config", configName));
                            } else {
                                LOG_INFO(sLogger,
                                         ("existing invalid config modified and disabled",
                                          "skip current object")("config", configName));
                            }
                            break;
                    }
                    continue;
                }
                if (!CheckModifiedConfig(configName, std::move(detail), pDiff, tDiff, singletonCache)) {
                    continue;
                }
            } else {
                LOG_DEBUG(sLogger, ("existing config file unchanged", "skip current object"));
                CheckUnchangedConfig(configName, path, pDiff, tDiff, singletonCache);
            }
        }
    }
}

bool PipelineConfigWatcher::CheckAddedConfig(const string& configName,
                                             unique_ptr<Json::Value>&& configDetail,
                                             PipelineConfigDiff& pDiff,
                                             TaskConfigDiff& tDiff,
                                             SingletonConfigCache& singletonCache) {
    switch (GetConfigType(*configDetail)) {
        case ConfigType::Pipeline: {
            PipelineConfig config(configName, std::move(configDetail));
            if (!config.Parse()) {
                LOG_ERROR(sLogger, ("new config found but invalid", "skip current object")("config", configName));
                AlarmManager::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                       "new config found but invalid: skip current object, config: "
                                                           + configName,
                                                       config.mProject,
                                                       config.mLogstore,
                                                       config.mRegion);
                return false;
            }
            PushPipelineConfig(std::move(config), ConfigDiffEnum::Added, pDiff, singletonCache);
            LOG_INFO(sLogger,
                     ("new config found and passed topology check", "prepare to build pipeline")("config", configName));
            break;
        }
        case ConfigType::Task: {
            TaskConfig config(configName, std::move(configDetail));
            if (!config.Parse()) {
                LOG_ERROR(sLogger, ("new config found but invalid", "skip current object")("config", configName));
                AlarmManager::GetInstance()->SendAlarm(
                    CATEGORY_CONFIG_ALARM, "new config found but invalid: skip current object, config: " + configName);
                return false;
            }
            tDiff.mAdded.push_back(std::move(config));
            LOG_INFO(sLogger,
                     ("new config found and passed topology check", "prepare to build task")("config", configName));
            break;
        }
    }
    return true;
}

bool PipelineConfigWatcher::CheckModifiedConfig(const string& configName,
                                                unique_ptr<Json::Value>&& configDetail,
                                                PipelineConfigDiff& pDiff,
                                                TaskConfigDiff& tDiff,
                                                SingletonConfigCache& singletonCache) {
    switch (GetConfigType(*configDetail)) {
        case ConfigType::Pipeline: {
            shared_ptr<Pipeline> p = mPipelineManager->FindConfigByName(configName);
            if (!p) {
                PipelineConfig config(configName, std::move(configDetail));
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
                    return false;
                }
                LOG_INFO(sLogger,
                         ("existing invalid config modified and passed topology check",
                          "prepare to build pipeline")("config", configName));
                PushPipelineConfig(std::move(config), ConfigDiffEnum::Added, pDiff, singletonCache);
            } else if (*configDetail != p->GetConfig()) {
                PipelineConfig config(configName, std::move(configDetail));
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
                    return false;
                }
                LOG_INFO(sLogger,
                         ("existing valid config modified and passed topology check",
                          "prepare to rebuild pipeline")("config", configName));
                PushPipelineConfig(std::move(config), ConfigDiffEnum::Modified, pDiff, singletonCache);
            } else {
                LOG_DEBUG(sLogger, ("existing valid config file modified, but no change found", "skip current object"));
            }
            break;
        }
        case ConfigType::Task: {
            auto& p = mTaskPipelineManager->FindPipelineByName(configName);
            if (!p) {
                TaskConfig config(configName, std::move(configDetail));
                if (!config.Parse()) {
                    LOG_ERROR(sLogger,
                              ("existing invalid config modified and remains invalid",
                               "skip current object")("config", configName));
                    AlarmManager::GetInstance()->SendAlarm(
                        CATEGORY_CONFIG_ALARM,
                        "existing invalid config modified and remains invalid: skip current object, config: "
                            + configName);
                    return false;
                }
                tDiff.mAdded.push_back(std::move(config));
                LOG_INFO(sLogger,
                         ("existing invalid config modified and passed topology check",
                          "prepare to build task")("config", configName));
            } else if (*configDetail != p->GetConfig()) {
                TaskConfig config(configName, std::move(configDetail));
                if (!config.Parse()) {
                    LOG_ERROR(sLogger,
                              ("existing valid config modified and becomes invalid",
                               "keep current task running")("config", configName));
                    AlarmManager::GetInstance()->SendAlarm(
                        CATEGORY_CONFIG_ALARM,
                        "existing valid config modified and becomes invalid: skip current object, config: "
                            + configName);
                    return false;
                }
                tDiff.mModified.push_back(std::move(config));
                LOG_INFO(sLogger,
                         ("existing valid config modified and passed topology check",
                          "prepare to rebuild task")("config", configName));
            } else {
                LOG_DEBUG(sLogger, ("existing valid config file modified, but no change found", "skip current object"));
            }
            break;
        }
    }
    return true;
}

bool PipelineConfigWatcher::CheckUnchangedConfig(const std::string& configName,
                                                 const filesystem::path& path,
                                                 PipelineConfigDiff& pDiff,
                                                 TaskConfigDiff& tDiff,
                                                 SingletonConfigCache& singletonCache) {
    auto pipeline = mPipelineManager->FindConfigByName(configName);
    auto task = mTaskPipelineManager->FindPipelineByName(configName).get();
    if (task) {
        return true;
    } else if (pipeline) { // running pipeline in last config update
        std::unique_ptr<Json::Value> configDetail = make_unique<Json::Value>();
        PipelineConfig config(configName, std::move(configDetail));
        config.mCreateTime = pipeline->GetContext().GetCreateTime();
        config.mSingletonInput = pipeline->GetSingletonInput();
        PushPipelineConfig(std::move(config), ConfigDiffEnum::Unchanged, pDiff, singletonCache);
    } else { // low priority singleton input in last config update, sort it again
        unique_ptr<Json::Value> detail = make_unique<Json::Value>();
        if (!LoadConfigDetailFromFile(path, *detail)) {
            return false;
        }
        if (!IsConfigEnabled(configName, *detail)) {
            LOG_DEBUG(sLogger, ("unchanged config found and disabled", "skip current object")("config", configName));
            return false;
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
            return false;
        }
        if (config.mSingletonInput) {
            singletonCache[config.mSingletonInput.value()].push_back(
                make_shared<PipelineConfigWithDiffInfo>(std::move(config), ConfigDiffEnum::Added));
        }
    }
    return true;
}

void PipelineConfigWatcher::PushPipelineConfig(PipelineConfig&& config,
                                               ConfigDiffEnum diffEnum,
                                               PipelineConfigDiff& pDiff,
                                               SingletonConfigCache& singletonCache) {
    // singleton input
    if (config.mSingletonInput) {
        if (diffEnum == ConfigDiffEnum::Added || diffEnum == ConfigDiffEnum::Modified
            || diffEnum == ConfigDiffEnum::Unchanged) {
            singletonCache[config.mSingletonInput.value()].push_back(
                make_shared<PipelineConfigWithDiffInfo>(std::move(config), diffEnum));
        } else {
            LOG_ERROR(sLogger, ("should not reach here", "invalid diff enum")("diff", diffEnum));
        }
        return;
    }
    // no singleton input
    switch (diffEnum) {
        case ConfigDiffEnum::Added:
            pDiff.mAdded.push_back(std::move(config));
            break;
        case ConfigDiffEnum::Modified:
            pDiff.mModified.push_back(std::move(config));
            break;
        default:
            break;
    }
}

void PipelineConfigWatcher::CheckSingletonInput(PipelineConfigDiff& pDiff, SingletonConfigCache& singletonCache) {
    for (auto& [name, configs] : singletonCache) {
        std::sort(configs.begin(),
                  configs.end(),
                  [](const std::shared_ptr<PipelineConfigWithDiffInfo>& a,
                     const std::shared_ptr<PipelineConfigWithDiffInfo>& b) -> bool {
                      if (a->config.mCreateTime == b->config.mCreateTime) {
                          return a->config.mName < b->config.mName;
                      }
                      return a->config.mCreateTime < b->config.mCreateTime;
                  });
        for (size_t i = 0; i < configs.size(); ++i) {
            const auto& diffEnum = configs[i]->diffEnum;
            const auto& configName = configs[i]->config.mName;
            if (i == 0) {
                switch (diffEnum) {
                    // greatest priority config
                    case ConfigDiffEnum::Added:
                        LOG_INFO(sLogger,
                                 ("new config with singleton input found and passed topology check",
                                  "prepare to build pipeline")("config", configName));
                        pDiff.mAdded.push_back(std::move(configs[0]->config));
                        break;
                    case ConfigDiffEnum::Modified:
                        LOG_INFO(sLogger,
                                 ("existing config with singleton input modified and passed topology check",
                                  "prepare to build pipeline")("config", configName));
                        pDiff.mModified.push_back(std::move(configs[0]->config));
                        break;
                    default:
                        break;
                }
            } else {
                // other low priority configs
                switch (diffEnum) {
                    case ConfigDiffEnum::Modified:
                        LOG_WARNING(sLogger,
                                    ("global singleton plugin found, but another older config or smaller name config "
                                     "already exists",
                                     "skip current object")("config", configName));
                        pDiff.mRemoved.push_back(configName);
                        break;
                    case ConfigDiffEnum::Unchanged:
                        LOG_WARNING(sLogger,
                                    ("existing valid config with global singleton plugin, but another older config or "
                                     "smaller name config found",
                                     "prepare to stop current running pipeline")("config", configName));
                        pDiff.mRemoved.push_back(configName);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

} // namespace logtail
