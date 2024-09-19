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
#include "monitor/MetricConstants.h"
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

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(mMetricsRecordRef,
                                                         {{METRIC_LABEL_PROJECT, ctx.GetProjectName()},
                                                          {METRIC_LABEL_CONFIG_NAME, ctx.GetConfigName()},
                                                          {METRIC_LABEL_KEY_COMPONENT_NAME, "router"}});
    mInEventsCnt = mMetricsRecordRef.CreateCounter(METRIC_IN_EVENTS_CNT);
    mInGroupDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_IN_EVENT_GROUP_SIZE_BYTES);
    return true;
}

vector<size_t> Router::Route(const PipelineEventGroup& g) const {
    mInEventsCnt->Add(g.GetEvents().size());
    mInGroupDataSizeBytes->Add(g.DataSize());

    vector<size_t> res(mAlwaysMatchedFlusherIdx);
    for (size_t i = 0; i < mConditions.size(); ++i) {
        if (mConditions[i].second.Check(g)) {
            res.push_back(i);
        }
    }
    return res;
}

} // namespace logtail
