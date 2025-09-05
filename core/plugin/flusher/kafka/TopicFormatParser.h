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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/PipelineEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"

namespace logtail {

enum class FieldType { CONTENT, TAG, ENV };

struct FieldRef {
    FieldType type;
    std::string fieldName;
};

class TopicFormatParser {
public:
    TopicFormatParser();
    ~TopicFormatParser();

    bool Init(const std::string& formatString);
    bool FormatTopic(const PipelineEventPtr& event, std::string& result, const GroupTags& groupTags) const;

    const std::vector<FieldRef>& GetRequiredFields() const { return mRequiredFields; }
    bool IsDynamic() const { return !mRequiredFields.empty(); }

private:
    std::string mFormatTemplate;
    std::vector<FieldRef> mRequiredFields;
    std::vector<std::string> mStaticParts;
    std::vector<size_t> mPlaceholderPositions;

    std::string
    ExtractFieldValue(const PipelineEventPtr& event, const FieldRef& fieldRef, const GroupTags& groupTags) const;
    bool ParseFormatString(const std::string& formatString);
};

} // namespace logtail
