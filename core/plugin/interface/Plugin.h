/*
 * Copyright 2023 iLogtail Authors
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
#include <utility>
#include <vector>

#include "pipeline/PipelineContext.h"
#include "monitor/LogtailMetric.h"

namespace logtail {

class Plugin {
public:
    virtual ~Plugin() = default;

    virtual const std::string& Name() const = 0;
    PipelineContext& GetContext() { return *mContext; }
    void SetContext(PipelineContext& context) { mContext = &context; }
    MetricsRecordRef& GetMetricsRecordRef() { return mMetricsRecordRef; }
    void SetMetricsRecordRef(const std::string& name, const std::string& id) {
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

protected:
    PipelineContext* mContext = nullptr;
    MetricsRecordRef mMetricsRecordRef;
};

} // namespace logtail
