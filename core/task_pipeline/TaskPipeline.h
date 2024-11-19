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

#pragma once

#include <json/json.h>

#include <memory>
#include <string>

#include "config/TaskConfig.h"
#include "task_pipeline/Task.h"

namespace logtail {

class TaskPipeline {
public:
    const std::string& Name() const { return mConfigName; }
    bool Init(TaskConfig&& config);
    void Start() { mPlugin->Start(); }
    void Stop(bool isRemoving) { mPlugin->Stop(isRemoving); }
    const Json::Value& GetConfig() const { return *mConfig; }

#ifdef APSARA_UNIT_TEST_MAIN
    Task* GetPlugin() const { return mPlugin.get(); }
#endif

private:
    std::unique_ptr<Task> mPlugin;

    std::string mConfigName;
    std::unique_ptr<Json::Value> mConfig;
    uint32_t mCreateTime = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TaskPipelineUnittest;
#endif
};

} // namespace logtail
