/*
 * Copyright 2025 iLogtail Authors
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

#include <condition_variable>
#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/FileTagOptions.h"
#include "file_server/MultilineOptions.h"
#include "file_server/reader/FileReaderOptions.h"
#include "file_server/reader/LogFileReader.h"
#include "runner/InputRunner.h"

namespace logtail {

class StaticFileServer : public InputRunner {
public:
    StaticFileServer(const StaticFileServer&) = delete;
    StaticFileServer& operator=(const StaticFileServer&) = delete;

    static StaticFileServer* GetInstance() {
        static StaticFileServer sInstance;
        return &sInstance;
    }

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;
    void ClearUnusedCheckpoints() override;

    void AddInput(const std::string& configName,
                  size_t idx,
                  const std::optional<std::vector<std::filesystem::path>>& files,
                  FileDiscoveryOptions* fileDiscoveryOpts,
                  const FileReaderOptions* fileReaderOpts,
                  const MultilineOptions* multilineOpts,
                  const FileTagOptions* fileTagOpts,
                  const CollectionPipelineContext* ctx);
    void RemoveInput(const std::string& configName, size_t idx);

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    StaticFileServer() = default;
    ~StaticFileServer() = default;

    void Run();
    void ReadFiles();
    void UpdateInputs();
    LogFileReaderPtr GetNextAvailableReader(const std::string& configName, size_t idx);

    FileDiscoveryConfig GetFileDiscoveryConfig(const std::string& name, size_t idx) const;
    FileReaderConfig GetFileReaderConfig(const std::string& name, size_t idx) const;
    MultilineConfig GetMultilineConfig(const std::string& name, size_t idx) const;
    FileTagConfig GetFileTagConfig(const std::string& name, size_t idx) const;

    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mStopCV;

    time_t mStartTime = 0;
    bool mIsUnusedCheckpointsCleared = false;

    std::multimap<std::string, std::pair<size_t, LogFileReaderPtr>> mPipelineNameReadersMap;

    // accessed by main thread and input runner thread
    mutable std::mutex mUpdateMux;
    std::map<std::pair<std::string, size_t>, FileDiscoveryConfig> mInputFileDiscoveryConfigsMap;
    std::map<std::pair<std::string, size_t>, FileReaderConfig> mInputFileReaderConfigsMap;
    std::map<std::pair<std::string, size_t>, MultilineConfig> mInputMultilineConfigsMap;
    std::map<std::pair<std::string, size_t>, FileTagConfig> mInputFileTagConfigsMap;
    std::multimap<std::string, size_t> mAddedInputs;
    std::set<std::pair<std::string, size_t>> mDeletedInputs;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class StaticFileServerUnittest;
    friend class InputStaticFileUnittest;
#endif
};

} // namespace logtail
