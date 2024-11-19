// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "task_pipeline/TaskRegistry.h"

using namespace std;

namespace logtail {

void TaskRegistry::LoadPlugins() {
    // RegisterCreator(TaskMock::sName, []() { return make_unique<TaskMock>(); });
}

void TaskRegistry::UnloadPlugins() {
    mPluginDict.clear();
}

unique_ptr<Task> TaskRegistry::CreateTask(const std::string& name) {
    auto it = mPluginDict.find(name);
    if (it == mPluginDict.end()) {
        return nullptr;
    }
    return it->second();
}

bool TaskRegistry::IsValidPlugin(const std::string& name) const {
    return mPluginDict.find(name) != mPluginDict.end();
}

void TaskRegistry::RegisterCreator(const std::string& name, std::function<std::unique_ptr<Task>()>&& creator) {
    mPluginDict[name] = std::move(creator);
}

} // namespace logtail
