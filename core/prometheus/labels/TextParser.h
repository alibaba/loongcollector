/*
 * Copyright 2024 iLogtail Authors
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

#include <re2/re2.h>

#include <string>

#include "models/PipelineEventGroup.h"

namespace logtail {

extern const std::string SAMPLE_RE;

class TextParser {
public:
    TextParser() : mSampleRegex(SAMPLE_RE) {}
    PipelineEventGroup Parse(const std::string& content);


    PipelineEventGroup
    Parse(const std::string& content, std::time_t defaultTs, const std::string& jobName, const std::string& instance);
    bool ParseLine(const std::string& line, MetricEvent& e, time_t defaultTs);

private:
    RE2 mSampleRegex;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TextParserUnittest;
#endif
};

} // namespace logtail
