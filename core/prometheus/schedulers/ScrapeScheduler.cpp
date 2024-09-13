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

#include "Common.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/timer/HttpRequestTimerEvent.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/async/PromHttpRequest.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKey.h"

using namespace std;

namespace logtail {

ScrapeScheduler::ScrapeScheduler(std::shared_ptr<ScrapeConfig> scrapeConfigPtr,
                                 std::string host,
                                 int32_t port,
                                 Labels labels,
                                 QueueKey queueKey,
                                 size_t inputIndex)
    : mScrapeConfigPtr(std::move(scrapeConfigPtr)),
      mHost(std::move(host)),
      mPort(port),
      mLabels(std::move(labels)),
      mQueueKey(queueKey),
      mInputIndex(inputIndex) {
    string tmpTargetURL = mScrapeConfigPtr->mScheme + "://" + mHost + ":" + ToString(mPort)
        + mScrapeConfigPtr->mMetricsPath
        + (mScrapeConfigPtr->mQueryString.empty() ? "" : "?" + mScrapeConfigPtr->mQueryString);
    mHash = mScrapeConfigPtr->mJobName + tmpTargetURL + ToString(mLabels.Hash());
    mInstance = mHost + ":" + ToString(mPort);
    mInterval = mScrapeConfigPtr->mScrapeIntervalSeconds;

    mParser = make_unique<TextParser>();
}

void ScrapeScheduler::OnMetricResult(const HttpResponse& response, uint64_t timestampMilliSec) {
    mScrapeTimestampMilliSec = timestampMilliSec;
    mScrapeDurationSeconds = 1.0 * (GetCurrentTimeInMilliSeconds() - timestampMilliSec) / 1000;
    mScrapeResponseSizeBytes = response.mBody.size();
    mUpState = response.mStatusCode == 200;
    if (response.mStatusCode != 200) {
        mScrapeResponseSizeBytes = 0;
        string headerStr;
        for (const auto& [k, v] : mScrapeConfigPtr->mAuthHeaders) {
            headerStr.append(k).append(":").append(v).append(";");
        }
        LOG_WARNING(sLogger,
                    ("scrape failed, status code", response.mStatusCode)("target", mHash)("http header", headerStr));
    }
    auto eventGroup = BuildPipelineEventGroup(response.mBody);

    SetAutoMetricMeta(eventGroup);
    PushEventGroup(std::move(eventGroup));
}

void ScrapeScheduler::SetAutoMetricMeta(PipelineEventGroup& eGroup) {
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC, ToString(mScrapeTimestampMilliSec));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION, ToString(mScrapeDurationSeconds));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE, ToString(mScrapeResponseSizeBytes));
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_INSTANCE, mInstance);
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE, ToString(mUpState));
}

PipelineEventGroup ScrapeScheduler::BuildPipelineEventGroup(const std::string& content) {
    return mParser->BuildLogGroup(content);
}

void ScrapeScheduler::PushEventGroup(PipelineEventGroup&& eGroup) {
    auto item = make_unique<ProcessQueueItem>(std::move(eGroup), mInputIndex);
#ifdef APSARA_UNIT_TEST_MAIN
    mItem.push_back(std::move(item));
#endif
    ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item));
}

string ScrapeScheduler::GetId() const {
    return mHash;
}

void ScrapeScheduler::ScheduleNext() {
    auto future = std::make_shared<PromFuture>();
    future->AddDoneCallback([this](const HttpResponse& response, uint64_t timestampMilliSec) {
        this->OnMetricResult(response, timestampMilliSec);
        this->ExecDone();
        this->ScheduleNext();
    });

    if (IsCancelled()) {
        mFuture->Cancel();
        return;
    }

    {
        WriteLock lock(mLock);
        mFuture = future;
    }

    auto event = BuildScrapeTimerEvent(GetNextExecTime());
    mTimer->PushEvent(std::move(event));
}

void ScrapeScheduler::ScrapeOnce(std::chrono::steady_clock::time_point execTime) {
    auto future = std::make_shared<PromFuture>();
    future->AddDoneCallback([this](const HttpResponse& response, uint64_t timestampMilliSec) {
        this->OnMetricResult(response, timestampMilliSec);
    });
    mFuture = future;
    auto event = BuildScrapeTimerEvent(execTime);
    if (mTimer) {
        mTimer->PushEvent(std::move(event));
    }
}

std::unique_ptr<TimerEvent> ScrapeScheduler::BuildScrapeTimerEvent(std::chrono::steady_clock::time_point execTime) {
    auto request = std::make_unique<PromHttpRequest>(sdk::HTTP_GET,
                                                     mScrapeConfigPtr->mScheme == prometheus::HTTPS,
                                                     mHost,
                                                     mPort,
                                                     mScrapeConfigPtr->mMetricsPath,
                                                     mScrapeConfigPtr->mQueryString,
                                                     mScrapeConfigPtr->mAuthHeaders,
                                                     "",
                                                     mScrapeConfigPtr->mScrapeTimeoutSeconds,
                                                     mScrapeConfigPtr->mScrapeIntervalSeconds
                                                         / mScrapeConfigPtr->mScrapeTimeoutSeconds,
                                                     this->mFuture);
    auto timerEvent = std::make_unique<HttpRequestTimerEvent>(execTime, std::move(request));
    return timerEvent;
}

void ScrapeScheduler::Cancel() {
    if (mFuture) {
        mFuture->Cancel();
    }
    {
        WriteLock lock(mLock);
        mValidState = false;
    }
}

void ScrapeScheduler::SetTimer(std::shared_ptr<Timer> timer) {
    mTimer = std::move(timer);
}
} // namespace logtail
