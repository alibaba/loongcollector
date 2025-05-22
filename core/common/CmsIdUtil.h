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

#include <string>
#include <array>
#include <charconv>

#include "logger/Logger.h"
#include "common/magic_enum.hpp"

namespace logtail {

template<typename T>
inline int Parse(T& val, int base, std::string& res) {
    res.resize(sizeof(T) << 3);
    
    auto [ptr, ec] = std::to_chars(res.data(), res.data() + res.size(), val, base);
    if (ec != std::errc{}) {
        LOG_ERROR(sLogger, ("failed to call to_chars, err", magic_enum::enum_name(ec)));
        return 1;
    }
    res.resize(ptr - res.data());
    return 0;
}

template<typename T>
inline int From(const std::string& val, int base, T& res) {
    auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), res, base);
    if (ec != std::errc{} || ptr != val.data() + val.size()) {
        return 1;
    }
    return 0;
}

int AdaptMD5(std::string& input);

int EncodeUserId(const std::string& userId, std::string& res);

int GenAPMPid(const std::string& userId, const std::string& appName, std::string& res);

bool IsDefaultWorkspace(const std::string& userId, const std::string& regionId, const std::string& workspace);

std::string GetDefaultWorkspace(const std::string& userId, const std::string& regionId);

int GenerateSiteName(const std::string& workspace, const std::string& pid, const std::string& regionId, std::string& res);

int GenerateServiceId(const std::string& userId, const std::string& regionId, const std::string& pid, const std::string& workspace, std::string& res);

int GenerateWorkspaceAPMProject(const std::string& workspace, const std::string& userId, const std::string& regionId, std::string& res);

} // namespace logtail
