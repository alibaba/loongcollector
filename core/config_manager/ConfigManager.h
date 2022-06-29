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
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <json/json.h>
#include "config_manager/ConfigManagerBase.h"

namespace logtail {

class ConfigManager : public ConfigManagerBase {
public:
    static ConfigManager* GetInstance() {
        static ConfigManager* ptr = new ConfigManager();
        return ptr;
    }

    // @configExistFlag indicates if there are loaded configs before calling.
    void InitUpdateConfig(bool configExistFlag) override;

    /** Read configuration, detect any format errors.
     *
     * @param configFile path for the configuration file
     *
     * @return 0 on success; nonzero on error
     */
    bool LoadConfig(const std::string& configFile) override;

    bool UpdateAccessKey(const std::string& aliuid,
                         std::string& accessKeyId,
                         std::string& accessKey,
                         int32_t& lastUpdateTime) override;

    void CleanUnusedUserAK() override;

    bool GetRegionStatus(const std::string& region) override;

    void SetStartWorkerStatus(const std::string& result, const std::string& message) override;

private:
    ConfigManager();
    virtual ~ConfigManager(); // no copy
    ConfigManager(const ConfigManager&);
    ConfigManager& operator=(const ConfigManager&);

    /**
     * @brief CreateCustomizedFuseConfig, call this after starting, insert it into config map
     * @return
     */
    void CreateCustomizedFuseConfig() override;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcherTest;
    friend class SenderUnittest;
    friend class UtilUnittest;
    friend class ConfigUpdatorUnittest;
    friend class ConfigMatchUnittest;
    friend class FuxiSceneUnittest;
    friend class SymlinkInotifyTest;
    friend class LogFilterUnittest;
    friend class FuseFileUnittest;
    friend class MultiServerConfigUpdatorUnitest;
#endif
};

} // namespace logtail
