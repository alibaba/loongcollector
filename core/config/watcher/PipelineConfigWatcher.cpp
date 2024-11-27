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
    // inner configs
    InsertInnerPipelines(pDiff, tDiff, configSet);
    // configs from file
    InsertPipelines(pDiff, tDiff, configSet);

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

void PipelineConfigWatcher::InsertInnerPipelines(PipelineConfigDiff& pDiff,
                                                 TaskConfigDiff& tDiff,
                                                 unordered_set<string>& configSet) {
    std::map<std::string, std::string> innerPipelines;
    // self-monitor metric
    innerPipelines[LoongCollectorMonitor::GetInnerSelfMonitorMetricPipelineName()]
        = LoongCollectorMonitor::GetInnerSelfMonitorMetricPipeline();

    // process
    for (const auto& pipeline : innerPipelines) {
        if (configSet.find(pipeline.first) != configSet.end()) {
            LOG_WARNING(sLogger,
                        ("more than 1 config with the same name is found", "skip current config")("inner pipeline",
                                                                                                  pipeline.first));
            continue;
        }
        configSet.insert(pipeline.first);

        string errorMsg;
        auto iter = mInnerConfigMap.find(pipeline.first);
        if (iter == mInnerConfigMap.end()) {
            mInnerConfigMap[pipeline.first] = pipeline.second;
            unique_ptr<Json::Value> detail = make_unique<Json::Value>();
            if (!ParseConfigDetail(pipeline.second, ".json", *detail, errorMsg)) {
                LOG_WARNING(sLogger,
                            ("config format error", "skip current object")("error msg", errorMsg)("inner pipeline",
                                                                                                  pipeline.first));
                continue;
            }
            if (!IsConfigEnabled(pipeline.first, *detail)) {
                LOG_INFO(sLogger, ("new config found and disabled", "skip current object")("config", pipeline.first));
                continue;
            }
            if (!CheckAddedConfig(pipeline.first, std::move(detail), pDiff, tDiff)) {
                continue;
            }
        } else if (pipeline.second != iter->second) {
            mInnerConfigMap[pipeline.first] = pipeline.second;
            unique_ptr<Json::Value> detail = make_unique<Json::Value>();
            if (!ParseConfigDetail(pipeline.second, ".json", *detail, errorMsg)) {
                LOG_WARNING(sLogger,
                            ("config format error", "skip current object")("error msg", errorMsg)("inner pipeline",
                                                                                                  pipeline.first));
                continue;
            }
            if (!IsConfigEnabled(pipeline.first, *detail)) {
                switch (GetConfigType(*detail)) {
                    case ConfigType::Pipeline:
                        if (mPipelineManager->FindConfigByName(pipeline.first)) {
                            pDiff.mRemoved.push_back(pipeline.first);
                            LOG_INFO(sLogger,
                                     ("existing valid config modified and disabled",
                                      "prepare to stop current running pipeline")("config", pipeline.first));
                        } else {
                            LOG_INFO(sLogger,
                                     ("existing invalid config modified and disabled",
                                      "skip current object")("config", pipeline.first));
                        }
                        break;
                    case ConfigType::Task:
                        if (mTaskPipelineManager->FindPipelineByName(pipeline.first)) {
                            tDiff.mRemoved.push_back(pipeline.first);
                            LOG_INFO(sLogger,
                                     ("existing valid config modified and disabled",
                                      "prepare to stop current running task")("config", pipeline.first));
                        } else {
                            LOG_INFO(sLogger,
                                     ("existing invalid config modified and disabled",
                                      "skip current object")("config", pipeline.first));
                        }
                        break;
                }
                continue;
            }
            if (!CheckModifiedConfig(pipeline.first, std::move(detail), pDiff, tDiff)) {
                continue;
            }
        } else {
            LOG_DEBUG(sLogger, ("existing inner config unchanged", "skip current object"));
        }
    }
}

void PipelineConfigWatcher::InsertPipelines(PipelineConfigDiff& pDiff,
                                            TaskConfigDiff& tDiff,
                                            std::unordered_set<std::string>& configSet) {
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
                if (!CheckAddedConfig(configName, std::move(detail), pDiff, tDiff)) {
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
                if (!CheckModifiedConfig(configName, std::move(detail), pDiff, tDiff)) {
                    continue;
                }
            } else {
                LOG_DEBUG(sLogger, ("existing config file unchanged", "skip current object"));
            }
        }
    }
}

bool PipelineConfigWatcher::CheckAddedConfig(const string& configName,
                                             unique_ptr<Json::Value>&& configDetail,
                                             PipelineConfigDiff& pDiff,
                                             TaskConfigDiff& tDiff) {
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
            pDiff.mAdded.push_back(std::move(config));
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
                                                TaskConfigDiff& tDiff) {
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
                pDiff.mAdded.push_back(std::move(config));
                LOG_INFO(sLogger,
                         ("existing invalid config modified and passed topology check",
                          "prepare to build pipeline")("config", configName));
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
                pDiff.mModified.push_back(std::move(config));
                LOG_INFO(sLogger,
                         ("existing valid config modified and passed topology check",
                          "prepare to rebuild pipeline")("config", configName));
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

} // namespace logtail
