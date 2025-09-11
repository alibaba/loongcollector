/*
 * Copyright 2022 iLogtail Authors
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

#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "runner/ProcessorRunner.h"
#include "constants/TagConstants.h"
#include "container_manager/ContainerDiff.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "file_server/ContainerInfo.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/event/Event.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/Monitor.h"
#include "monitor/SelfMonitorServer.h"


namespace logtail {

struct ContainerConfigResult {
    std::string DataType;
    std::string Project;
    std::string Logstore;
    std::string ConfigName;
    std::string PathNotExistInputContainerIDs;
    std::string PathExistInputContainerIDs;
    std::string SourceAddress;
    std::string InputType;
    std::string InputIsContainerFile;
    std::string FlusherType;
    std::string FlusherTargetAddress;
};

class ContainerManager {
public:
    ContainerManager();
    ~ContainerManager();
    static ContainerManager* GetInstance();
    void Init();
    void Stop();

    void ApplyContainerDiffs();
    bool CheckContainerDiffForAllConfig();

    void GetContainerStoppedEvents(std::vector<Event*>& eventVec);
    // Persist/restore container runtime state
    void SaveContainerInfo();
    void LoadContainerInfo();

    /**
     * @brief Create a container config result similar to input_docker_stdout plugin
     * @param options FileDiscoveryOptions pointer
     * @param ctx CollectionPipelineContext pointer
     * @param containerIDs List of container IDs (optional)
     * @return ContainerConfigResult containing config result data
     */
    ContainerConfigResult CreateContainerConfigResult(
        const FileDiscoveryOptions* options,
        const CollectionPipelineContext* ctx,
        const std::vector<std::string>& containerIDs = {});

    void UpdateConfigContainerInfoPipeline(CollectionPipelineContext* ctx, size_t inputIndex);
    void RemoveConfigContainerInfoPipeline();

private:
    void pollingLoop();
    void refreshAllContainersSnapshot();
    void incrementallyUpdateContainersSnapshot();

    bool checkContainerDiffForOneConfig(FileDiscoveryOptions* options, const CollectionPipelineContext* ctx);
    void
    computeMatchedContainersDiff(std::set<std::string>& fullContainerIDList,
                                 const std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>>& matchList,
                                 const ContainerFilters& filters,
                                 ContainerDiff& diff);

    void loadContainerInfoFromDetailFormat(const Json::Value& root, const std::string& configPath);
    void loadContainerInfoFromContainersFormat(const Json::Value& root, const std::string& configPath);

    void sendConfigContainerInfo();

    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> mContainerMap;
    std::unordered_map<std::string, std::shared_ptr<ContainerDiff>> mConfigContainerDiffMap;
    std::unordered_map<std::string, ContainerConfigResult> mConfigContainerResultMap;
    std::mutex mContainerMapMutex;
    std::vector<std::string> mStoppedContainerIDs;
    std::mutex mStoppedContainerIDsMutex;

    uint32_t mLastUpdateTime = 0;
    std::future<void> mThreadRes;

    std::atomic<bool> mIsRunning{false};
    friend class ContainerManagerUnittest;

    mutable ReadWriteLock mConfigContainerInfoPipelineMux;
    CollectionPipelineContext* mConfigContainerInfoPipelineCtx = nullptr;
    size_t mConfigContainerInfoInputIndex = 0;
};

} // namespace logtail
