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

#include <memory>
#include <string>

#include "BaseScheduler.h"
#include "common/http/HttpResponse.h"
#include "models/PipelineEventGroup.h"
#include "monitor/metric_models/MetricTypes.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/PromSelfMonitor.h"
#include "prometheus/Utils.h"
#include "prometheus/schedulers/ScrapeConfig.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include "pipeline/queue/ProcessQueueItem.h"
#endif

namespace logtail {

size_t PromMetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);

struct PromMetricResponseBody {
    PipelineEventGroup mEventGroup;
    std::string mCache;
    size_t mRawSize = 0;
    EventPool* mEventPool = nullptr;

    explicit PromMetricResponseBody(EventPool* eventPool)
        : mEventGroup(std::make_shared<SourceBuffer>()), mEventPool(eventPool) {};
    void AddEvent(char* line, size_t len) {
        if (IsValidMetric(StringView(line, len))) {
            auto* e = mEventGroup.AddRawEvent(true, mEventPool);
            auto sb = mEventGroup.GetSourceBuffer()->CopyString(line, len);
            e->SetContentNoCopy(sb);
        }
    }
    void FlushCache() {
        AddEvent(mCache.data(), mCache.size());
        mCache.clear();
    }
};

class ScrapeScheduler : public BaseScheduler {
public:
    ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                    std::string host,
                    int32_t port,
                    Labels labels,
                    QueueKey queueKey,
                    size_t inputIndex);
    ScrapeScheduler(const ScrapeScheduler&) = default;
    ~ScrapeScheduler() override = default;

    void OnMetricResult(HttpResponse&, uint64_t timestampMilliSec);

    std::string GetId() const;

    void ScheduleNext() override;
    void ScrapeOnce(std::chrono::steady_clock::time_point execTime);
    void Cancel() override;
    void InitSelfMonitor(const MetricLabels&);

private:
    void PushEventGroup(PipelineEventGroup&&);
    void SetAutoMetricMeta(PipelineEventGroup& eGroup);
    void SetTargetLabels(PipelineEventGroup& eGroup);

    std::unique_ptr<TimerEvent> BuildScrapeTimerEvent(std::chrono::steady_clock::time_point execTime);

    std::shared_ptr<ScrapeConfig> mScrapeConfigPtr;

    std::string mHash;
    std::string mHost;
    int32_t mPort;
    std::string mInstance;
    Labels mTargetLabels;

    QueueKey mQueueKey;
    size_t mInputIndex;

    // auto metrics
    uint64_t mScrapeTimestampMilliSec = 0;
    double mScrapeDurationSeconds = 0;
    uint64_t mScrapeResponseSizeBytes = 0;
    bool mUpState = true;

    // self monitor
    std::shared_ptr<PromSelfMonitorUnsafe> mSelfMonitor;
    MetricsRecordRef mMetricsRecordRef;
    CounterPtr mPromDelayTotal;
    CounterPtr mPluginTotalDelayMs;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParsePrometheusMetricUnittest;
    friend class ScrapeSchedulerUnittest;
    std::vector<std::shared_ptr<ProcessQueueItem>> mItem;
#endif
};

} // namespace logtail
