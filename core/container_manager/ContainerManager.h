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
#include "runner/ProcessorRunner.h"


namespace logtail {
class ContainerManager {
public:
    ContainerManager();
    ~ContainerManager();
    static ContainerManager* GetInstance();
    void Init();
    void Stop();

    // 由于获取容器信息可能耗时较大，这里通过mLastUpdateTime是否更新过判断是否成功拉取过一次容器信息
    bool IsReady() { return mIsRunning.load() && mLastUpdateTime.load() > 0; }

    // file server
    void ApplyFileServerContainerDiffs();
    bool CheckFileServerContainerDiffs();
    void GetFileServerContainerStoppedEvents(std::vector<Event*>& eventVec);
    // static file server
    void ApplyStaticFileServerContainerDiffs();
    bool CheckStaticFileServerContainerDiffs();
    void GetStaticFileServerContainerStoppedEvents();

    // Persist/restore container runtime state
    void SaveContainerInfo();
    void LoadContainerInfo();

    MatchedContainerInfo CreateMatchedContainerInfo(const std::string& configName,
                                                    size_t inputIndex,
                                                    const FileDiscoveryOptions* options,
                                                    const CollectionPipelineContext* ctx,
                                                    const std::vector<std::string>& pathExistContainerIDs,
                                                    const std::vector<std::string>& pathNotExistContainerIDs);

    void UpdateMatchedContainerInfoPipeline(CollectionPipelineContext* ctx, size_t inputIndex);
    void RemoveMatchedContainerInfoPipeline();

private:
    void pollingLoop();
    void refreshAllContainersSnapshot();
    void incrementallyUpdateContainersSnapshot();

    void applyContainerDiffForOneConfig(const std::string& configName,
                                        size_t inputIndex,
                                        FileDiscoveryOptions* options,
                                        const CollectionPipelineContext* ctx,
                                        std::shared_ptr<MatchedContainerInfo> configResult);
    bool checkContainerDiffForOneConfig(const std::string& configName,
                                        size_t inputIndex,
                                        FileDiscoveryOptions* options,
                                        const CollectionPipelineContext* ctx);
    void getContainerStoppedEventForOneConfig(const std::string& configName,
                                              size_t inputIndex,
                                              const std::string& containerID,
                                              const FileDiscoveryConfig& config,
                                              std::vector<Event*>* eventVec = nullptr);
    void updateContainerInfoPointersInAllConfigs();
    void updateContainerInfoPointersForContainers(const std::vector<std::string>& containerIDs);
    void
    computeMatchedContainersDiff(std::set<std::string>& fullContainerIDList,
                                 const std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>>& matchList,
                                 const ContainerFilters& filters,
                                 bool isStdio,
                                 ContainerDiff& diff);

    std::map<std::pair<std::string, size_t>, FileDiscoveryConfig> getAllFileDiscoveryConfigs();
    void loadContainerInfoFromDetailFormat(const Json::Value& root, const std::string& configPath);
    void loadContainerInfoFromContainersFormat(const Json::Value& root, const std::string& configPath);

    void sendMatchedContainerInfo(std::vector<std::shared_ptr<MatchedContainerInfo>> configResults);
    void sendAllMatchedContainerInfo();

    // Helper method for joining container IDs
    std::string joinContainerIDs(const std::vector<std::string>& containerIDs);

    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> mContainerMap;
    std::map<std::pair<std::string, size_t>, std::shared_ptr<ContainerDiff>> mConfigContainerDiffMap;
    std::map<std::pair<std::string, size_t>, std::shared_ptr<MatchedContainerInfo>> mConfigContainerResultMap;
    std::mutex mContainerMapMutex;
    std::vector<std::string> mStoppedContainerIDs;
    std::mutex mStoppedContainerIDsMutex;

    std::atomic<uint32_t> mLastUpdateTime{0};
    std::future<void> mThreadRes;

    std::atomic<bool> mIsRunning{false};
    friend class ContainerManagerUnittest;

    mutable ReadWriteLock mMatchedContainerInfoPipelineMux;
    CollectionPipelineContext* mMatchedContainerInfoPipelineCtx = nullptr;
    size_t mMatchedContainerInfoInputIndex = 0;
};

} // namespace logtail
