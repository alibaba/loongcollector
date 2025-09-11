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

#include <functional>
#include <string>
#include <vector>

namespace logtail {

using ValueProvider = std::function<std::string(const std::string& placeholder)>;

struct PlaceholderInfo {
    std::string name; // The placeholder name (e.g., "content.application" or "ENV_VAR")
    std::string prefix; // The prefix type ("%{" or "${")
    size_t position; // Position in the static parts array
};

class DynamicStringFormatter {
public:
    DynamicStringFormatter();
    ~DynamicStringFormatter();

    bool Init(const std::string& formatString);
    bool Format(const ValueProvider& valueProvider, std::string& result) const;
    bool IsDynamic() const { return !mPlaceholders.empty(); }

    const std::string& GetFormatTemplate() const { return mFormatTemplate; }
    const std::vector<PlaceholderInfo>& GetPlaceholders() const { return mPlaceholders; }

private:
    std::string mFormatTemplate;
    std::vector<PlaceholderInfo> mPlaceholders;
    std::vector<std::string> mStaticParts;

    bool ParseFormatString(const std::string& formatString);
};

} // namespace logtail
