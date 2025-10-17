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

#include <memory>
#include <string>

#include "json/json.h"

#include "config/TaskConfig.h"
#include "task_pipeline/Task.h"
#include "task_pipeline/TaskPipelineContext.h"

namespace logtail {

class TaskPipeline {
public:
    const std::string& Name() const { return mName; }
    bool Init(TaskConfig&& config);
    void Start();
    void Stop(bool isRemoving);
    const Json::Value& GetConfig() const { return *mConfig; }
    bool IsOnetime() const { return mIsOnetime; }

#ifdef APSARA_UNIT_TEST_MAIN
    Task* GetPlugin() const { return mPlugin.get(); }
#endif

private:
    std::unique_ptr<Task> mPlugin;

    std::string mName;
    bool mIsOnetime = false;
    std::unique_ptr<Json::Value> mConfig;
    uint32_t mCreateTime = 0;
    mutable TaskPipelineContext mContext;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TaskPipelineUnittest;
#endif
};

} // namespace logtail
