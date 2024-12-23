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

#include <cstdint>
#include <memory>
#include <string>

#include "common/Lock.h"
#include "common/http/HttpResponse.h"
#include "common/timer/Timer.h"
#include "monitor/metric_models/MetricTypes.h"
#include "prometheus/schedulers/TargetSubscriberScheduler.h"
#include "runner/InputRunner.h"

namespace logtail {

class PrometheusInputRunner : public InputRunner {
public:
    PrometheusInputRunner(const PrometheusInputRunner&) = delete;
    PrometheusInputRunner(PrometheusInputRunner&&) = delete;
    PrometheusInputRunner& operator=(const PrometheusInputRunner&) = delete;
    PrometheusInputRunner& operator=(PrometheusInputRunner&&) = delete;
    ~PrometheusInputRunner() override = default;
    static PrometheusInputRunner* GetInstance() {
        static PrometheusInputRunner sInstance;
        return &sInstance;
    }
    void CheckGC();

    // input plugin update
    void UpdateScrapeInput(std::shared_ptr<TargetSubscriberScheduler> targetSubscriber,
                           const MetricLabels& defaultLabels,
                           const std::string& projectName);
    void RemoveScrapeInput(const std::string& jobName);

    // target discover and scrape
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

private:
    PrometheusInputRunner();
    HttpResponse SendRegisterMessage(const std::string& url) const;

    void CancelAllTargetSubscriber();
    void SubscribeOnce();

    std::string GetAllProjects();

    bool mIsStarted = false;
    std::mutex mStartMutex;

    std::mutex mRegisterMutex;
    std::atomic<bool> mIsThreadRunning = true;
    std::future<void> mThreadRes;

    std::string mServiceHost;
    int32_t mServicePort;
    std::string mPodName;

    std::shared_ptr<Timer> mTimer;
    EventPool mEventPool;

    mutable ReadWriteLock mSubscriberMapRWLock;
    std::map<std::string, std::shared_ptr<TargetSubscriberScheduler>> mTargetSubscriberSchedulerMap;

    std::atomic<uint64_t> mUnRegisterMs;

    // self monitor
    ReadWriteLock mProjectRWLock;
    std::map<std::string, std::string> mJobNameToProjectNameMap;
    MetricsRecordRef mMetricsRecordRef;
    CounterPtr mPromRegisterRetryTotal;
    IntGaugePtr mPromRegisterState;
    IntGaugePtr mPromJobNum;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PrometheusInputRunnerUnittest;
    friend class InputPrometheusUnittest;

#endif
};

} // namespace logtail
