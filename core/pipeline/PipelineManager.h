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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "common/Lock.h"
#include "config/ConfigDiff.h"
#include "pipeline/Pipeline.h"
#include "runner/InputRunner.h"

namespace logtail {

class PipelineManager {
public:
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    static PipelineManager* GetInstance() {
        static PipelineManager instance;
        return &instance;
    }

    void UpdatePipelines(PipelineConfigDiff& diff);
    const std::shared_ptr<Pipeline>& FindConfigByName(const std::string& configName) const;
    std::vector<std::string> GetAllConfigNames() const;
    std::string GetPluginStatistics() const;
    // for shennong only
    const std::unordered_map<std::string, std::shared_ptr<Pipeline>>& GetAllPipelines() const {
        return mPipelineNameEntityMap;
    }
    // 过渡使用
    void StopAllPipelines();

private:
    PipelineManager();
    ~PipelineManager() = default;

    virtual std::shared_ptr<Pipeline> BuildPipeline(PipelineConfig&& config); // virtual for ut
    void IncreasePluginUsageCnt(
        const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& statistics);
    void DecreasePluginUsageCnt(
        const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& statistics);
    void FlushAllBatch();
    // TODO: 长期过渡使用
    bool CheckIfFileServerUpdated(PipelineConfigDiff& diff);

    std::unordered_map<std::string, std::shared_ptr<Pipeline>> mPipelineNameEntityMap;
    mutable SpinLock mPluginCntMapLock;
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> mPluginCntMap;

    std::vector<InputRunner*> mInputRunners;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineManagerMock;
    friend class PipelineManagerUnittest;
    friend class ProcessQueueManagerUnittest;
    friend class ExactlyOnceQueueManagerUnittest;
    friend class BoundedProcessQueueUnittest;
    friend class CircularProcessQueueUnittest;
    friend class CommonConfigProviderUnittest;
    friend class FlusherUnittest;
#endif
};

} // namespace logtail
