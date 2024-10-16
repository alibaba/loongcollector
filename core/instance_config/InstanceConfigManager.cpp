/*
 * Copyright 2023 iLogtail Authors
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

#include "instance_config/InstanceConfigManager.h"

#include "app_config/AppConfig.h"
#include "config/feedbacker/ConfigFeedbackReceiver.h"

using namespace std;

namespace logtail {

InstanceConfigManager::InstanceConfigManager() = default;

void InstanceConfigManager::UpdateInstanceConfigs(InstanceConfigDiff& diff) {
    for (auto& config : diff.mAdded) {
        mInstanceConfigMap[config.mConfigName] = 
            std::make_shared<InstanceConfig>(config.mConfigName, std::move(config.mDetail), config.mDirName);
        ConfigFeedbackReceiver::GetInstance().FeedbackInstanceConfigStatus(config.mConfigName, ConfigFeedbackStatus::APPLIED);
    }
    for (auto& config : diff.mModified) {
        mInstanceConfigMap[config.mConfigName] = 
            std::make_shared<InstanceConfig>(config.mConfigName, std::move(config.mDetail), config.mDirName);
        ConfigFeedbackReceiver::GetInstance().FeedbackInstanceConfigStatus(config.mConfigName, ConfigFeedbackStatus::APPLIED);
    }
    for (const auto& configName : diff.mRemoved) {
        mInstanceConfigMap.erase(configName);
        ConfigFeedbackReceiver::GetInstance().FeedbackInstanceConfigStatus(configName, ConfigFeedbackStatus::DELETED);
    }
    AppConfig::GetInstance()->LoadInstanceConfig(mInstanceConfigMap);
}

std::shared_ptr<InstanceConfig> InstanceConfigManager::FindConfigByName(const string& configName) const {
    auto it = mInstanceConfigMap.find(configName);
    if (it != mInstanceConfigMap.end()) {
        return it->second;
    }
    return nullptr;
}

vector<string> InstanceConfigManager::GetAllConfigNames() const {
    vector<string> res;
    res.reserve(mInstanceConfigMap.size());
    for (const auto& item : mInstanceConfigMap) {
        res.push_back(item.first);
    }
    return res;
}

} // namespace logtail
