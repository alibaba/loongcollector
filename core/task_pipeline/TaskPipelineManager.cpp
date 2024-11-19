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
        auto iter = mPipelineNameEntityMap.find(name);
        iter->second->Stop(true);
        mPipelineNameEntityMap.erase(iter);
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(name, ConfigFeedbackStatus::DELETED);
    }
    for (auto& config : diff.mModified) {
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(
                sLogger,
                ("failed to build task for existing config", "keep current task running")("config", config.mName));
            AlarmManager::GetInstance()->SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "failed to build task for existing config: keep current task running, config: " + config.mName);
            ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName,
                                                                               ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger,
                 ("task building for existing config succeeded",
                  "stop the old task and start the new one")("config", config.mName));
        auto iter = mPipelineNameEntityMap.find(config.mName);
        iter->second->Stop(false);
        mPipelineNameEntityMap[config.mName] = std::move(p);
        mPipelineNameEntityMap[config.mName]->Start();
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName, ConfigFeedbackStatus::APPLIED);
    }
    for (auto& config : diff.mAdded) {
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build task for new config", "skip current object")("config", config.mName));
            AlarmManager::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "failed to build task for new config: skip current object, config: "
                                                       + config.mName);
            ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName,
                                                                               ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger, ("task building for new config succeeded", "begin to start task")("config", config.mName));
        mPipelineNameEntityMap[config.mName] = std::move(p);
        mPipelineNameEntityMap[config.mName]->Start();
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName, ConfigFeedbackStatus::APPLIED);
    }
}

void TaskPipelineManager::StopAllPipelines() {
    for (auto& item : mPipelineNameEntityMap) {
        item.second->Stop(true);
    }
    mPipelineNameEntityMap.clear();
}

const unique_ptr<TaskPipeline>& TaskPipelineManager::FindPipelineByName(const string& configName) const {
    auto it = mPipelineNameEntityMap.find(configName);
    if (it != mPipelineNameEntityMap.end()) {
        return it->second;
    }
    return sEmptyTask;
}

vector<string> TaskPipelineManager::GetAllPipelineNames() const {
    vector<string> res;
    for (const auto& item : mPipelineNameEntityMap) {
        res.push_back(item.first);
    }
    return res;
}

unique_ptr<TaskPipeline> TaskPipelineManager::BuildPipeline(TaskConfig&& config) {
    auto p = make_unique<TaskPipeline>();
    if (!p->Init(std::move(config))) {
        return nullptr;
    }
    return p;
}

} // namespace logtail
