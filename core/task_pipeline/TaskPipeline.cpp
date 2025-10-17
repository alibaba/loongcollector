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

#include "task_pipeline/TaskPipeline.h"

#include "config/OnetimeConfigInfoManager.h"
#include "task_pipeline/TaskRegistry.h"

using namespace std;

namespace logtail {

bool TaskPipeline::Init(TaskConfig&& config) {
    mName = config.mName;
    mIsOnetime = config.mOnetimeExpireTime.has_value();
    mCreateTime = config.mCreateTime;
    mConfig = std::move(config.mDetail);

    const auto& detail = (*mConfig)["task"];
    mPlugin = TaskRegistry::GetInstance()->CreateTask(detail["Type"].asString());
    if (!mPlugin->Init(detail)) {
        return false;
    }
    if (mIsOnetime) {
        OnetimeConfigInfoManager::GetInstance()->UpdateConfig(
            mName, ConfigType::Collection, config.mFilePath, config.mConfigHash, config.mOnetimeExpireTime.value());
    }
    return true;
}

void TaskPipeline::Start() {
    mPlugin->Start();
}

void TaskPipeline::Stop(bool isRemoving) {
    mPlugin->Stop(isRemoving);

    // only valid for onetime config
    // for update, the old expire has been replaced by the new one on init, should not remove here
    if (mIsOnetime && isRemoving) {
        OnetimeConfigInfoManager::GetInstance()->RemoveConfig(mName);
    }
}

} // namespace logtail
