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

#include <cstdint>
#include <memory>
#include <string>

#include "common/http/HttpResponse.h"
#include "common/timer/Timer.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/PromSelfMonitor.h"
#include "prometheus/schedulers/BaseScheduler.h"
#include "prometheus/schedulers/ScrapeConfig.h"
#include "prometheus/schedulers/ScrapeScheduler.h"

namespace logtail {

class TargetSubscriberScheduler : public BaseScheduler {
public:
    TargetSubscriberScheduler();
    ~TargetSubscriberScheduler() override = default;

    bool Init(const Json::Value& scrapeConfig);
    bool operator<(const TargetSubscriberScheduler& other) const;

    void OnSubscription(HttpResponse&, uint64_t);
    void SetTimer(std::shared_ptr<Timer> timer);
    void SubscribeOnce(std::chrono::steady_clock::time_point execTime);

    std::string GetId() const;

    void ScheduleNext() override;
    void Cancel() override;
    void InitSelfMonitor(const MetricLabels&);

    // from pipeline context
    QueueKey mQueueKey;
    size_t mInputIndex;

    // from env
    std::string mPodName;
    std::string mServiceHost;
    int32_t mServicePort;

    // zero cost upgrade
    uint64_t mUnRegisterMs;

private:
    bool ParseScrapeSchedulerGroup(const std::string& content, std::vector<Labels>& scrapeSchedulerGroup);

    std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>>
    BuildScrapeSchedulerSet(std::vector<Labels>& scrapeSchedulerGroup);

    std::unique_ptr<TimerEvent> BuildSubscriberTimerEvent(std::chrono::steady_clock::time_point execTime);
    void UpdateScrapeScheduler(std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>>&);

    void CancelAllScrapeScheduler();

    std::shared_ptr<ScrapeConfig> mScrapeConfigPtr;

    ReadWriteLock mRWLock;
    std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>> mScrapeSchedulerMap;

    std::string mJobName;
    std::shared_ptr<Timer> mTimer;

    std::string mETag;

    // self monitor
    std::shared_ptr<PromSelfMonitorUnsafe> mSelfMonitor;
    MetricsRecordRef mMetricsRecordRef;
    IntGaugePtr mPromSubscriberTargets;
    CounterPtr mTotalDelayMs;
    MetricLabels mDefaultLabels;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class TargetSubscriberSchedulerUnittest;
    friend class InputPrometheusUnittest;
#endif
};

} // namespace logtail