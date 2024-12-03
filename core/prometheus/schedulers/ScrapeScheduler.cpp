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

#include "prometheus/schedulers/ScrapeScheduler.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/timer/HttpRequestTimerEvent.h"
#include "logger/Logger.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/Constants.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/async/PromHttpRequest.h"
#include "prometheus/component/StreamScraper.h"
#include "sdk/Common.h"

using namespace std;

namespace logtail {

ScrapeScheduler::ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                                 std::string host,
                                 int32_t port,
                                 Labels labels,
                                 QueueKey queueKey,
                                 size_t inputIndex)
    : mPromStreamScraper(labels, queueKey, inputIndex),
      mScrapeConfigPtr(std::move(scrapeConfigPtr)),
      mHost(std::move(host)),
      mPort(port),
      mQueueKey(queueKey) {
    string tmpTargetURL = mScrapeConfigPtr->mScheme + "://" + mHost + ":" + ToString(mPort)
        + mScrapeConfigPtr->mMetricsPath
        + (mScrapeConfigPtr->mQueryString.empty() ? "" : "?" + mScrapeConfigPtr->mQueryString);
    mHash = mScrapeConfigPtr->mJobName + tmpTargetURL + ToString(labels.Hash());
    mInstance = mHost + ":" + ToString(mPort);
    mInterval = mScrapeConfigPtr->mScrapeIntervalSeconds;

    mPromStreamScraper.mHash = mHash;
}

void ScrapeScheduler::OnMetricResult(HttpResponse& response, uint64_t) {
    static double sRate = 0.001;
    auto now = GetCurrentTimeInMilliSeconds();
    auto scrapeTimestampMilliSec
        = chrono::duration_cast<chrono::milliseconds>(mLatestScrapeTime.time_since_epoch()).count();
    auto scrapeDurationMilliSeconds = now - scrapeTimestampMilliSec;

    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL, response.GetStatusCode());
    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_SIZE_BYTES, response.GetStatusCode(), mPromStreamScraper.mRawSize);
    mSelfMonitor->AddCounter(METRIC_PLUGIN_PROM_SCRAPE_TIME_MS, response.GetStatusCode(), scrapeDurationMilliSeconds);


    if (response.GetStatusCode() != 200) {
        string headerStr;
        for (const auto& [k, v] : mScrapeConfigPtr->mRequestHeaders) {
            headerStr.append(k).append(":").append(v).append(";");
        }
        LOG_WARNING(
            sLogger,
            ("scrape failed, status code", response.GetStatusCode())("target", mHash)("http header", headerStr));
    }

    auto mScrapeDurationSeconds = scrapeDurationMilliSeconds * sRate;
    auto mUpState = response.GetStatusCode() == 200;
    mPromStreamScraper.mStreamIndex++;
    mPromStreamScraper.FlushCache();
    mPromStreamScraper.SetAutoMetricMeta(mScrapeDurationSeconds, mUpState);
    mPromStreamScraper.SendMetrics();
    mPromStreamScraper.Reset();

    mPluginTotalDelayMs->Add(scrapeDurationMilliSeconds);
}


string ScrapeScheduler::GetId() const {
    return mHash;
}

void ScrapeScheduler::SetComponent(shared_ptr<Timer> timer, EventPool* eventPool) {
    mTimer = std::move(timer);
    mEventPool = eventPool;
    mPromStreamScraper.mEventPool = mEventPool;
}

void ScrapeScheduler::ScheduleNext() {
    auto future = std::make_shared<PromFuture<HttpResponse&, uint64_t>>();
    auto isContextValidFuture = std::make_shared<PromFuture<>>();
    future->AddDoneCallback([this](HttpResponse& response, uint64_t timestampMilliSec) {
        this->OnMetricResult(response, timestampMilliSec);
        this->ExecDone();
        this->ScheduleNext();
        return true;
    });
    isContextValidFuture->AddDoneCallback([this]() -> bool {
        if (ProcessQueueManager::GetInstance()->IsValidToPush(mQueueKey)) {
            return true;
        }
        this->DelayExecTime(1);
        this->mPromDelayTotal->Add(1);
        this->ScheduleNext();
        return false;
    });

    if (IsCancelled()) {
        mFuture->Cancel();
        mIsContextValidFuture->Cancel();
        return;
    }

    {
        WriteLock lock(mLock);
        mFuture = future;
        mIsContextValidFuture = isContextValidFuture;
    }

    mPromStreamScraper.SetScrapeTime(mLatestScrapeTime);
    auto event = BuildScrapeTimerEvent(GetNextExecTime());
    mTimer->PushEvent(std::move(event));
}

void ScrapeScheduler::ScrapeOnce(std::chrono::steady_clock::time_point execTime) {
    auto future = std::make_shared<PromFuture<HttpResponse&, uint64_t>>();
    future->AddDoneCallback([this](HttpResponse& response, uint64_t timestampMilliSec) {
        this->OnMetricResult(response, timestampMilliSec);
        return true;
    });
    mFuture = future;
    auto event = BuildScrapeTimerEvent(execTime);
    if (mTimer) {
        mTimer->PushEvent(std::move(event));
    }
}

std::unique_ptr<TimerEvent> ScrapeScheduler::BuildScrapeTimerEvent(std::chrono::steady_clock::time_point execTime) {
    auto retry = mScrapeConfigPtr->mScrapeIntervalSeconds / mScrapeConfigPtr->mScrapeTimeoutSeconds;
    if (retry > 0) {
        retry -= 1;
    }
    mPromStreamScraper.SetScrapeTime(mLatestScrapeTime);
    auto request = std::make_unique<PromHttpRequest>(
        sdk::HTTP_GET,
        mScrapeConfigPtr->mScheme == prometheus::HTTPS,
        mHost,
        mPort,
        mScrapeConfigPtr->mMetricsPath,
        mScrapeConfigPtr->mQueryString,
        mScrapeConfigPtr->mRequestHeaders,
        "",
        HttpResponse(
            &mPromStreamScraper, [](void*) {}, prom::PromStreamScraper::MetricWriteCallback),
        mScrapeConfigPtr->mScrapeTimeoutSeconds,
        retry,
        this->mFuture,
        this->mIsContextValidFuture);
    auto timerEvent = std::make_unique<HttpRequestTimerEvent>(execTime, std::move(request));
    return timerEvent;
}

void ScrapeScheduler::Cancel() {
    if (mFuture != nullptr) {
        mFuture->Cancel();
    }
    if (mIsContextValidFuture != nullptr) {
        mIsContextValidFuture->Cancel();
    }
    {
        WriteLock lock(mLock);
        mValidState = false;
    }
}

void ScrapeScheduler::InitSelfMonitor(const MetricLabels& defaultLabels) {
    mSelfMonitor = std::make_shared<PromSelfMonitorUnsafe>();
    MetricLabels labels = defaultLabels;
    labels.emplace_back(METRIC_LABEL_KEY_INSTANCE, mInstance);

    static const std::unordered_map<std::string, MetricType> sScrapeMetricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_PROM_SCRAPE_TIME_MS, MetricType::METRIC_TYPE_COUNTER},
    };

    mSelfMonitor->InitMetricManager(sScrapeMetricKeys, labels);

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE, std::move(labels));
    mPromDelayTotal = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_PROM_SCRAPE_DELAY_TOTAL);
    mPluginTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_TOTAL_DELAY_MS);
}

} // namespace logtail
