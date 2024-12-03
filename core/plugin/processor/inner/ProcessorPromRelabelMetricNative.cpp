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
#include "plugin/processor/inner/ProcessorPromRelabelMetricNative.h"

#include <json/json.h>

#include <cstddef>

#include "common/Flags.h"
#include "common/StringTools.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "prometheus/Constants.h"

using namespace std;
DECLARE_FLAG_STRING(_pod_name_);
namespace logtail {

const string ProcessorPromRelabelMetricNative::sName = "processor_prom_relabel_metric_native";

// only for inner processor
bool ProcessorPromRelabelMetricNative::Init(const Json::Value& config) {
    std::string errorMsg;
    mScrapeConfigPtr = std::make_unique<ScrapeConfig>();
    if (!mScrapeConfigPtr->InitStaticConfig(config)) {
        return false;
    }

    mLoongCollectorScraper = STRING_FLAG(_pod_name_);

    return true;
}

void ProcessorPromRelabelMetricNative::Process(PipelineEventGroup& eGroup) {
    // if mMetricRelabelConfigs is empty and honor_labels is true, skip it
    auto targetTags = eGroup.GetTags();
    auto toDelete = GetToDeleteTargetLabels(targetTags);

    if (!mScrapeConfigPtr->mMetricRelabelConfigs.Empty() || !targetTags.empty()) {
        EventsContainer& events = eGroup.MutableEvents();
        size_t wIdx = 0;
        for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
            if (ProcessEvent(events[rIdx], targetTags, toDelete)) {
                if (wIdx != rIdx) {
                    events[wIdx] = std::move(events[rIdx]);
                }
                ++wIdx;
            }
        }
        events.resize(wIdx);
    }

    // delete mTags when key starts with __
    for (const auto& k : toDelete) {
        eGroup.DelTag(k);
    }

    if (eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_TOTAL)) {
        auto autoMetric = prom::AutoMetric();
        UpdateAutoMetrics(eGroup, autoMetric);
        AddAutoMetrics(eGroup, autoMetric);
    }


    // delete all tags
    for (const auto& [k, v] : targetTags) {
        eGroup.DelTag(k);
    }
}

bool ProcessorPromRelabelMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<MetricEvent>();
}

bool ProcessorPromRelabelMetricNative::ProcessEvent(PipelineEventPtr& e,
                                                    const GroupTags& targetTags,
                                                    const vector<StringView>& toDelete) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<MetricEvent>();

    for (const auto& [k, v] : targetTags) {
        if (sourceEvent.HasTag(k)) {
            if (!mScrapeConfigPtr->mHonorLabels) {
                // metric event labels is secondary
                // if confiliction, then rename it exported_<label_name>
                auto key = prometheus::EXPORTED_PREFIX + k.to_string();
                auto b = sourceEvent.GetSourceBuffer()->CopyString(key);
                sourceEvent.SetTagNoCopy(StringView(b.data, b.size), sourceEvent.GetTag(k));
                sourceEvent.SetTagNoCopy(k, v);
            }
        } else {
            sourceEvent.SetTagNoCopy(k, v);
        }
    }

    vector<string> toDeleteInRelabel;
    if (!mScrapeConfigPtr->mMetricRelabelConfigs.Empty()
        && !mScrapeConfigPtr->mMetricRelabelConfigs.Process(sourceEvent, toDeleteInRelabel)) {
        return false;
    }
    // set metricEvent name
    sourceEvent.SetNameNoCopy(sourceEvent.GetTag(prometheus::NAME));

    for (const auto& k : toDelete) {
        sourceEvent.DelTag(k);
    }
    for (const auto& k : toDeleteInRelabel) {
        sourceEvent.DelTag(k);
    }

    // set metricEvent name
    sourceEvent.SetTagNoCopy(prometheus::NAME, sourceEvent.GetName());

    return true;
}

