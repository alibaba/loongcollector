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

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/SafeThread.h"
#include "common/StringTools.h"
#include "common/http/AsynCurlRunner.h"
#include "common/http/Constant.h"
#include "common/http/Curl.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

using namespace std;

DECLARE_FLAG_STRING(operator_service);
DECLARE_FLAG_INT32(operator_service_port);
DECLARE_FLAG_STRING(_pod_name_);

namespace logtail {

Json::Value PrometheusAgentInfo::ToJson() const {
    Json::Value value;
    value[prometheus::CPU_LIMIT] = mCpuLimit;
    value[prometheus::CPU_USAGE] = mCpuUsage;
    value[prometheus::MEM_LIMIT] = mMemLimit;
    value[prometheus::MEM_USAGE] = mMemUsage;
    value[prometheus::LOAD] = mLoad;
    value[prometheus::SCRAPE_DELAY_SECONDS] = mScrapeDelaySeconds;
    value[prometheus::HTTP_SINK_IN_ITEMS_TOTAL] = mHttpSinkInItemsTotal;
    value[prometheus::HTTP_SINK_OUT_FAILED] = mHttpSinkOutFailed;
    return value;
}

PrometheusInputRunner::PrometheusInputRunner()
    : mHttpRequestSender([](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
          return SendHttpRequest(std::move(request), response);
      }),
      mServiceHost(STRING_FLAG(operator_service)),
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

PrometheusInputRunner::~PrometheusInputRunner() {
    if (mStopToken) {
        lock_guard<mutex> lock(mStopToken->mRequestMutex);
        mStopToken->mStopped.store(true);
    }
    if (mThreadRes.valid()) {
        // Join before destroying the sender, snapshot locks, and metrics used by the worker.
        mThreadRes.wait();
    }
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
    targetSubscriber->CalculateFirstExecTime(firstExecTime, firstSubscribeTime);
    // 1. add subscriber to mTargetSubscriberSchedulerMap
    {
        WriteLock lock(mSubscriberMapRWLock);
        mTargetSubscriberSchedulerMap[targetSubscriber->GetId()] = targetSubscriber;
    }
    // 2. build Ticker Event and add it to Timer
    // if host only mode, build target subscriber directly, otherwise build scrape scheduler
    if (targetSubscriber->IsHostOnlyMode()) {
        targetSubscriber->ScheduleHostOnlyTargets();
    } else {
        targetSubscriber->ScheduleNext();
    }
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

    if (mThreadRes.valid()) {
        // Prevent overlapping snapshot producers across runner restarts.
        mThreadRes.wait();
    }
    if (!mServiceHost.empty()) {
        mStopToken = make_shared<StopToken>();
        LaunchAsync(
            mThreadRes, "PrometheusHeartbeat", [this, stop = mStopToken]() { RunRegistrationAndHeartbeat(stop); });
    }
}

void PrometheusInputRunner::RunRegistrationAndHeartbeat(const shared_ptr<StopToken>& stop) {
    bool registered = false;
    bool initialRegistration = true;
    int retry = 0;
    const auto interval = chrono::seconds(prometheus::RefeshIntervalSeconds);
    chrono::steady_clock::time_point nextHeartbeatTime;
    while (!stop->mStopped.load()) {
        if (!registered) {
            auto response = SendRegisterMessage(prometheus::REGISTER_COLLECTOR_PATH, stop);
            if (stop->mStopped.load()) {
                break;
            }
            if (response.GetStatusCode() != 200) {
                ++retry;
                ADD_COUNTER(mPromRegisterRetryTotal, 1);
                if (retry % 10 == 0) {
                    LOG_INFO(sLogger, ("register failed, retried", retry)("statusCode", response.GetStatusCode()));
                }
                SleepFor(chrono::seconds(1));
                continue;
            }
            {
                lock_guard<mutex> lock(stop->mRequestMutex);
                if (stop->mStopped.load()) {
                    break;
                }
                if (initialRegistration) {
                    OnInitialRegistration(response);
                    initialRegistration = false;
                }
                SET_GAUGE(mPromRegisterState, 1);
            }
            registered = true;
            retry = 0;
            nextHeartbeatTime = SteadyNow();
            LOG_INFO(sLogger, ("Register Success", mPodName));
        }
        if (stop->mStopped.load()) {
            break;
        }
        auto result = SendHeartbeat(stop);
        if (stop->mStopped.load()) {
            break;
        }
        if (result == HeartbeatResult::NotRegistered) {
            registered = false;
            SET_GAUGE(mPromRegisterState, 0);
        }
        nextHeartbeatTime += interval;
        auto now = SteadyNow();
        if (nextHeartbeatTime < now) {
            // Skip missed slots instead of sending a burst of overdue heartbeats.
            nextHeartbeatTime += interval * ((now - nextHeartbeatTime) / interval);
            if (nextHeartbeatTime < now) {
                nextHeartbeatTime += interval;
            }
        }
        SleepFor(nextHeartbeatTime - now);
    }
}

void PrometheusInputRunner::OnInitialRegistration(const HttpResponse& response) {
    const auto& body = *response.GetBody<string>();
    if (!body.empty()) {
        string errorMsg;
        Json::Value responseJson;
        if (!ParseJsonTable(body, responseJson, errorMsg)) {
            LOG_ERROR(sLogger, ("register failed, parse response failed", body));
        }
        if (responseJson.isObject() && responseJson[prometheus::UNREGISTER_MS].isString()) {
            const auto& timestamp = responseJson[prometheus::UNREGISTER_MS].asString();
            if (timestamp.empty()) {
                mUnRegisterMs = 0;
            } else {
                uint64_t unRegisterMs{};
                StringTo(timestamp, unRegisterMs);
                mUnRegisterMs.store(unRegisterMs);
                // Preserve the one-second upgrade compensation before the first subscription.
                mUnRegisterMs -= 1000;
                LOG_INFO(sLogger, ("unRegisterMs", ToString(mUnRegisterMs)));
            }
        }
    }
    SubscribeOnce();
}

chrono::steady_clock::time_point PrometheusInputRunner::SteadyNow() const {
#ifdef APSARA_UNIT_TEST_MAIN
    if (mSteadyNow) {
        return mSteadyNow();
    }
#endif
    return chrono::steady_clock::now();
}

void PrometheusInputRunner::SleepFor(chrono::steady_clock::duration interval) const {
#ifdef APSARA_UNIT_TEST_MAIN
    if (mSleepFor) {
        mSleepFor(interval);
        return;
    }
#endif
    this_thread::sleep_for(interval);
}

/// @brief stop scrape work and clear all scrape jobs
void PrometheusInputRunner::Stop() {
    std::lock_guard<mutex> lock(mStartMutex);
    if (!mIsStarted) {
        return;
    }

    mIsStarted = false;
    if (mStopToken) {
        lock_guard<mutex> stopLock(mStopToken->mRequestMutex);
        mStopToken->mStopped.store(true);
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
        std::future<void> res;
        LaunchAsync(res, "PrometheusUnregister", [this]() {
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

PrometheusAgentInfo PrometheusInputRunner::RefreshAgentInfoSnapshot() {
    PrometheusAgentInfo snapshot;

    SelfMonitorMetricEvent agentMetric;
    LoongCollectorMonitor::GetInstance()->GetAgentMetric(agentMetric);
    SelfMonitorMetricEvent httpSinkMetric;
    LoongCollectorMonitor::GetInstance()->GetRunnerMetric(METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK, httpSinkMetric);

    snapshot.mCpuLimit = AppConfig::GetInstance()->GetCpuUsageUpLimit();
    snapshot.mCpuUsage = agentMetric.GetGauge(METRIC_AGENT_CPU);
    snapshot.mMemLimit = AppConfig::GetInstance()->GetMemUsageUpLimit();
    snapshot.mMemUsage = agentMetric.GetGauge(METRIC_AGENT_MEMORY);
    snapshot.mHttpSinkInItemsTotal = httpSinkMetric.GetCounter(METRIC_RUNNER_IN_ITEMS_TOTAL);
    snapshot.mHttpSinkOutFailed = httpSinkMetric.GetCounter(METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL);

    {
        ReadLock lock(mSubscriberMapRWLock);
        for (const auto& item : mTargetSubscriberSchedulerMap) {
            auto statistics = item.second->CollectLoadAndDrainScrapeDelay();
            snapshot.mLoad += statistics.mLoad;
            snapshot.mScrapeDelaySeconds += statistics.mScrapeDelaySeconds;
        }
    }
    {
        WriteLock lock(mAgentInfoRWLock);
        mAgentInfoSnapshot = snapshot;
    }
    return snapshot;
}

PrometheusAgentInfo PrometheusInputRunner::GetAgentInfoSnapshot() const {
    ReadLock lock(mAgentInfoRWLock);
    return mAgentInfoSnapshot;
}

HttpResponse PrometheusInputRunner::SendOperatorRequest(const string& method,
                                                        const string& url,
                                                        const string& query,
                                                        const map<string, string>& headers,
                                                        const string& body,
                                                        uint32_t timeout,
                                                        uint32_t maxTryCnt,
                                                        const shared_ptr<StopToken>& stop) const {
    HttpResponse httpResponse;
    auto request = make_unique<HttpRequest>(
        method, false, mServiceHost, mServicePort, url, query, headers, body, timeout, maxTryCnt);
    unique_lock<mutex> requestLock;
    if (stop) {
        requestLock = unique_lock<mutex>(stop->mRequestMutex);
        if (stop->mStopped.load()) {
            return httpResponse;
        }
        // Registration must finish before Stop can unregister; heartbeats need not.
        if (url != prometheus::REGISTER_COLLECTOR_PATH) {
            requestLock.unlock();
        }
    }
    if (!mHttpRequestSender(std::move(request), httpResponse)) {
        LOG_ERROR(sLogger, ("curl error", "")("method", method)("url", url)("query", query));
    }
    return httpResponse;
}

HttpResponse PrometheusInputRunner::SendRegisterMessage(const string& url, const shared_ptr<StopToken>& stop) const {
    map<string, string> headers{
        {prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION, prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1}};
    return SendOperatorRequest(HTTP_GET,
                               url,
                               "pod_name=" + mPodName,
                               headers,
                               "",
                               10,
                               stop ? 1U : static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)),
                               stop);
}

PrometheusInputRunner::HeartbeatResult PrometheusInputRunner::SendHeartbeat(const shared_ptr<StopToken>& stop) {
    if (stop && stop->mStopped.load()) {
        return HeartbeatResult::Failed;
    }
    Json::Value body;
    body[prometheus::AGENT_INFO] = RefreshAgentInfoSnapshot().ToJson();
    map<string, string> headers{
        {prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION, prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1},
        {CONTENT_TYPE, prometheus::APPLICATION_JSON},
        {prometheus::ACCEPT, prometheus::APPLICATION_JSON}};
    auto response = SendOperatorRequest(HTTP_POST,
                                        prometheus::HEARTBEAT_COLLECTOR_PATH,
                                        string(prometheus::COLLECTOR_ID) + "=" + URLEncode(mPodName),
                                        headers,
                                        body.toStyledString(),
                                        3,
                                        1,
                                        stop);
    if (response.GetNetworkStatus().mCode != NetworkCode::Ok) {
        return HeartbeatResult::Failed;
    }
    if (response.GetStatusCode() == 204) {
        return HeartbeatResult::Success;
    }
    if (response.GetStatusCode() == 404) {
        const auto& responseHeaders = response.GetHeader();
        auto version = responseHeaders.find(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION);
        if (version != responseHeaders.end()
            && version->second == prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1) {
            Json::Value responseBody;
            string errorMsg;
            if (ParseJsonTable(*response.GetBody<string>(), responseBody, errorMsg) && responseBody.isObject()
                && responseBody[prometheus::MSG].isString()
                && responseBody[prometheus::MSG].asString() == prometheus::NOT_REGISTERED) {
                return HeartbeatResult::NotRegistered;
            }
        }
    } else if (response.GetStatusCode() == 400) {
        LOG_WARNING(sLogger, ("heartbeat resource snapshot rejected", mPodName));
    }
    return HeartbeatResult::Failed;
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
