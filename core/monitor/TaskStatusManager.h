/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <mutex>
#include <string>

#include "models/PipelineEventGroup.h"

namespace logtail {

extern const std::string& TASK_TYPE_STATIC_FILE;

class TaskStatusManager {
public:
    static TaskStatusManager* GetInstance() {
        static TaskStatusManager instance;
        return &instance;
    }

    LogEvent* AddTaskStatus(const std::string& taskType, const std::string& region);
    void FlushTaskStatus(std::vector<PipelineEventGroup>& pipelineEventGroupList);

private:
    TaskStatusManager() = default;
    ~TaskStatusManager() = default;

    std::mutex mMutex;
    std::map<std::string, PipelineEventGroup> mTaskStatusMap;
};

} // namespace logtail
