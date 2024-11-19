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
#include <unordered_map>
#include <vector>

#include "config/ConfigDiff.h"
#include "config/TaskConfig.h"
#include "task_pipeline/TaskPipeline.h"

namespace logtail {

class TaskPipelineManager {
public:
    TaskPipelineManager(const TaskPipelineManager&) = delete;
    TaskPipelineManager& operator=(const TaskPipelineManager&) = delete;

    static TaskPipelineManager* GetInstance() {
        static TaskPipelineManager instance;
        return &instance;
    }

    void UpdatePipelines(TaskConfigDiff& diff);
    void StopAllPipelines();
    const std::unique_ptr<TaskPipeline>& FindPipelineByName(const std::string& configName) const;
    std::vector<std::string> GetAllPipelineNames() const;

#ifdef APSARA_UNIT_TEST_MAIN
    void ClearEnvironment() { mPipelineNameEntityMap.clear(); }
#endif

private:
    TaskPipelineManager() = default;
    ~TaskPipelineManager() = default;

    std::unique_ptr<TaskPipeline> BuildPipeline(TaskConfig&& config);

    std::unordered_map<std::string, std::unique_ptr<TaskPipeline>> mPipelineNameEntityMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TaskPipelineManagerUnittest;
#endif
};

} // namespace logtail
