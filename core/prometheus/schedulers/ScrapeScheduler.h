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
#include "monitor/metric_models/MetricTypes.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/PromSelfMonitor.h"
#include "prometheus/component/StreamScraper.h"
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

    std::string GetId() const;

    void SetComponent(std::shared_ptr<Timer> timer, EventPool* eventPool);

    void ScheduleNext() override;
    void ScrapeOnce(std::chrono::steady_clock::time_point execTime);
    void Cancel() override;
    void InitSelfMonitor(const MetricLabels&);

private:
    std::unique_ptr<TimerEvent> BuildScrapeTimerEvent(std::chrono::steady_clock::time_point execTime);

    prom::StreamScraper mPromStreamScraper;

    std::shared_ptr<ScrapeConfig> mScrapeConfigPtr;
    std::string mHash;
    std::string mHost;
    int32_t mPort;
    std::string mInstance;

    // pipeline
    QueueKey mQueueKey;

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
#endif
};

} // namespace logtail
