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

#include <json/json.h>
#include <re2/re2.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace logtail {

struct InstanceConfig {
    std::string mConfigName;
    std::string mDirName;
    Json::Value mDetail;

    InstanceConfig(const std::string& name, const Json::Value& detail, const std::string& dirName)
        : mConfigName(name), mDirName(dirName), mDetail(detail) {}

    const Json::Value& GetConfig() const { return mDetail; }
};

} // namespace logtail
