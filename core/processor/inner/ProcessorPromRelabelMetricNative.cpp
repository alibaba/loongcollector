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
#include "processor/inner/ProcessorPromRelabelMetricNative.h"

#include <json/json.h>

#include <cstddef>

#include "common/StringTools.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

using namespace std;
namespace logtail {

const string ProcessorPromRelabelMetricNative::sName = "processor_prom_relabel_metric_native";

// only for inner processor
bool ProcessorPromRelabelMetricNative::Init(const Json::Value& config) {
    std::string errorMsg;
    if (!mScrapeConfigPtr->Init(config)) {
        return false;
    }
    if (config.isMember(prometheus::METRIC_RELABEL_CONFIGS) && config[prometheus::METRIC_RELABEL_CONFIGS].isArray()
        && config[prometheus::METRIC_RELABEL_CONFIGS].size() > 0) {
        for (const auto& item : config[prometheus::METRIC_RELABEL_CONFIGS]) {
            mRelabelConfigs.emplace_back(item);
            if (!mRelabelConfigs.back().Validate()) {
                errorMsg = "metric_relabel_configs is invalid";
                LOG_ERROR(sLogger, ("init prometheus processor failed", errorMsg));
                return false;
            }
        }
    }


    if (config.isMember(prometheus::JOB_NAME) && config[prometheus::JOB_NAME].isString()) {
        mJobName = config[prometheus::JOB_NAME].asString();
    } else {
        return false;
    }

    if (config.isMember(prometheus::HONOR_LABELS) && config[prometheus::HONOR_LABELS].isBool()) {
        mHonorLabels = config[prometheus::HONOR_LABELS].asBool();
    } else {
        mHonorLabels = false;
    }

    if (config.isMember(prometheus::SCRAPE_TIMEOUT) && config[prometheus::SCRAPE_TIMEOUT].isString()) {
        string tmpScrapeTimeoutString = config[prometheus::SCRAPE_TIMEOUT].asString();
        mScrapeTimeoutSeconds = DurationToSecond(tmpScrapeTimeoutString);
    } else {
        mScrapeTimeoutSeconds = 10;
    }
    if (config.isMember(prometheus::SAMPLE_LIMIT) && config[prometheus::SAMPLE_LIMIT].isInt64()) {
        mSampleLimit = config[prometheus::SAMPLE_LIMIT].asInt64();
    } else {
        mSampleLimit = -1;
    }
    if (config.isMember(prometheus::SERIES_LIMIT) && config[prometheus::SERIES_LIMIT].isInt64()) {
        mSeriesLimit = config[prometheus::SERIES_LIMIT].asInt64();
    } else {
        mSeriesLimit = -1;
    }

    return true;
}

void ProcessorPromRelabelMetricNative::Process(PipelineEventGroup& metricGroup) {
    EventsContainer& events = metricGroup.MutableEvents();

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(events[rIdx], metricGroup)) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);

    AddAutoMetrics(metricGroup);
}

bool ProcessorPromRelabelMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<MetricEvent>();
}

bool ProcessorPromRelabelMetricNative::ProcessEvent(PipelineEventPtr& e, PipelineEventGroup& metricGroup) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<MetricEvent>();

    if (!mScrapeConfigPtr->mMetricRelabelConfigs.Process(sourceEvent)) {
        return false;
    }

    // set metricEvent name
    if (!sourceEvent.GetTag(prometheus::NAME).empty()) {
        sourceEvent.SetNameNoCopy(sourceEvent.GetTag(prometheus::NAME));
    } else {
        LOG_ERROR(sLogger, ("metric relabel", "metric event name is empty"));
        return false;
    }

    // if mHonorLabels is true, then keep sourceEvent labels, and when serializing sourceEvent labels are primary
    // labels.
    if (!mHonorLabels) {
        // metric event labels is secondary
        // if confiliction, then rename it exported_<label_name>
        for (const auto& [k, v] : metricGroup.GetTags()) {
            if (sourceEvent.HasTag(k)) {
                sourceEvent.SetTag("exported_" + k.to_string(), sourceEvent.GetTag(k).to_string());
                sourceEvent.DelTag(k);
            }
        }
    }
    return true;
}

void ProcessorPromRelabelMetricNative::AddAutoMetrics(PipelineEventGroup& metricGroup) {
    // if up is set, then add self monitor metrics
    if (metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE).empty()) {
        return;
    }

    StringView scrapeTimestampMilliSecStr
        = metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC);
    auto timestampMilliSec = StringTo<uint64_t>(scrapeTimestampMilliSecStr.to_string());
    auto timestamp = timestampMilliSec / 1000;
    auto nanoSec = timestampMilliSec % 1000 * 1000000;

    auto instance = metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_INSTANCE);

    uint64_t samplesPostMetricRelabel = metricGroup.GetEvents().size();

    auto scrapeDurationSeconds
        = StringTo<double>(metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION).to_string());

    AddMetric(metricGroup, prometheus::SCRAPE_DURATION_SECONDS, scrapeDurationSeconds, timestamp, nanoSec, instance);

    auto scrapeResponseSize
        = StringTo<uint64_t>(metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE).to_string());
    AddMetric(metricGroup, prometheus::SCRAPE_RESPONSE_SIZE_BYTES, scrapeResponseSize, timestamp, nanoSec, instance);

    if (mSampleLimit > 0) {
        AddMetric(metricGroup, prometheus::SCRAPE_SAMPLES_LIMIT, mSampleLimit, timestamp, nanoSec, instance);
    }

    AddMetric(metricGroup,
              prometheus::SCRAPE_SAMPLES_POST_METRIC_RELABELING,
              samplesPostMetricRelabel,
              timestamp,
              nanoSec,
              instance);

    auto samplesScraped
        = StringTo<uint64_t>(metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED).to_string());

    AddMetric(metricGroup, prometheus::SCRAPE_SAMPLES_SCRAPED, samplesScraped, timestamp, nanoSec, instance);

    AddMetric(metricGroup, prometheus::SCRAPE_TIMEOUT_SECONDS, mScrapeTimeoutSeconds, timestamp, nanoSec, instance);

    // up metric must be the last one
    bool upState = StringTo<bool>(metricGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE).to_string());

    AddMetric(metricGroup, prometheus::UP, 1.0 * upState, timestamp, nanoSec, instance);
}

void ProcessorPromRelabelMetricNative::AddMetric(PipelineEventGroup& metricGroup,
                                                 const string& name,
                                                 double value,
                                                 time_t timestamp,
                                                 uint32_t nanoSec,
                                                 StringView instance) {
    auto* metricEvent = metricGroup.AddMetricEvent();
    metricEvent->SetName(name);
    metricEvent->SetValue<UntypedSingleValue>(value);
    metricEvent->SetTimestamp(timestamp, nanoSec);
    metricEvent->SetTag(prometheus::JOB, mJobName);
    metricEvent->SetTag(prometheus::INSTANCE, instance);
}

} // namespace logtail
