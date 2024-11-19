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

#include "task_pipeline/TaskRegistry.h"

using namespace std;

namespace logtail {

bool TaskPipeline::Init(TaskConfig&& config) {
    mConfigName = config.mName;
    mCreateTime = config.mCreateTime;
    mConfig = std::move(config.mDetail);

    const auto& detail = (*mConfig)["task"];
    mPlugin = TaskRegistry::GetInstance()->CreateTask(detail["Type"].asString());
    return mPlugin->Init(detail);
}

} // namespace logtail
