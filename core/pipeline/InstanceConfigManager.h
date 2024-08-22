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
#include "config/InstanceConfig.h"

namespace logtail {

class InstanceConfigManager {
public:
    InstanceConfigManager(const InstanceConfigManager&) = delete;
    InstanceConfigManager& operator=(const InstanceConfigManager&) = delete;

    static InstanceConfigManager* GetInstance() {
        static InstanceConfigManager instance;
        return &instance;
    }

    void UpdateInstanceConfigs(InstanceConfigDiff& diff);
    std::shared_ptr<InstanceConfig> FindConfigByName(const std::string& configName) const;
    std::vector<std::string> GetAllConfigNames() const;

private:
    InstanceConfigManager();
    ~InstanceConfigManager() = default;
    std::map<std::string, std::shared_ptr<InstanceConfig>> mInstanceConfigMap;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InstanceConfigManagerUnittest;
    friend class CommonConfigProviderUnittest;
#endif
};

} // namespace logtail
