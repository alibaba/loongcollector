/*
 * Copyright 2023 iLogtail Authors
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

#include "processor/Processor.h"
#include <string>

namespace logtail {

class ProcessorFillSlsGroupInfo : public Processor {
public:
    static const char* Name() { return "processor_fill_sls_group_info"; }
    bool Init(const ComponentConfig& config) override;
    void Process(PipelineEventGroup& logGroup) override;

private:
    std::string GetTopicName(const std::string& path, std::vector<sls_logs::LogTag>& extraTags);

    std::string mTopicFormat, mGroupTopic, mStaticTopic;
    bool mIsStaticTopic = false;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorFillSlsGroupInfoUnittest;
#endif
};
} // namespace logtail