vector<StringView> ProcessorPromRelabelMetricNative::GetToDeleteTargetLabels(const GroupTags& targetTags) const {
    // delete tag which starts with __
    vector<StringView> toDelete;
    for (const auto& [k, v] : targetTags) {
        if (k.starts_with("__")) {
            toDelete.push_back(k);
        }
    }
    return toDelete;
}

void ProcessorPromRelabelMetricNative::UpdateAutoMetrics(const PipelineEventGroup& eGroup,
                                                         prom::AutoMetric& autoMetric) const {
    if (eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION)) {
        autoMetric.mScrapeDurationSeconds
            = StringTo<double>(eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION).to_string());
    }
    if (eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE)) {
        autoMetric.mScrapeResponseSizeBytes
            = StringTo<uint64_t>(eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE).to_string());
    }
    autoMetric.mScrapeSamplesLimit = mScrapeConfigPtr->mSampleLimit;
    if (eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED)) {
        autoMetric.mScrapeSamplesScraped
            = StringTo<uint64_t>(eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED).to_string());
    }
    autoMetric.mScrapeTimeoutSeconds = mScrapeConfigPtr->mScrapeTimeoutSeconds;
    if (eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE)) {
        autoMetric.mUp = StringTo<bool>(eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE).to_string());
    }
}

void ProcessorPromRelabelMetricNative::AddAutoMetrics(PipelineEventGroup& eGroup,
                                                      const prom::AutoMetric& autoMetric) const {
    auto targetTags = eGroup.GetTags();
    if (!eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC)) {
        LOG_ERROR(sLogger, ("scrape_timestamp_milliseconds is not set", ""));
        return;
    }

    StringView scrapeTimestampMilliSecStr = eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC);
    auto timestampMilliSec = StringTo<uint64_t>(scrapeTimestampMilliSecStr.to_string());
    auto timestamp = timestampMilliSec / 1000;
    auto nanoSec = timestampMilliSec % 1000 * 1000000;


    AddMetric(
        eGroup, prometheus::SCRAPE_DURATION_SECONDS, autoMetric.mScrapeDurationSeconds, timestamp, nanoSec, targetTags);

    AddMetric(eGroup,
              prometheus::SCRAPE_RESPONSE_SIZE_BYTES,
              autoMetric.mScrapeResponseSizeBytes,
              timestamp,
              nanoSec,
              targetTags);

    if (autoMetric.mScrapeSamplesLimit > 0) {
        AddMetric(
            eGroup, prometheus::SCRAPE_SAMPLES_LIMIT, autoMetric.mScrapeSamplesLimit, timestamp, nanoSec, targetTags);
    }

    // AddMetric(eGroup,
    //           prometheus::SCRAPE_SAMPLES_POST_METRIC_RELABELING,
    //           autoMetric.mPostRelabel,
    //           timestamp,
    //           nanoSec,
    //           targetTags);

    AddMetric(
        eGroup, prometheus::SCRAPE_SAMPLES_SCRAPED, autoMetric.mScrapeSamplesScraped, timestamp, nanoSec, targetTags);

    AddMetric(
        eGroup, prometheus::SCRAPE_TIMEOUT_SECONDS, autoMetric.mScrapeTimeoutSeconds, timestamp, nanoSec, targetTags);

    // up metric must be the last one
    AddMetric(eGroup, prometheus::UP, 1.0 * autoMetric.mUp, timestamp, nanoSec, targetTags);
}

void ProcessorPromRelabelMetricNative::AddMetric(PipelineEventGroup& metricGroup,
                                                 const string& name,
                                                 double value,
                                                 time_t timestamp,
                                                 uint32_t nanoSec,
                                                 const GroupTags& targetTags) const {
    auto* metricEvent = metricGroup.AddMetricEvent(true);
    metricEvent->SetName(name);
    metricEvent->SetValue<UntypedSingleValue>(value);
    metricEvent->SetTimestamp(timestamp, nanoSec);
    metricEvent->SetTag(prometheus::NAME, name);
    for (const auto& [k, v] : targetTags) {
        metricEvent->SetTag(k, v);
    }
}

} // namespace logtail
