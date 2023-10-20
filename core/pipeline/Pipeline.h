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
#include "plugin/instance/ProcessorInstance.h"
#include "pipeline/PipelineConfig.h"
#include "pipeline/PipelineContext.h"

namespace logtail {

class Pipeline {
public:
    Pipeline() {}
    const std::string& Name() const { return mName; }
    bool Init(const PipelineConfig& config);
    void Process(PipelineEventGroup&& logGroup, std::vector<PipelineEventGroup>& logGroupList);
    PipelineContext& GetContext() { return mContext; }
    PipelineConfig& GetPipelineConfig() { return mConfig; }

private:
    bool InitAndAddProcessor(std::unique_ptr<ProcessorInstance>&& processor, const PipelineConfig& config);

    std::string mName;
    std::vector<std::unique_ptr<ProcessorInstance> > mProcessorLine;
    PipelineContext mContext;
    PipelineConfig mConfig;
};
} // namespace logtail