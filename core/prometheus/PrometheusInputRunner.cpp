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

#include "AppConfig.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"
#include "sdk/Common.h"
#include "sdk/Exception.h"

using namespace std;

DECLARE_FLAG_STRING(loong_collector_operator_service);
DECLARE_FLAG_INT32(loong_collector_operator_service_port);
DECLARE_FLAG_STRING(_pod_name_);

namespace logtail::prom {

PrometheusServer::PrometheusServer()
    : mServiceHost(STRING_FLAG(loong_collector_operator_service)),
      mServicePort(INT32_FLAG(loong_collector_operator_service_port)),
      mPodName(STRING_FLAG(_pod_name_)),
      mEventPool(true),
      mUnRegisterMs(0) {
    mClient = std::make_unique<sdk::CurlClient>();
    mTimer = std::make_shared<Timer>();
    mGlobalConfig = std::make_shared<GlobalConfig>();

    // self monitor
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS);
    labels.emplace_back(METRIC_LABEL_KEY_INSTANCE_ID, Application::GetInstance()->GetInstanceId());
    labels.emplace_back(METRIC_LABEL_KEY_POD_NAME, mPodName);
    labels.emplace_back(METRIC_LABEL_KEY_SERVICE_HOST, mServiceHost);
    labels.emplace_back(METRIC_LABEL_KEY_SERVICE_PORT, ToString(mServicePort));

    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_KEY_PROJECT, [this]() -> std::string { return this->GetAllProjects(); });

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels), std::move(dynamicLabels));

    mPromRegisterState = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_CLIENT_REGISTER_STATE);
    mPromJobNum = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_JOBS_TOTAL);
    mPromRegisterRetryTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL);
}

/// @brief receive scrape jobs from input plugins and update scrape jobs
void PrometheusServer::UpdateScrapeInput(std::shared_ptr<TargetSubscriberScheduler> targetSubscriber,
                                         const MetricLabels& defaultLabels,
                                         const string& projectName) {
    RemoveScrapeInput(targetSubscriber->GetId());

    targetSubscriber->mServiceHost = mServiceHost;
    targetSubscriber->mServicePort = mServicePort;
    targetSubscriber->mPodName = mPodName;

    targetSubscriber->InitSelfMonitor(defaultLabels);

    targetSubscriber->mUnRegisterMs = mUnRegisterMs.load();
    targetSubscriber->SetComponent(mTimer, &mEventPool);
    auto currSystemTime = chrono::system_clock::now();
    auto randSleepMilliSec
        = GetRandSleepMilliSec(targetSubscriber->GetId(),
                               prom::RefeshIntervalSeconds,
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
        mPromJobNum->Set(mTargetSubscriberSchedulerMap.size());
    }
    // 3. add project name to mJobNameToProjectNameMap for self monitor
    {
        WriteLock lock(mProjectRWLock);
        mJobNameToProjectNameMap[targetSubscriber->GetId()] = projectName;
    }
}

void PrometheusServer::RemoveScrapeInput(const std::string& jobName) {
    {
        WriteLock lock(mSubscriberMapRWLock);
        if (mTargetSubscriberSchedulerMap.count(jobName)) {
            mTargetSubscriberSchedulerMap[jobName]->Cancel();
            mTargetSubscriberSchedulerMap.erase(jobName);
            mPromJobNum->Set(mTargetSubscriberSchedulerMap.size());
        }
    }
    {
        WriteLock lock(mProjectRWLock);
        if (mJobNameToProjectNameMap.count(jobName)) {
            mJobNameToProjectNameMap.erase(jobName);
        }
    }
}

bool PrometheusServer::UpdateGlobalConfig() {
    auto validateFn = [](const std::string& key, const std::string& _) -> bool {
        if (key == prom::PROM_DROP_METRICS) {
            return true;
        }
        return false;
    };
    auto dropMetrics = AppConfig::GetInstance()->MergeString("", "", prom::PROM_DROP_METRICS, validateFn);
    mGlobalConfig->UpdateDropMetrics(dropMetrics);
    return true;
}

