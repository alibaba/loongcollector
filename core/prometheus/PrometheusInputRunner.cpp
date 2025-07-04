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

#include "PrometheusInputRunner.h"

#include <chrono>
#include <memory>
#include <string>

#include "application/Application.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "common/http/AsynCurlRunner.h"
#include "common/http/Constant.h"
#include "common/http/Curl.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

using namespace std;

DECLARE_FLAG_STRING(operator_service);
DECLARE_FLAG_INT32(operator_service_port);
DECLARE_FLAG_STRING(_pod_name_);

namespace logtail {

PrometheusInputRunner::PrometheusInputRunner()
    : mServiceHost(STRING_FLAG(operator_service)),
      mServicePort(INT32_FLAG(operator_service_port)),
      mPodName(STRING_FLAG(_pod_name_)),
      mEventPool(true),
      mUnRegisterMs(0) {
    // self monitor
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS);
    labels.emplace_back(METRIC_LABEL_KEY_INSTANCE_ID, Application::GetInstance()->GetInstanceId());
    labels.emplace_back(METRIC_LABEL_KEY_POD_NAME, mPodName);
    labels.emplace_back(METRIC_LABEL_KEY_SERVICE_HOST, mServiceHost);
    labels.emplace_back(METRIC_LABEL_KEY_SERVICE_PORT, ToString(mServicePort));

    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_KEY_PROJECT, [this]() -> std::string { return this->GetAllProjects(); });

    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels), std::move(dynamicLabels));

    mPromRegisterState = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_CLIENT_REGISTER_STATE);
    mPromJobNum = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_JOBS_TOTAL);
    mPromRegisterRetryTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

/// @brief receive scrape jobs from input plugins and update scrape jobs
void PrometheusInputRunner::UpdateScrapeInput(std::shared_ptr<TargetSubscriberScheduler> targetSubscriber,
                                              const MetricLabels& defaultLabels,
                                              const string& projectName) {
    RemoveScrapeInput(targetSubscriber->GetId());

    targetSubscriber->mServiceHost = mServiceHost;
    targetSubscriber->mServicePort = mServicePort;
    targetSubscriber->mPodName = mPodName;

    targetSubscriber->InitSelfMonitor(defaultLabels);

    targetSubscriber->mUnRegisterMs = mUnRegisterMs.load();
    targetSubscriber->SetComponent(&mEventPool);
    auto currSystemTime = chrono::system_clock::now();
    auto randSleepMilliSec
        = GetRandSleepMilliSec(targetSubscriber->GetId(),
                               prometheus::RefeshIntervalSeconds,
                               chrono::duration_cast<chrono::milliseconds>(currSystemTime.time_since_epoch()).count());
    auto firstExecTime = chrono::steady_clock::now() + chrono::milliseconds(randSleepMilliSec);
    auto firstSubscribeTime = currSystemTime + chrono::milliseconds(randSleepMilliSec);
    targetSubscriber->SetFirstExecTime(firstExecTime, firstSubscribeTime);
    // 1. add subscriber to mTargetSubscriberSchedulerMap
    {
        WriteLock lock(mSubscriberMapRWLock);
        mTargetSubscriberSchedulerMap[targetSubscriber->GetId()] = targetSubscriber;
    }
    // 2. build Ticker Event and add it to Timer
    targetSubscriber->ScheduleNext();
    {
        ReadLock lock(mSubscriberMapRWLock);
        SET_GAUGE(mPromJobNum, mTargetSubscriberSchedulerMap.size());
    }
    // 3. add project name to mJobNameToProjectNameMap for self monitor
    {
        WriteLock lock(mProjectRWLock);
        mJobNameToProjectNameMap[targetSubscriber->GetId()] = projectName;
    }
}

void PrometheusInputRunner::RemoveScrapeInput(const std::string& jobName) {
    {
        WriteLock lock(mSubscriberMapRWLock);
        if (mTargetSubscriberSchedulerMap.count(jobName)) {
            mTargetSubscriberSchedulerMap[jobName]->Cancel();
            mTargetSubscriberSchedulerMap.erase(jobName);
            SET_GAUGE(mPromJobNum, mTargetSubscriberSchedulerMap.size());
        }
    }
    {
        WriteLock lock(mProjectRWLock);
        if (mJobNameToProjectNameMap.count(jobName)) {
            mJobNameToProjectNameMap.erase(jobName);
        }
    }
}

