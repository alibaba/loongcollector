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

#include <string>
#include <vector>

#include "common/StringView.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"

namespace logtail {

class FormattedString {
public:
    FormattedString() = default;
    ~FormattedString() = default;

    bool Init(const std::string& formatString);

    bool IsDynamic() const { return !mPlaceholderNames.empty(); }
    const std::string& GetTemplate() const { return mTemplate; }
    const std::vector<std::string>& GetRequiredKeys() const { return mRequiredKeys; }

    bool Format(const PipelineEventPtr& event, const GroupTags& groupTags, std::string& result) const;

private:
    bool ParseFormatString(const std::string& formatString);

private:
    std::string mTemplate;
    std::vector<StringView> mStaticParts;
    std::vector<StringView> mPlaceholderNames;
    std::vector<std::string> mRequiredKeys;
};

} // namespace logtail
