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
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineConfig.h"
#include "pipeline/PipelineContext.h"
#include "monitor/LogtailMetric.h"


namespace logtail {

class ProcessorInstance;
class Processor {
public:
    virtual ~Processor() {}
    void SetContext(PipelineContext& context) { mContext = &context; }
    PipelineContext& GetContext() { return *mContext; }
    MetricsRecordRef& GetMetricsRecordRef() { return mMetricsRecordRef; }
    virtual bool Init(const ComponentConfig& config) = 0;
    virtual void Process(PipelineEventGroup& logGroup) = 0;

protected:
    virtual bool IsSupportedEvent(const PipelineEventPtr& e) = 0;
    PipelineContext* mContext = nullptr;
    MetricsRecordRef mMetricsRecordRef;

    void SetMetricsRecordRef(std::string name, std::string id) {
        std::vector<std::pair<std::string, std::string>> labels;
        WriteMetrics::GetInstance()->PreparePluginCommonLabels(GetContext().GetProjectName(),
                                                           GetContext().GetLogstoreName(),
                                                           GetContext().GetRegion(),
                                                           GetContext().GetConfigName(),
                                                           name,
                                                           id,
                                                           labels);

        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(mMetricsRecordRef, std::move(labels));
    }
};
} // namespace logtail