/// @brief targets discovery and start scrape work
void PrometheusInputRunner::Init() {
    std::lock_guard<mutex> lock(mStartMutex);
    if (mIsStarted) {
        return;
    }
    LOG_INFO(sLogger, ("PrometheusInputRunner", "Start"));
    mIsStarted = true;

#ifndef APSARA_UNIT_TEST_MAIN
    Timer::GetInstance()->Init();
    AsynCurlRunner::GetInstance()->Init();
#endif

    LOG_INFO(sLogger, ("PrometheusInputRunner", "register"));
    // only register when operator exist
    if (!mServiceHost.empty()) {
        mIsThreadRunning.store(true);
        mThreadRes = std::async(launch::async, [this]() {
            std::lock_guard<mutex> lock(mRegisterMutex);
            int retry = 0;
            while (mIsThreadRunning.load()) {
                ++retry;
                auto httpResponse = SendRegisterMessage(prometheus::REGISTER_COLLECTOR_PATH);
                if (httpResponse.GetStatusCode() != 200) {
                    ADD_COUNTER(mPromRegisterRetryTotal, 1);
                    if (retry % 10 == 0) {
                        LOG_INFO(sLogger,
                                 ("register failed, retried", retry)("statusCode", httpResponse.GetStatusCode()));
                    }
                } else {
                    // register success
                    // response will be { "unRegisterMs": 30000 }
                    if (!httpResponse.GetBody<string>()->empty()) {
                        string responseStr = *httpResponse.GetBody<string>();
                        string errMsg;
                        Json::Value responseJson;
                        if (!ParseJsonTable(responseStr, responseJson, errMsg)) {
                            LOG_ERROR(sLogger, ("register failed, parse response failed", responseStr));
                        }
                        if (responseJson.isMember(prometheus::UNREGISTER_MS)
                            && responseJson[prometheus::UNREGISTER_MS].isString()) {
                            auto tmpStr = responseJson[prometheus::UNREGISTER_MS].asString();
                            if (tmpStr.empty()) {
                                mUnRegisterMs = 0;
                            } else {
                                uint64_t unRegisterMs{};
                                StringTo(tmpStr, unRegisterMs);
                                mUnRegisterMs.store(unRegisterMs);
                                // adjust unRegisterMs to scrape targets for zero-cost
                                mUnRegisterMs -= 1000;
                                LOG_INFO(sLogger, ("unRegisterMs", ToString(mUnRegisterMs)));
                            }
                        }
                    }
                    SET_GAUGE(mPromRegisterState, 1);
                    LOG_INFO(sLogger, ("Register Success", mPodName));
                    // subscribe immediately
                    SubscribeOnce();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
}

/// @brief stop scrape work and clear all scrape jobs
void PrometheusInputRunner::Stop() {
    std::lock_guard<mutex> lock(mStartMutex);
    if (!mIsStarted) {
        return;
    }

    mIsStarted = false;
    mIsThreadRunning.store(false);
    if (mThreadRes.valid()) {
        mThreadRes.wait_for(chrono::seconds(1));
    }

    LOG_INFO(sLogger, ("PrometheusInputRunner", "cancel all target subscribers"));
    CancelAllTargetSubscriber();
    {
        WriteLock lock(mSubscriberMapRWLock);
        mTargetSubscriberSchedulerMap.clear();
    }

    // only unregister when operator exist
    if (!mServiceHost.empty()) {
        LOG_INFO(sLogger, ("PrometheusInputRunner", "unregister"));
        auto res = std::async(launch::async, [this]() {
            std::lock_guard<mutex> lock(mRegisterMutex);
            for (int retry = 0; retry < 3; ++retry) {
                auto httpResponse = SendRegisterMessage(prometheus::UNREGISTER_COLLECTOR_PATH);
                if (httpResponse.GetStatusCode() != 200) {
                    LOG_ERROR(sLogger, ("unregister failed, statusCode", httpResponse.GetStatusCode()));
                } else {
                    LOG_INFO(sLogger, ("Unregister Success", mPodName));
                    SET_GAUGE(mPromRegisterState, 0);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    LOG_INFO(sLogger, ("PrometheusInputRunner", "Stop"));
}

bool PrometheusInputRunner::HasRegisteredPlugins() const {
    ReadLock lock(mSubscriberMapRWLock);
    return !mTargetSubscriberSchedulerMap.empty();
}

void PrometheusInputRunner::EventGC() {
    mEventPool.CheckGC();
}

HttpResponse PrometheusInputRunner::SendRegisterMessage(const string& url) const {
    HttpResponse httpResponse;
#ifdef APSARA_UNIT_TEST_MAIN
    httpResponse.SetStatusCode(200);
    return httpResponse;
#endif
    map<string, string> httpHeader;
    if (!SendHttpRequest(
            make_unique<HttpRequest>(
                HTTP_GET, false, mServiceHost, mServicePort, url, "pod_name=" + mPodName, httpHeader, "", 10),
            httpResponse)) {
        LOG_ERROR(sLogger, ("curl error", "")("url", url)("pod_name", mPodName));
    }
    return httpResponse;
}


void PrometheusInputRunner::CancelAllTargetSubscriber() {
    ReadLock lock(mSubscriberMapRWLock);
    for (auto& it : mTargetSubscriberSchedulerMap) {
        it.second->Cancel();
    }
}

void PrometheusInputRunner::SubscribeOnce() {
    ReadLock lock(mSubscriberMapRWLock);
    for (auto& [k, v] : mTargetSubscriberSchedulerMap) {
        v->SubscribeOnce(std::chrono::steady_clock::now());
    }
}

string PrometheusInputRunner::GetAllProjects() {
    string result;
    set<string> existProjects;
    ReadLock lock(mProjectRWLock);
    for (auto& [k, v] : mJobNameToProjectNameMap) {
        if (existProjects.find(v) == existProjects.end()) {
            if (!result.empty()) {
                result += " ";
            }
            existProjects.insert(v);
            result += v;
        }
    }
    return result;
}

}; // namespace logtail
