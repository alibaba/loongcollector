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

namespace logtail::apm {

class PackageManager {
public:
    PackageManager(const PackageManager&) = delete;
    PackageManager(PackageManager&&) = delete;
    PackageManager& operator=(const PackageManager&) = delete;
    PackageManager& operator=(PackageManager&&) = delete;

    explicit PackageManager() = default;

    virtual ~PackageManager() = default;

    void Init() {}


    bool UpdateExecHook();
    bool UninstallExecHook();

    // download and un-zip to target path ...
    bool PrepareAPMAgent(APMLanguage lang, const std::string& version, std::string& agentJarPath);

    bool InstallExecHook();

    // TODO update latest APM Agent ...

private:
    bool downloadFromOss(const std::string& url, const std::string& dir, const std::string& filename, bool& changed);
};

} // namespace logtail::apm