/// @brief targets discovery and start scrape work
void PrometheusServer::Init() {
    std::lock_guard<mutex> lock(mStartMutex);
    if (mIsStarted) {
        return;
    }
    LOG_INFO(sLogger, ("PrometheusInputRunner", "Start"));
    mIsStarted = true;

    UpdateGlobalConfig();
    mCallback = [this]() -> bool { return UpdateGlobalConfig(); };
#ifndef APSARA_UNIT_TEST_MAIN
    mTimer->Init();
    AsynCurlRunner::GetInstance()->Init();
#endif
    AppConfig::GetInstance()->RegisterCallback(prom::PROM_DROP_METRICS, &mCallback);

    LOG_INFO(sLogger, ("PrometheusInputRunner", "register"));
    // only register when operator exist
    if (!mServiceHost.empty()) {
        mIsThreadRunning.store(true);
        mThreadRes = std::async(launch::async, [this]() {
            std::lock_guard<mutex> lock(mRegisterMutex);
            int retry = 0;
            while (mIsThreadRunning.load()) {
                ++retry;
                sdk::HttpMessage httpResponse = SendRegisterMessage(prom::REGISTER_COLLECTOR_PATH);
                if (httpResponse.statusCode != 200) {
                    mPromRegisterRetryTotal->Add(1);
                    if (retry % 10 == 0) {
                        LOG_INFO(sLogger, ("register failed, retried", retry)("statusCode", httpResponse.statusCode));
                    }
                } else {
                    // register success
                    // response will be { "unRegisterMs": 30000 }
                    if (!httpResponse.content.empty()) {
                        string responseStr = httpResponse.content;
                        string errMsg;
                        Json::Value responseJson;
                        if (!ParseJsonTable(responseStr, responseJson, errMsg)) {
                            LOG_ERROR(sLogger, ("register failed, parse response failed", responseStr));
                        }
                        if (responseJson.isMember(prom::UNREGISTER_MS)
                            && responseJson[prom::UNREGISTER_MS].isString()) {
                            auto tmpStr = responseJson[prom::UNREGISTER_MS].asString();
                            if (tmpStr.empty()) {
                                mUnRegisterMs = 0;
                            } else {
                                mUnRegisterMs.store(StringTo<uint64_t>(tmpStr));
                                LOG_INFO(sLogger, ("unRegisterMs", ToString(mUnRegisterMs)));
                            }
                        }
                    }
                    mPromRegisterState->Set(1);
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
void PrometheusServer::Stop() {
    std::lock_guard<mutex> lock(mStartMutex);
    if (!mIsStarted) {
        return;
    }

    mIsStarted = false;
    mIsThreadRunning.store(false);
    if (mThreadRes.valid()) {
        mThreadRes.wait_for(chrono::seconds(1));
    }

#ifndef APSARA_UNIT_TEST_MAIN
    mTimer->Stop();
    LOG_INFO(sLogger, ("PrometheusInputRunner", "stop asyn curl runner"));
    AsynCurlRunner::GetInstance()->Stop();
#endif
    AppConfig::GetInstance()->UnregisterCallback(prom::PROM_DROP_METRICS);

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
                sdk::HttpMessage httpResponse = SendRegisterMessage(prom::UNREGISTER_COLLECTOR_PATH);
                if (httpResponse.statusCode != 200) {
                    LOG_ERROR(sLogger, ("unregister failed, statusCode", httpResponse.statusCode));
                } else {
                    LOG_INFO(sLogger, ("Unregister Success", mPodName));
                    mPromRegisterState->Set(0);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    LOG_INFO(sLogger, ("PrometheusInputRunner", "Stop"));
}

bool PrometheusServer::HasRegisteredPlugins() const {
    ReadLock lock(mSubscriberMapRWLock);
    return !mTargetSubscriberSchedulerMap.empty();
}

sdk::HttpMessage PrometheusServer::SendRegisterMessage(const string& url) const {
    map<string, string> httpHeader;
    httpHeader[sdk::X_LOG_REQUEST_ID] = prom::PROMETHEUS_PREFIX + mPodName;
    sdk::HttpMessage httpResponse;
    httpResponse.header[sdk::X_LOG_REQUEST_ID] = prom::PROMETHEUS_PREFIX + mPodName;
#ifdef APSARA_UNIT_TEST_MAIN
    httpResponse.statusCode = 200;
    return httpResponse;
#endif
    try {
        mClient->Send(sdk::HTTP_GET,
                      mServiceHost,
                      mServicePort,
                      url,
                      "pod_name=" + mPodName,
                      httpHeader,
                      "",
                      10,
                      httpResponse,
                      "",
                      false);
    } catch (const sdk::LOGException& e) {
        LOG_ERROR(sLogger, ("curl error", e.what())("url", url)("pod_name", mPodName));
    }
    return httpResponse;
}


void PrometheusServer::CancelAllTargetSubscriber() {
    ReadLock lock(mSubscriberMapRWLock);
    for (auto& it : mTargetSubscriberSchedulerMap) {
        it.second->Cancel();
    }
}

void PrometheusServer::SubscribeOnce() {
    ReadLock lock(mSubscriberMapRWLock);
    for (auto& [k, v] : mTargetSubscriberSchedulerMap) {
        v->SubscribeOnce(std::chrono::steady_clock::now());
    }
}

string PrometheusServer::GetAllProjects() {
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

void PrometheusServer::CheckGC() {
    mEventPool.CheckGC();
}
}; // namespace logtail::prom