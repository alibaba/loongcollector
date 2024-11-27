/*
 * Copyright 2024 iLogtail Authors
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

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace logtail {

class ConfigWatcher {
public:
    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

    void AddSource(const std::string& dir, std::mutex* mux = nullptr);

#ifdef APSARA_UNIT_TEST_MAIN
    void ClearEnvironment();
#endif

protected:
    ConfigWatcher() = default;
    virtual ~ConfigWatcher() = default;

    std::vector<std::filesystem::path> mSourceDir;
    std::map<std::string, std::mutex*> mDirMutexMap;
    std::map<std::string, std::pair<uintmax_t, std::filesystem::file_time_type>> mFileInfoMap;
    std::map<std::string, std::string> mInnerConfigMap;
};

} // namespace logtail
