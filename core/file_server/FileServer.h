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

#include <string>
#include <unordered_map>
#include <utility>

#include "file_server/FileDiscoveryOptions.h"
#include "file_server/MultilineOptions.h"
#include "pipeline/PipelineContext.h"
#include "reader/FileReaderOptions.h"

namespace logtail {

class FileServer {
public:
    FileServer(const FileServer&) = delete;
    FileServer& operator=(const FileServer&) = delete;

    static FileServer* GetInstance() {
        static FileServer instance;
        return &instance;
    }

    void Start();
    void Pause(bool isConfigUpdate = true);

    FileDiscoveryConfig GetFileDiscoveryConfig(const std::string& name) const;
    const std::unordered_map<std::string, FileDiscoveryConfig>& GetAllFileDiscoveryConfigs() const {
        return mPipelineNameFileDiscoveryConfigsMap;
    }
    void AddFileDiscoveryConfig(const std::string& name, FileDiscoveryOptions* opts, const PipelineContext* ctx);
    void RemoveFileDiscoveryConfig(const std::string& name);

    FileReaderConfig GetFileReaderConfig(const std::string& name) const;
    const std::unordered_map<std::string, FileReaderConfig>& GetAllFileReaderConfigs() const {
        return mPipelineNameFileReaderConfigsMap;
    }
    void AddFileReaderConfig(const std::string& name, const FileReaderOptions* opts, const PipelineContext* ctx);
    void RemoveFileReaderConfig(const std::string& name);

    MultilineConfig GetMultilineConfig(const std::string& name) const;
    const std::unordered_map<std::string, MultilineConfig>& GetAllMultilineConfigs() const {
        return mPipelineNameMultilineConfigsMap;
    }
    void AddMultilineConfig(const std::string& name, const MultilineOptions* opts, const PipelineContext* ctx);
    void RemoveMultilineConfig(const std::string& name);

    void SaveContainerInfo(const std::string& pipeline, const std::shared_ptr<std::vector<ContainerInfo>>& info);
    std::shared_ptr<std::vector<ContainerInfo>> GetAndRemoveContainerInfo(const std::string& pipeline);
    void ClearContainerInfo();
    // 过渡使用
    void Resume(bool isConfigUpdate = true);
    void Stop();
    uint32_t GetExactlyOnceConcurrency(const std::string& name) const;
    std::vector<std::string> GetExactlyOnceConfigs() const;
    void AddExactlyOnceConcurrency(const std::string& name, uint32_t concurrency);
    void RemoveExactlyOnceConcurrency(const std::string& name);

private:
    FileServer() = default;
    ~FileServer() = default;

    void PauseInner();

    std::unordered_map<std::string, FileDiscoveryConfig> mPipelineNameFileDiscoveryConfigsMap;
    std::unordered_map<std::string, FileReaderConfig> mPipelineNameFileReaderConfigsMap;
    std::unordered_map<std::string, MultilineConfig> mPipelineNameMultilineConfigsMap;
    std::unordered_map<std::string, std::shared_ptr<std::vector<ContainerInfo>>> mAllContainerInfoMap;
    // 过渡使用
    std::unordered_map<std::string, uint32_t> mPipelineNameEOConcurrencyMap;
};

} // namespace logtail
