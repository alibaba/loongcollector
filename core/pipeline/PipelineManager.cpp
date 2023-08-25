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

#include "pipeline/PipelineManager.h"

#include "config_manager/ConfigManager.h"

namespace logtail {

PipelineManager::~PipelineManager() {
}

PipelineManager* PipelineManager::GetInstance() {
    static PipelineManager instance;
    return &instance;
}

bool PipelineManager::LoadAllPipelines() {
    auto& allConfig = ConfigManager::GetInstance()->GetAllConfig();
    for (auto& kv : allConfig) {
        auto p = std::make_shared<Pipeline>();
        if (p->Init(*kv.second)) {
            mPipelineDict.emplace(kv.first, p);
        } else {
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "pipeline " + kv.second->mConfigName + " init failed",
                                                   kv.second->GetProjectName(),
                                                   kv.second->GetCategory(),
                                                   kv.second->mRegion);
            LOG_WARNING(sLogger,
                        ("pipeline init failed", kv.second->mConfigName)("project", kv.second->GetProjectName())(
                            "logstore", kv.second->GetCategory())("region", kv.second->mRegion));
        }
    }
    return true;
}

bool PipelineManager::RemoveAllPipelines() {
    mPipelineDict.clear();
    return true;
}

std::shared_ptr<Pipeline> PipelineManager::FindPipelineByName(const std::string& configName) {
    auto it = mPipelineDict.find(configName);
    if (it != mPipelineDict.end()) {
        return it->second;
    }
    return nullptr;
}
} // namespace logtail
