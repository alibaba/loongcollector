/*
 * Copyright 2023 iLogtail Authors
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

#include <unordered_set>

#include "config/ConfigDiff.h"
#include "config/watcher/ConfigWatcher.h"

namespace logtail {

class PipelineManager;
class TaskPipelineManager;

class PipelineConfigWatcher : public ConfigWatcher {
public:
    PipelineConfigWatcher(const PipelineConfigWatcher&) = delete;
    PipelineConfigWatcher& operator=(const PipelineConfigWatcher&) = delete;

    static PipelineConfigWatcher* GetInstance() {
        static PipelineConfigWatcher instance;
        return &instance;
    }

    std::pair<PipelineConfigDiff, TaskConfigDiff> CheckConfigDiff();

#ifdef APSARA_UNIT_TEST_MAIN
    void SetPipelineManager(const PipelineManager* pm) { mPipelineManager = pm; }
#endif

private:
    PipelineConfigWatcher();
    ~PipelineConfigWatcher() = default;

    void InsertBuiltInPipelines(PipelineConfigDiff& pDiff, TaskConfigDiff& tDiff, std::unordered_set<std::string>& configSet);
    void InsertPipelines(PipelineConfigDiff& pDiff, TaskConfigDiff& tDiff, std::unordered_set<std::string>& configSet);
    bool CheckAddedConfig(const std::string& configName,
                          std::unique_ptr<Json::Value>&& configDetail,
                          PipelineConfigDiff& pDiff,
                          TaskConfigDiff& tDiff);
    bool CheckModifiedConfig(const std::string& configName,
                             std::unique_ptr<Json::Value>&& configDetail,
                             PipelineConfigDiff& pDiff,
                             TaskConfigDiff& tDiff);

    const PipelineManager* mPipelineManager = nullptr;
    const TaskPipelineManager* mTaskPipelineManager = nullptr;
};

} // namespace logtail
