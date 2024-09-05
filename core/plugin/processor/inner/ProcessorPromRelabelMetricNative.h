/*
 * Copyright 2024 iLogtail Authors
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

#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "pipeline/plugin/interface/Processor.h"
#include "prometheus/labels/Relabel.h"

namespace logtail {
class ProcessorPromRelabelMetricNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& metricGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    bool ProcessEvent(PipelineEventPtr& e, StringView instance);

    void AddAutoMetrics(PipelineEventGroup& metricGroup);
    void AddMetric(PipelineEventGroup& metricGroup,
                   const std::string& name,
                   double value,
                   time_t timestamp,
                   uint32_t nanoSec,
                   StringView instance);

    std::vector<RelabelConfig> mRelabelConfigs;

    // from config
    std::string mJobName;
    int64_t mScrapeTimeoutSeconds;
    int64_t mSampleLimit;
    int64_t mSeriesLimit;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorPromRelabelMetricNativeUnittest;
    friend class InputPrometheusUnittest;
#endif
};

} // namespace logtail
