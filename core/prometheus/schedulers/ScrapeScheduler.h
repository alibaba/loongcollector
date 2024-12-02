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
#include "monitor/MetricTypes.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/PromSelfMonitor.h"
#include "prometheus/schedulers/ScrapeConfig.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include "pipeline/queue/ProcessQueueItem.h"
#endif

namespace logtail {


class ScrapeScheduler : public BaseScheduler {
public:
    ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                    std::string host,
                    int32_t port,
                    Labels labels,
                    QueueKey queueKey,
                    size_t inputIndex);
    ScrapeScheduler(const ScrapeScheduler&) = delete;
    ~ScrapeScheduler() override = default;

    void OnMetricResult(HttpResponse&, uint64_t timestampMilliSec);
    static size_t PromMetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);
    void AddEvent(const char* line, size_t len);
    void FlushCache();
    size_t mRawSize = 0;
    size_t mCurrStreamSize = 0;
    std::string mCache;
    PipelineEventGroup mEventGroup;
    uint64_t mStreamIndex = 0;
    std::string mCurrTimestampMilliSec;

    std::string GetId() const;

    void ScheduleNext() override;
    void ScrapeOnce(std::chrono::steady_clock::time_point execTime);
    void Cancel() override;
    void InitSelfMonitor(const MetricLabels&);

private:
    void PushEventGroup(PipelineEventGroup&&) const;
    void SetAutoMetricMeta(PipelineEventGroup& eGroup) const;
    void SetTargetLabels(PipelineEventGroup& eGroup) const;

    std::unique_ptr<TimerEvent> BuildScrapeTimerEvent(std::chrono::steady_clock::time_point execTime);

    std::shared_ptr<ScrapeConfig> mScrapeConfigPtr;

    std::string mHash;
    std::string mHost;
    int32_t mPort;
    std::string mInstance;
    Labels mTargetLabels;

    // pipeline
    QueueKey mQueueKey;
    size_t mInputIndex;

    // auto metrics
    uint64_t mScrapeTimestampMilliSec = 0;
    uint64_t mScrapeSamplesScraped = 0;
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
