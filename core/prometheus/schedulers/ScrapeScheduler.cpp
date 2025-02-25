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

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/http/Constant.h"
#include "common/timer/HttpRequestTimerEvent.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/async/PromHttpRequest.h"
#include "prometheus/component/StreamScraper.h"

using namespace std;

namespace logtail {

ScrapeScheduler::ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                                 std::string host,
                                 int32_t port,
                                 std::string scheme,
                                 std::string metricsPath,
                                 uint64_t scrapeIntervalSeconds,
                                 uint64_t scrapeTimeoutSeconds,
                                 Labels labels,
                                 QueueKey queueKey,
                                 size_t inputIndex)
    : mScrapeConfigPtr(std::move(scrapeConfigPtr)),
      mHost(std::move(host)),
      mPort(port),
      mMetricsPath(std::move(metricsPath)),
      mScheme(std::move(scheme)),
      mScrapeTimeoutSeconds(scrapeTimeoutSeconds),
      mQueueKey(queueKey),
      mInputIndex(inputIndex),
      mTargetLabels(labels) {
    string tmpTargetURL = mScheme + "://" + mHost + ":" + ToString(mPort) + mMetricsPath;
    mHash = mScrapeConfigPtr->mJobName + tmpTargetURL + ToString(labels.Hash());
    mInstance = mHost + ":" + ToString(mPort);
    mInterval = scrapeIntervalSeconds;
}

void ScrapeScheduler::OnMetricResult(HttpResponse& response, uint64_t) {
    static double sRate = 0.001;
    auto now = GetCurrentTimeInMilliSeconds();
    auto scrapeTimestampMilliSec
        = chrono::duration_cast<chrono::milliseconds>(mLatestScrapeTime.time_since_epoch()).count();
    auto scrapeDurationMilliSeconds = now - scrapeTimestampMilliSec;
    auto* streamScraper = response.GetBody<prom::StreamScraper>();

    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL, response.GetStatusCode());
    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_SIZE_BYTES, response.GetStatusCode(), streamScraper->mRawSize);
    mSelfMonitor->AddCounter(METRIC_PLUGIN_PROM_SCRAPE_TIME_MS, response.GetStatusCode(), scrapeDurationMilliSeconds);

    const auto& networkStatus = response.GetNetworkStatus();
    string scrapeState;
    mUpState = false;
    if (networkStatus.mCode != NetworkCode::Ok) {
        // not 0 means curl error
        scrapeState = prom::NetworkCodeToState(networkStatus.mCode);
    } else if (response.GetStatusCode() != 200) {
        scrapeState = prom::HttpCodeToState(response.GetStatusCode());
    } else {
        // 0 means success
        scrapeState = prom::NetworkCodeToState(NetworkCode::Ok);
        mUpState = true;
    }

    mScrapeDurationSeconds = scrapeDurationMilliSeconds * sRate;
    if (response.GetStatusCode() != 200) {
        LOG_WARNING(sLogger,
                    ("scrape failed, status code",
                     response.GetStatusCode())("target", mHash)("curl msg", response.GetNetworkStatus().mMessage));
    }

    auto mScrapeDurationSeconds = scrapeDurationMilliSeconds * sRate;
    streamScraper->mStreamIndex++;

    if (mUpState) {
        streamScraper->FlushCache();
    }
    streamScraper->SetAutoMetricMeta(mScrapeDurationSeconds, mUpState, scrapeState);
    streamScraper->SendMetrics();
    streamScraper->Reset();

    ADD_COUNTER(mPluginTotalDelayMs, scrapeDurationMilliSeconds);
}


string ScrapeScheduler::GetId() const {
    return mHash;
}

uint64_t ScrapeScheduler::GetScrapeIntervalSeconds() const {
    return mInterval;
}

void ScrapeScheduler::SetComponent(shared_ptr<Timer> timer, EventPool* eventPool) {
    mTimer = std::move(timer);
    mEventPool = eventPool;
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
        ADD_COUNTER(this->mPromDelayTotal, 1);
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
    auto retry = mInterval / mScrapeTimeoutSeconds;
    if (retry > 0) {
        retry -= 1;
    }

    auto request = std::make_unique<PromHttpRequest>(
        HTTP_GET,
        mScheme == prometheus::HTTPS,
        mHost,
        mPort,
        mMetricsPath,
        "",
        mScrapeConfigPtr->mRequestHeaders,
        "",
        HttpResponse(
            new prom::StreamScraper(mTargetLabels, mQueueKey, mInputIndex, mHash, mEventPool, mLatestScrapeTime),
            [](void* p) { delete static_cast<prom::StreamScraper*>(p); },
            prom::StreamScraper::MetricWriteCallback),
        mScrapeTimeoutSeconds,
        retry,
        this->mFuture,
        this->mIsContextValidFuture,
        mScrapeConfigPtr->mFollowRedirects,
        mScrapeConfigPtr->mEnableTLS ? std::optional<CurlTLS>(mScrapeConfigPtr->mTLS) : std::nullopt);

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

    static const std::unordered_map<std::string, MetricType> sScrapeMetricKeys
        = {{METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
           {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
           {METRIC_PLUGIN_PROM_SCRAPE_TIME_MS, MetricType::METRIC_TYPE_COUNTER}};

    mSelfMonitor->InitMetricManager(sScrapeMetricKeys, labels);

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE, std::move(labels));
    mPromDelayTotal = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_PROM_SCRAPE_DELAY_TOTAL);
    mPluginTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_PLUGIN_TOTAL_DELAY_MS);
}

} // namespace logtail
