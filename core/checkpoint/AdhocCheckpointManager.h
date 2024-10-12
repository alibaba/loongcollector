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
#include <unordered_map>
#include "AdhocJobCheckpoint.h"

namespace logtail {

class AdhocCheckpointManager {
private:
    AdhocCheckpointManager() {}
    AdhocCheckpointManager(const AdhocCheckpointManager&) = delete;
    AdhocCheckpointManager& operator=(const AdhocCheckpointManager&) = delete;

    std::unordered_map<std::string, AdhocJobCheckpointPtr> mAdhocJobCheckpointMap;
    int32_t mLastDumpTime = 0;

public:
    static AdhocCheckpointManager* GetInstance() {
        static AdhocCheckpointManager* ptr = new AdhocCheckpointManager();
        return ptr;
    }

    void DumpAdhocCheckpoint();
    void LoadAdhocCheckpoint();

    AdhocJobCheckpointPtr GetAdhocJobCheckpoint(const std::string& jobName);
    AdhocFileCheckpointPtr GetAdhocFileCheckpoint(const std::string& jobName, AdhocFileKey* fileKey);
    AdhocJobCheckpointPtr CreateAdhocJobCheckpoint(const std::string& jobName,
                                                   std::vector<AdhocFileCheckpointPtr>& fileCheckpointList);
    AdhocFileCheckpointPtr CreateAdhocFileCheckpoint(const std::string& jobName, const std::string& filePath);
    void
    UpdateAdhocFileCheckpoint(const std::string& jobName, AdhocFileKey* fileKey, AdhocFileCheckpointPtr fileCheckpoint);
    void DeleteAdhocJobCheckpoint(const std::string& jobName);

    std::string GetJobCheckpointPath(const std::string& jobName);
};

} // namespace logtail