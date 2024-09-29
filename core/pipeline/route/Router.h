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

#include <json/json.h>

#include <optional>
#include <vector>

#include "models/PipelineEventGroup.h"
#include "monitor/LogtailMetric.h"
#include "pipeline/route/Condition.h"

namespace logtail {

class Flusher;

class Router {
public:
    bool Init(std::vector<std::pair<size_t, const Json::Value*>> config, const PipelineContext& ctx);
    std::vector<size_t> Route(const PipelineEventGroup& g) const;

private:
    std::vector<std::pair<size_t, Condition>> mConditions;
    std::vector<size_t> mAlwaysMatchedFlusherIdx;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInEventsTotal;
    CounterPtr mInGroupDataSizeBytes;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class RouterUnittest;
    friend class PipelineUnittest;
#endif
};

} // namespace logtail
