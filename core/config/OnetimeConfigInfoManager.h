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
#include <string>

#include "config/ConfigUtil.h"

namespace logtail {

enum class OnetimeConfigStatus {
    NEW,
    OLD,
    OBSOLETE,
};

class OnetimeConfigInfoManager {
public:
    OnetimeConfigInfoManager(const OnetimeConfigInfoManager&) = delete;
    OnetimeConfigInfoManager& operator=(const OnetimeConfigInfoManager&) = delete;

    static OnetimeConfigInfoManager* GetInstance() {
        static OnetimeConfigInfoManager instance;
        return &instance;
    }

    OnetimeConfigStatus
    GetOnetimeConfigStatusFromCheckpoint(const std::string& configName, uint64_t hash, uint32_t* expireTime);
    bool UpdateConfig(const std::string& configName,
                      ConfigType type,
                      const std::filesystem::path& filepath,
                      uint64_t hash,
                      uint32_t expireTime);
    bool RemoveConfig(const std::string& configName);
    void DeleteTimeoutConfigFiles();
    bool LoadCheckpointFile();
    void DumpCheckpointFile() const;
    void ClearUnusedCheckpoints();

#ifdef APSARA_UNIT_TEST_MAIN
    void Clear();
#endif

private:
    struct ConfigInfo {
        ConfigType mType;
        std::filesystem::path mFilepath;
        uint64_t mHash;
        uint32_t mExpireTime;

        ConfigInfo(ConfigType type, const std::filesystem::path& filepath, uint64_t hash, uint32_t expireTime)
            : mType(type), mFilepath(filepath), mHash(hash), mExpireTime(expireTime) {}
    };

    OnetimeConfigInfoManager();
    ~OnetimeConfigInfoManager() = default;

    std::filesystem::path mCheckpointFilePath;

    // only accessed by main thread, however, for protection, we still add a lock
    mutable std::mutex mMux;
    std::map<std::string, ConfigInfo> mConfigInfoMap;
    std::map<std::string, std::pair<uint64_t, uint32_t>> mConfigExpireTimeCheckpoint;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineConfigUnittest;
    friend class OnetimeConfigInfoManagerUnittest;
    friend class OnetimeConfigUpdateUnittest;
#endif
};

} // namespace logtail
