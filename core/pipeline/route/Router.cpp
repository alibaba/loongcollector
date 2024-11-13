// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/route/Router.h"

#include "common/ParamExtractor.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/Pipeline.h"
#include "pipeline/plugin/interface/Flusher.h"

using namespace std;

namespace logtail {

bool Router::Init(std::vector<pair<size_t, const Json::Value*>> configs, const PipelineContext& ctx) {
    for (auto& item : configs) {
        if (item.second != nullptr) {
            mConditions.emplace_back(item.first, Condition());
            if (!mConditions.back().second.Init(*item.second, ctx)) {
                return false;
            }
        } else {
            mAlwaysMatchedFlusherIdx.push_back(item.first);
        }
    }

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_COMPONENT,
        {{METRIC_LABEL_KEY_PROJECT, ctx.GetProjectName()},
         {METRIC_LABEL_KEY_PIPELINE_NAME, ctx.GetConfigName()},
         {METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_ROUTER}});
    mInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_EVENTS_TOTAL);
    mInGroupDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_SIZE_BYTES);
    return true;
}

vector<pair<size_t, PipelineEventGroup>> Router::Route(PipelineEventGroup& g) const {
    mInEventsTotal->Add(g.GetEvents().size());
    mInGroupDataSizeBytes->Add(g.DataSize());

    vector<size_t> dest;
    for (size_t i = 0; i < mConditions.size(); ++i) {
        if (mConditions[i].second.Check(g)) {
            dest.push_back(i);
        }
    }
    auto resSz = dest.size() + mAlwaysMatchedFlusherIdx.size();

    vector<pair<size_t, PipelineEventGroup>> res;
    res.reserve(resSz);
    for (size_t i = 0; i < mAlwaysMatchedFlusherIdx.size(); ++i, --resSz) {
        if (resSz == 1) {
            res.emplace_back(mAlwaysMatchedFlusherIdx[i], std::move(g));
        } else {
            res.emplace_back(mAlwaysMatchedFlusherIdx[i], g.Copy());
        }
    }
    for (size_t i = 0; i < dest.size(); ++i, --resSz) {
        if (resSz == 1) {
            mConditions[dest[i]].second.GetResult(g);
            res.emplace_back(dest[i], std::move(g));
        } else {
            auto copy = g.Copy();
            mConditions[dest[i]].second.GetResult(copy);
            res.emplace_back(dest[i], std::move(copy));
        }
    }
    return res;
}

} // namespace logtail
