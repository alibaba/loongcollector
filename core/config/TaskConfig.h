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

#include <json/json.h>

#include <cstdint>
#include <memory>
#include <string>

namespace logtail {

struct TaskConfig {
    std::string mName;
    std::unique_ptr<Json::Value> mDetail;
    uint32_t mCreateTime = 0;

    TaskConfig(const std::string& name, std::unique_ptr<Json::Value>&& detail)
        : mName(name), mDetail(std::move(detail)) {}

    bool Parse();
};

inline bool operator==(const TaskConfig& lhs, const TaskConfig& rhs) {
    return (lhs.mName == rhs.mName) && (*lhs.mDetail == *rhs.mDetail);
}

inline bool operator!=(const TaskConfig& lhs, const TaskConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
