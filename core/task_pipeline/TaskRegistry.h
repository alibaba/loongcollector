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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "task_pipeline/Task.h"

namespace logtail {

class TaskRegistry {
public:
    TaskRegistry(const TaskRegistry&) = delete;
    TaskRegistry& operator=(const TaskRegistry&) = delete;

    static TaskRegistry* GetInstance() {
        static TaskRegistry instance;
        return &instance;
    }

    void LoadPlugins();
    void UnloadPlugins();
    std::unique_ptr<Task> CreateTask(const std::string& name);
    bool IsValidPlugin(const std::string& name) const;

private:
    TaskRegistry() = default;
    ~TaskRegistry() = default;

    void RegisterCreator(const std::string& name, std::function<std::unique_ptr<Task>()>&& creator);

    std::unordered_map<std::string, std::function<std::unique_ptr<Task>()>> mPluginDict;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TaskConfigUnittest;
    friend void LoadTaskMock();
#endif
};

} // namespace logtail
