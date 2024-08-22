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

#include <filesystem>
#include <mutex>
#include <string>

namespace logtail {

class ConfigProvider {
public:
    ConfigProvider(const ConfigProvider&) = delete;
    ConfigProvider& operator=(const ConfigProvider&) = delete;

    virtual void Init(const std::string& dir);
    virtual void Stop() = 0;

protected:
    ConfigProvider() = default;
    virtual ~ConfigProvider() = default;

    std::filesystem::path mPipelineSourceDir;
    std::filesystem::path mInstanceSourceDir;
    mutable std::mutex mPipelineMux;
    mutable std::mutex mInstanceMux;
};

} // namespace logtail
