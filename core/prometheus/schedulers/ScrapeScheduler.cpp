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
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKey.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/async/PromHttpRequest.h"
#include "sdk/Common.h"

using namespace std;

DEFINE_FLAG_INT64(prom_stream_bytes_size, "stream bytes size", 1024 * 1024);

DEFINE_FLAG_BOOL(enable_prom_stream_scrape, "enable prom stream scrape", true);

namespace logtail {

size_t ScrapeScheduler::PromMetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data) {
    uint64_t sizes = size * nmemb;

    if (buffer == nullptr || data == nullptr) {
        return 0;
    }

    auto* body = static_cast<ScrapeScheduler*>(data);

    size_t begin = 0;
    for (size_t end = begin; end < sizes; ++end) {
        if (buffer[end] == '\n') {
            if (begin == 0 && !body->mCache.empty()) {
                body->mCache.append(buffer, end);
                body->AddEvent(body->mCache.data(), body->mCache.size());
                body->mCache.clear();
            } else if (begin != end) {
                body->AddEvent(buffer + begin, end - begin);
            }
            begin = end + 1;
        }
    }

    if (begin < sizes) {
        body->mCache.append(buffer + begin, sizes - begin);
    }
    body->mRawSize += sizes;
    body->mCurrStreamSize += sizes;
    if (body->mCurrTimestampMilliSec.empty()) {
        body->mCurrTimestampMilliSec = ToString(GetCurrentTimeInMilliSeconds());
    }

    if (BOOL_FLAG(enable_prom_stream_scrape) && body->mCurrStreamSize >= (size_t)INT64_FLAG(prom_stream_bytes_size)) {
        body->mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC,
                                      body->mCurrTimestampMilliSec);
        body->mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_ID,
                                      body->GetId() + body->mCurrTimestampMilliSec);

        body->SetTargetLabels(body->mEventGroup);
        body->mScrapeSamplesScraped += body->mEventGroup.GetEvents().size();
        body->PushEventGroup(std::move(body->mEventGroup));
        body->mStreamIndex++;
        body->mEventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
        body->mCurrStreamSize = 0;
    }

    return sizes;
}

void ScrapeScheduler::AddEvent(const char* line, size_t len) {
    if (IsValidMetric(StringView(line, len))) {
        auto* e = mEventGroup.AddRawEvent(true, mEventPool);
        auto sb = mEventGroup.GetSourceBuffer()->CopyString(line, len);
        e->SetContentNoCopy(sb);
    }
}

void ScrapeScheduler::FlushCache() {
    AddEvent(mCache.data(), mCache.size());
    mCache.clear();
}

ScrapeScheduler::ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                                 std::string host,
                                 int32_t port,
                                 Labels labels,
                                 QueueKey queueKey,
                                 size_t inputIndex)
    : mEventGroup(std::make_shared<SourceBuffer>()),
      mScrapeConfigPtr(std::move(scrapeConfigPtr)),
      mHost(std::move(host)),
      mPort(port),
      mTargetLabels(std::move(labels)),
      mQueueKey(queueKey),
      mInputIndex(inputIndex) {
    string tmpTargetURL = mScrapeConfigPtr->mScheme + "://" + mHost + ":" + ToString(mPort)
        + mScrapeConfigPtr->mMetricsPath
        + (mScrapeConfigPtr->mQueryString.empty() ? "" : "?" + mScrapeConfigPtr->mQueryString);
    mHash = mScrapeConfigPtr->mJobName + tmpTargetURL + ToString(mTargetLabels.Hash());
    mInstance = mHost + ":" + ToString(mPort);
    mInterval = mScrapeConfigPtr->mScrapeIntervalSeconds;
}

