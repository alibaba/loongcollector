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

#pragma once

#include <json/json.h>

#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "pipeline/plugin/interface/Plugin.h"

namespace logtail {

class Processor : public Plugin {
public:
    virtual ~Processor() {}

    virtual bool Init(const Json::Value& config) = 0;
    virtual void Process(std::vector<PipelineEventGroup>& logGroupList);

protected:
    virtual bool IsSupportedEvent(const PipelineEventPtr& e) const = 0;
    virtual void Process(PipelineEventGroup& logGroup) = 0;
};

} // namespace logtail
