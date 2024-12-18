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
#include "common/timer/Timer.h"
#include "component/GlobalConfig.h"
#include "monitor/metric_models/MetricTypes.h"
#include "prometheus/schedulers/TargetSubscriberScheduler.h"
#include "runner/InputRunner.h"
#include "sdk/Common.h"
#include "sdk/CurlImp.h"

namespace logtail::prom {

class PrometheusServer : public InputRunner {
public:
    PrometheusServer(const PrometheusServer&) = delete;
    PrometheusServer(PrometheusServer&&) = delete;
    PrometheusServer& operator=(const PrometheusServer&) = delete;
    PrometheusServer& operator=(PrometheusServer&&) = delete;
    ~PrometheusServer() override = default;
    static PrometheusServer* GetInstance() {
        static PrometheusServer sInstance;
        return &sInstance;
    }
    void CheckGC();

    // input plugin update
    void UpdateScrapeInput(std::shared_ptr<TargetSubscriberScheduler> targetSubscriber,
                           const MetricLabels& defaultLabels,
                           const std::string& projectName);
    void RemoveScrapeInput(const std::string& jobName);
    std::shared_ptr<GlobalConfig> GetGlobalConfig() const { return mGlobalConfig; }

    // target discover and scrape
    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

private:
    PrometheusServer();
    sdk::HttpMessage SendRegisterMessage(const std::string& url) const;

    void CancelAllTargetSubscriber();
    void SubscribeOnce();

    std::string GetAllProjects();
    bool UpdateGlobalConfig();

    bool mIsStarted = false;
    std::mutex mStartMutex;

    std::mutex mRegisterMutex;
    std::atomic<bool> mIsThreadRunning = true;
    std::future<void> mThreadRes;

    std::string mServiceHost;
    int32_t mServicePort;
    std::string mPodName;

    std::unique_ptr<sdk::CurlClient> mClient;
    std::shared_ptr<Timer> mTimer;
    EventPool mEventPool;
    std::shared_ptr<GlobalConfig> mGlobalConfig;
    std::function<bool()> mCallback;

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

} // namespace logtail::prom