void ScrapeScheduler::OnMetricResult(HttpResponse& response, uint64_t timestampMilliSec) {
    static double sMilliSecRate = 1.0 / 1000.0;
    auto currTimestampMilliSec = GetCurrentTimeInMilliSeconds();
    auto& responseBody = *response.GetBody<ScrapeScheduler>();
    responseBody.FlushCache();
    mStreamIndex++;
    responseBody.mScrapeSamplesScraped += responseBody.mEventGroup.GetEvents().size();
    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL, response.GetStatusCode());
    mSelfMonitor->AddCounter(METRIC_PLUGIN_OUT_SIZE_BYTES, response.GetStatusCode(), responseBody.mRawSize);
    mSelfMonitor->AddCounter(
        METRIC_PLUGIN_PROM_SCRAPE_TIME_MS, response.GetStatusCode(), currTimestampMilliSec - timestampMilliSec);

    mScrapeTimestampMilliSec = timestampMilliSec;
    mScrapeDurationSeconds = (currTimestampMilliSec - timestampMilliSec) * sMilliSecRate;
    mScrapeResponseSizeBytes = responseBody.mRawSize;
    mUpState = response.GetStatusCode() == 200;
    if (response.GetStatusCode() != 200) {
        mScrapeResponseSizeBytes = 0;
        string headerStr;
        for (const auto& [k, v] : mScrapeConfigPtr->mRequestHeaders) {
            headerStr.append(k).append(":").append(v).append(";");
        }
        LOG_WARNING(
            sLogger,
            ("scrape failed, status code", response.GetStatusCode())("target", mHash)("http header", headerStr));
    }
    if (responseBody.mCurrTimestampMilliSec.empty()) {
        responseBody.mCurrTimestampMilliSec = ToString(GetCurrentTimeInMilliSeconds());
    }
    SetAutoMetricMeta(responseBody.mEventGroup);
    SetTargetLabels(responseBody.mEventGroup);
    PushEventGroup(std::move(responseBody.mEventGroup));
    responseBody.mEventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    responseBody.mRawSize = 0;
    responseBody.mCurrStreamSize = 0;
    responseBody.mCache.clear();
    responseBody.mStreamIndex = 0;
    responseBody.mScrapeSamplesScraped = 0;
    responseBody.mCurrTimestampMilliSec.clear();

    mPluginTotalDelayMs->Add(GetCurrentTimeInMilliSeconds() - timestampMilliSec);
}

void ScrapeScheduler::SetAutoMetricMeta(PipelineEventGroup& eGroup) const {
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC, ToString(mScrapeTimestampMilliSec));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED, ToString(mScrapeSamplesScraped));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION, ToString(mScrapeDurationSeconds));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE, ToString(mScrapeResponseSizeBytes));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE, ToString(mUpState));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_ID, GetId() + mCurrTimestampMilliSec);
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_TOTAL, ToString(mStreamIndex));
}

void ScrapeScheduler::SetTargetLabels(PipelineEventGroup& eGroup) const {
    mTargetLabels.Range([&eGroup](const std::string& key, const std::string& value) { eGroup.SetTag(key, value); });
}

void ScrapeScheduler::PushEventGroup(PipelineEventGroup&& eGroup) const {
    auto item = make_unique<ProcessQueueItem>(std::move(eGroup), mInputIndex);
#ifdef APSARA_UNIT_TEST_MAIN
    mItem.push_back(std::move(item));
    return;
#endif
    while (true) {
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item)) == 0) {
            break;
        }
        usleep(10 * 1000);
    }
}

string ScrapeScheduler::GetId() const {
    return mHash;
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
        } else {
            this->DelayExecTime(1);
            this->mPromDelayTotal->Add(1);
            this->ScheduleNext();
            return false;
        }
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
    auto retry = mScrapeConfigPtr->mScrapeIntervalSeconds / mScrapeConfigPtr->mScrapeTimeoutSeconds;
    if (retry > 0) {
        retry -= 1;
    }
    auto request = std::make_unique<PromHttpRequest>(sdk::HTTP_GET,
                                                     mScrapeConfigPtr->mScheme == prometheus::HTTPS,
                                                     mHost,
                                                     mPort,
                                                     mScrapeConfigPtr->mMetricsPath,
                                                     mScrapeConfigPtr->mQueryString,
                                                     mScrapeConfigPtr->mRequestHeaders,
                                                     "",
                                                     HttpResponse(
                                                         this, [](void*) {}, PromMetricWriteCallback),
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
