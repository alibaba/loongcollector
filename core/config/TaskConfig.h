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

#include "config/PipelineConfig.h"

namespace logtail {

struct TaskConfig : public PipelineConfig {
    TaskConfig(const std::string& name, std::unique_ptr<Json::Value>&& detail, const std::filesystem::path& filepath)
        : PipelineConfig(name, std::move(detail), filepath) {}
    TaskConfig(TaskConfig&& rhs) = default;
    TaskConfig& operator=(TaskConfig&& rhs) noexcept = default;

    bool Parse() override;
};

inline bool operator==(const TaskConfig& lhs, const TaskConfig& rhs) {
    return static_cast<const PipelineConfig&>(lhs) == static_cast<const PipelineConfig&>(rhs);
}

inline bool operator!=(const TaskConfig& lhs, const TaskConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
