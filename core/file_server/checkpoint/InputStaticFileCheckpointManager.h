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

#include <cstdint>

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "json/json.h"

#include "file_server/checkpoint/InputStaticFileCheckpoint.h"

namespace logtail {

class InputStaticFileCheckpointManager {
public:
    InputStaticFileCheckpointManager(const InputStaticFileCheckpointManager&) = delete;
    InputStaticFileCheckpointManager& operator=(const InputStaticFileCheckpointManager&) = delete;

    static InputStaticFileCheckpointManager* GetInstance() {
        static InputStaticFileCheckpointManager instance;
        return &instance;
    }

    bool CreateCheckpoint(const std::string& configName,
                          size_t idx,
                          const std::optional<std::vector<std::filesystem::path>>& files = std::nullopt,
                          uint32_t startTime = 0,
                          uint32_t expireTime = 0);
    bool DeleteCheckpoint(const std::string& configName, size_t idx);
    bool UpdateCurrentFileCheckpoint(const std::string& configName, size_t idx, uint64_t offset, uint64_t size);
    bool InvalidateCurrentFileCheckpoint(const std::string& configName, size_t idx);
    bool GetCurrentFileFingerprint(const std::string& configName, size_t idx, FileFingerprint* cpt);

    void DumpAllCheckpointFiles() const;
    void GetAllCheckpointFileNames();
    void ClearUnusedCheckpoints();
    // std::vector<Json::Value> ExportAllCheckpoints();

private:
    InputStaticFileCheckpointManager();
    ~InputStaticFileCheckpointManager() = default;

    bool RetrieveCheckpointFromFile(const std::string& configName, size_t idx, InputStaticFileCheckpoint* cpt);
    // bool DeleteCheckpoint(const std::string& configName, size_t idx);
    bool DumpCheckpointFile(const InputStaticFileCheckpoint& cpt) const;
    bool LoadCheckpointFile(const std::filesystem::path& filepath, InputStaticFileCheckpoint* cpt);

    std::filesystem::path mCheckpointRootPath;

    // accessed by main thread, input runner thread and observabaility thread
    mutable std::mutex mUpdateMux;
    std::map<std::pair<std::string, size_t>, InputStaticFileCheckpoint> mInputCheckpointMap;
    // std::multimap<std::string, size_t> mDeletedInputs;

    // only accessed by main thread
    std::set<std::pair<std::string, size_t>> mCheckpointFileNamesOnInit;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputStaticFileCheckpointManagerUnittest;
    friend class StaticFileServerUnittest;
    friend class InputStaticFileUnittest;
#endif
};

} // namespace logtail
