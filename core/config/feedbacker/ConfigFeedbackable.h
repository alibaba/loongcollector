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

#include <cstdint>
#include <string>

namespace logtail {

enum class ConfigFeedbackStatus { UNSET = 0, APPLYING = 1, APPLIED = 2, FAILED = 3, DELETED = 4 };

std::string_view ToStringView(ConfigFeedbackStatus status);

class ConfigFeedbackable {
public:
    virtual ~ConfigFeedbackable() = default; // LCOV_EXCL_LINE
    virtual void FeedbackPipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status) = 0;
    virtual void FeedbackProcessConfigStatus(const std::string& name, ConfigFeedbackStatus status) = 0;
    virtual void
    FeedbackCommandConfigStatus(const std::string& type, const std::string& name, ConfigFeedbackStatus status)
        = 0;
};

} // namespace logtail
