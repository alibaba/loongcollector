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
#include "prometheus/schedulers/TargetSubscriberScheduler.h"
#include "sdk/Common.h"
#include "sdk/CurlImp.h"

namespace logtail {

class PrometheusInputRunner {
public:
    PrometheusInputRunner(const PrometheusInputRunner&) = delete;
    PrometheusInputRunner(PrometheusInputRunner&&) = delete;
    PrometheusInputRunner& operator=(const PrometheusInputRunner&) = delete;
    PrometheusInputRunner& operator=(PrometheusInputRunner&&) = delete;
    static PrometheusInputRunner* GetInstance() {
        static PrometheusInputRunner sInstance;
        return &sInstance;
    }

    // input plugin update
    void UpdateScrapeInput(std::shared_ptr<TargetSubscriberScheduler> targetSubscriber);
    void RemoveScrapeInput(const std::string& jobName);

    // target discover and scrape
    void Start();
    void Stop();
    bool HasRegisteredPlugin();

private:
    PrometheusInputRunner();
    ~PrometheusInputRunner() = default;

    sdk::HttpMessage SendRegisterMessage(const std::string& url) const;

    void CancelAllTargetSubscriber();

    std::atomic<bool> mIsStarted;

    std::future<void> mThreadRes;
    std::atomic<bool> mIsThreadRunning = true;

    std::unique_ptr<sdk::CurlClient> mClient;

    std::string mServiceHost;
    int32_t mServicePort;
    std::string mPodName;

    std::shared_ptr<Timer> mTimer;

    ReadWriteLock mSubscriberMapRWLock;
    std::map<std::string, std::shared_ptr<TargetSubscriberScheduler>> mTargetSubscriberSchedulerMap;

    uint64_t mUnRegisterMs;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PrometheusInputRunnerUnittest;
    friend class InputPrometheusUnittest;

#endif
};

} // namespace logtail
