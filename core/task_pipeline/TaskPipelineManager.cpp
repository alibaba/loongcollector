/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "task_pipeline/TaskPipelineManager.h"

#include "config/feedbacker/ConfigFeedbackReceiver.h"

using namespace std;

namespace logtail {

static unique_ptr<TaskPipeline> sEmptyTask;

void TaskPipelineManager::UpdatePipelines(TaskConfigDiff& diff) {
    for (const auto& name : diff.mRemoved) {
        bool isOnetime = false;
        {
            auto iter = mPipelineNameEntityMap.find(name);
            if (iter == mPipelineNameEntityMap.end()) {
                continue;
            }
            isOnetime = iter->second->IsOnetime()
                || ConfigFeedbackReceiver::GetInstance().IsOnetimePipelineConfigRegistered(name);
            iter->second->Stop(true);
            {
                unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
                mPipelineNameEntityMap.erase(iter);
            }
        }
        if (isOnetime) {
            ConfigFeedbackReceiver::GetInstance().FeedbackOnetimePipelineConfigStatus(name,
                                                                                      ConfigFeedbackStatus::DELETED);
        } else {
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(name,
                                                                                         ConfigFeedbackStatus::DELETED);
        }
    }
    for (auto& config : diff.mModified) {
        const string configName = config.mName;
        const bool isOnetimeConfig = config.IsOnetime()
            || ConfigFeedbackReceiver::GetInstance().IsOnetimePipelineConfigRegistered(configName);
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(
                sLogger,
                ("failed to build task for existing config", "keep current task running")("config", configName));
            AlarmManager::GetInstance()->SendAlarmError(
                CATEGORY_CONFIG_ALARM,
                "failed to build task for existing config: keep current task running, config: " + configName);
            if (isOnetimeConfig) {
                ConfigFeedbackReceiver::GetInstance().FeedbackOnetimePipelineConfigStatus(configName,
                                                                                          ConfigFeedbackStatus::FAILED);
            } else {
                ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(configName,
                                                                                             ConfigFeedbackStatus::FAILED);
            }
            continue;
        }
        LOG_INFO(sLogger,
                 ("task building for existing config succeeded",
                  "stop the old task and start the new one")("config", configName));
        bool builtIsOnetime = p->IsOnetime()
            || ConfigFeedbackReceiver::GetInstance().IsOnetimePipelineConfigRegistered(configName);
        {
            auto iter = mPipelineNameEntityMap.find(configName);
            // when pipeline lifespan attribute changes, two pipelines are considered unrelated, and thus the old one
            // should be considered as deleted
            if (iter == mPipelineNameEntityMap.end()) {
                continue;
            }
            iter->second->Stop(builtIsOnetime != iter->second->IsOnetime());
            {
                unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
                mPipelineNameEntityMap[configName] = std::move(p);
            }
            mPipelineNameEntityMap[configName]->Start();
        }
        if (builtIsOnetime) {
            ConfigFeedbackReceiver::GetInstance().FeedbackOnetimePipelineConfigStatus(configName,
                                                                                      ConfigFeedbackStatus::APPLIED);
        } else {
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(configName,
                                                                                         ConfigFeedbackStatus::APPLIED);
        }
    }
    for (auto& config : diff.mAdded) {
        const string configName = config.mName;
        const bool isOnetimeConfig = config.IsOnetime()
            || ConfigFeedbackReceiver::GetInstance().IsOnetimePipelineConfigRegistered(configName);
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build task for new config", "skip current object")("config", configName));
            AlarmManager::GetInstance()->SendAlarmError(
                CATEGORY_CONFIG_ALARM,
                "failed to build task for new config: skip current object, config: " + configName);
            if (isOnetimeConfig) {
                ConfigFeedbackReceiver::GetInstance().FeedbackOnetimePipelineConfigStatus(configName,
                                                                                          ConfigFeedbackStatus::FAILED);
            } else {
                ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(configName,
                                                                                             ConfigFeedbackStatus::FAILED);
            }
            continue;
        }
        LOG_INFO(sLogger, ("task building for new config succeeded", "begin to start task")("config", configName));
        bool builtIsOnetime = p->IsOnetime()
            || ConfigFeedbackReceiver::GetInstance().IsOnetimePipelineConfigRegistered(configName);
        {
            unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
            mPipelineNameEntityMap[configName] = std::move(p);
        }
        mPipelineNameEntityMap[configName]->Start();
        if (builtIsOnetime) {
            ConfigFeedbackReceiver::GetInstance().FeedbackOnetimePipelineConfigStatus(configName,
                                                                                      ConfigFeedbackStatus::APPLIED);
        } else {
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(configName,
                                                                                         ConfigFeedbackStatus::APPLIED);
        }
    }
}

void TaskPipelineManager::StopAllPipelines() {
    unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    for (auto& item : mPipelineNameEntityMap) {
        item.second->Stop(true);
    }
    mPipelineNameEntityMap.clear();
}

const unique_ptr<TaskPipeline>& TaskPipelineManager::FindPipelineByName(const string& configName) const {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    auto it = mPipelineNameEntityMap.find(configName);
    if (it != mPipelineNameEntityMap.end()) {
        return it->second;
    }
    return sEmptyTask;
}

vector<string> TaskPipelineManager::GetAllPipelineNames() const {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    vector<string> res;
    for (const auto& item : mPipelineNameEntityMap) {
        res.push_back(item.first);
    }
    return res;
}

size_t TaskPipelineManager::GetPipelineCount() const {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    return mPipelineNameEntityMap.size();
}

unique_ptr<TaskPipeline> TaskPipelineManager::BuildPipeline(TaskConfig&& config) {
    auto p = make_unique<TaskPipeline>();
    if (!p->Init(std::move(config))) {
        return nullptr;
    }
    return p;
}

} // namespace logtail
