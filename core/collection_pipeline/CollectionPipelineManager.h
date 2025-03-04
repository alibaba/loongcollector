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

#include <cstdint>

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "collection_pipeline/CollectionPipeline.h"
#include "config/ConfigDiff.h"
#include "runner/InputRunner.h"

namespace logtail {

class CollectionPipelineManager {
public:
    CollectionPipelineManager(const CollectionPipelineManager&) = delete;
    CollectionPipelineManager& operator=(const CollectionPipelineManager&) = delete;

    static CollectionPipelineManager* GetInstance() {
        static CollectionPipelineManager instance;
        return &instance;
    }

    void RegisterInputRunner(InputRunner* runner) { mInputRunners.push_back(runner); }
    void InputRunnerEventGC() {
        for (auto runner : mInputRunners) {
            runner->EventGC();
        }
    }

    const std::shared_ptr<CollectionPipeline>& FindConfigByName(const std::string& configName) const;
    void UpdatePipelines(CollectionConfigDiff& diff);
    void StopAllPipelines();
    void ClearAllPipelines();
    std::vector<std::string> GetAllConfigNames() const;

    // for shennong only
    const std::unordered_map<std::string, std::shared_ptr<CollectionPipeline>>& GetAllPipelines() const {
        return mPipelineNameEntityMap;
    }

private:
    CollectionPipelineManager() = default;
    ~CollectionPipelineManager() = default;

    virtual std::shared_ptr<CollectionPipeline> BuildPipeline(CollectionConfig&& config); // virtual for ut
    void FlushAllBatch();
    // TODO: 长期过渡使用
    bool CheckIfFileServerUpdated(CollectionConfigDiff& diff);

    mutable std::shared_mutex mPipelineNameEntityMapMutex;
    std::unordered_map<std::string, std::shared_ptr<CollectionPipeline>> mPipelineNameEntityMap;

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
    friend class PipelineUnittest;
#endif
};

} // namespace logtail
