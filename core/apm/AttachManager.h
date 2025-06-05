/*
 * Copyright 2025 iLogtail Authors
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

#include "apm/Types.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include <filesystem>

namespace logtail::apm {

/**
 * AttachManager is used to add attach config to target process.
 */
class AttachManager {
public:
    AttachManager(const AttachManager&) = delete;
    AttachManager(AttachManager&&) = delete;
    AttachManager& operator=(const AttachManager&) = delete;
    AttachManager& operator=(AttachManager&&) = delete;

    explicit AttachManager() = default;

    virtual ~AttachManager() = default;

    /**
     * DoAttach is used to write APP META to rc file
     */
    bool DoAttach(MatchRule& rule, const std::string& agentPath, AttachConfig& config);

    // bool DoDetach(AttachConfig& config, int pid);
#ifdef APSARA_UNIT_TEST_MAIN
    friend class AttachManagerUnittest;
#endif
private:
    int prepareRuntimeConfig(const std::filesystem::path& cwd, 
                           const std::string& agentJarPath,
                           const std::string& licenseKey,
                           const std::string& appName);
};

} // namespace logtail::apm
