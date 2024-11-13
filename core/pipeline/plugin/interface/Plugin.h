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

#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/PipelineContext.h"

namespace logtail {

class Plugin {
public:
    virtual ~Plugin() = default;

    virtual const std::string& Name() const = 0;

    PipelineContext& GetContext() const { return *mContext; }
    bool HasContext() const { return mContext != nullptr; }
    void SetContext(PipelineContext& context) { mContext = &context; }
    MetricsRecordRef& GetMetricsRecordRef() const { return mMetricsRecordRef; }
    void SetMetricsRecordRef(const std::string& name, const std::string& id) {
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mMetricsRecordRef,
            MetricCategory::METRIC_CATEGORY_PLUGIN,
            {{METRIC_LABEL_KEY_PROJECT, mContext->GetProjectName()},
             {METRIC_LABEL_KEY_PIPELINE_NAME, mContext->GetConfigName()},
             {METRIC_LABEL_KEY_PLUGIN_TYPE, name},
             {METRIC_LABEL_KEY_PLUGIN_ID, id}});
    }

protected:
    PipelineContext* mContext = nullptr;
    mutable MetricsRecordRef mMetricsRecordRef;
};

} // namespace logtail
