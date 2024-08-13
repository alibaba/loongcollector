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

#include "prometheus/schedulers/TargetSubscriberScheduler.h"

#include <xxhash/xxhash.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "Common.h"
#include "TimeUtil.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "common/timer/HttpRequestTimerEvent.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/async/PromHttpRequest.h"
#include "prometheus/schedulers/ScrapeScheduler.h"

using namespace std;

namespace logtail {

TargetSubscriberScheduler::TargetSubscriberScheduler()
    : mQueueKey(0), mInputIndex(0), mServicePort(0), mUnRegisterMs(0) {
}

bool TargetSubscriberScheduler::Init(const Json::Value& scrapeConfig) {
    mScrapeConfigPtr = std::make_shared<ScrapeConfig>();
    if (!mScrapeConfigPtr->Init(scrapeConfig)) {
        return false;
    }
    mJobName = mScrapeConfigPtr->mJobName;
    mInterval = prometheus::RefeshIntervalSeconds;

    return true;
}

bool TargetSubscriberScheduler::operator<(const TargetSubscriberScheduler& other) const {
    return mJobName < other.mJobName;
}

void TargetSubscriberScheduler::OnSubscription(const HttpResponse& response) {
    if (response.mStatusCode == 304) {
        // not modified
        return;
    }
    if (response.mStatusCode != 200) {
        return;
    }
    if (response.mHeader.count(prometheus::ETAG)) {
        mETag = response.mHeader.at(prometheus::ETAG);
    }
    const string& content = response.mBody;
    vector<Labels> targetGroup;
    if (!ParseScrapeSchedulerGroup(content, targetGroup)) {
        return;
    }
    std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>> newScrapeSchedulerSet
        = BuildScrapeSchedulerSet(targetGroup);
    UpdateScrapeScheduler(newScrapeSchedulerSet);
}

void TargetSubscriberScheduler::UpdateScrapeScheduler(
    std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>>& newScrapeSchedulerMap) {
    {
        WriteLock lock(mRWLock);
        vector<string> toRemove;

        // remove obsolete scrape work
        for (const auto& [k, v] : mScrapeSchedulerMap) {
            if (newScrapeSchedulerMap.find(k) == newScrapeSchedulerMap.end()) {
                toRemove.push_back(k);
            }
        }

        for (auto& k : toRemove) {
            mScrapeSchedulerMap[k]->Cancel();
            mScrapeSchedulerMap.erase(k);
        }

        // save new scrape work
        for (const auto& [k, v] : newScrapeSchedulerMap) {
            if (mScrapeSchedulerMap.find(k) == mScrapeSchedulerMap.end()) {
                mScrapeSchedulerMap[k] = v;
                if (mTimer) {
                    // zero-cost upgrade
                    if (mUnRegisterMs > 0
                        && (GetCurrentTimeInNanoSeconds() + v->GetRandSleep()
                                - (uint64_t)mScrapeConfigPtr->mScrapeIntervalSeconds * 1000000000
                            > mUnRegisterMs * 1000000)
                        && (GetCurrentTimeInNanoSeconds() + v->GetRandSleep()
                                - (uint64_t)mScrapeConfigPtr->mScrapeIntervalSeconds * 1000000000 * 2
                            < mUnRegisterMs * 1000000)) {
                        // scrape once just now
                        v->ScrapeOnce(std::chrono::steady_clock::now());
                    }
                    v->ScheduleNext();
                }
            }
        }
    }
}

bool TargetSubscriberScheduler::ParseScrapeSchedulerGroup(const std::string& content,
                                                          std::vector<Labels>& scrapeSchedulerGroup) {
    string errs;
    Json::Value root;
    if (!ParseJsonTable(content, root, errs) || !root.isArray()) {
        LOG_ERROR(sLogger,
                  ("http service discovery from operator failed", "Failed to parse JSON: " + errs)("job", mJobName));
        return false;
    }
    for (const auto& element : root) {
        if (!element.isObject()) {
            LOG_ERROR(
                sLogger,
                ("http service discovery from operator failed", "Invalid target group item found")("job", mJobName));
            return false;
        }

        // Parse targets
        vector<string> targets;
        if (element.isMember(prometheus::TARGETS) && element[prometheus::TARGETS].isArray()) {
            for (const auto& target : element[prometheus::TARGETS]) {
                if (target.isString()) {
                    targets.push_back(target.asString());
                } else {
                    LOG_ERROR(
                        sLogger,
                        ("http service discovery from operator failed", "Invalid target item found")("job", mJobName));
                    return false;
                }
            }
        }
        if (targets.empty()) {
            continue;
        }
        // Parse labels
        Labels labels;
        labels.Push(Label{prometheus::JOB, mJobName});
        labels.Push(Label{prometheus::ADDRESS_LABEL_NAME, targets[0]});
        labels.Push(Label{prometheus::SCHEME_LABEL_NAME, mScrapeConfigPtr->mScheme});
        labels.Push(Label{prometheus::METRICS_PATH_LABEL_NAME, mScrapeConfigPtr->mMetricsPath});
        labels.Push(
            Label{prometheus::SCRAPE_INTERVAL_LABEL_NAME, SecondToDuration(mScrapeConfigPtr->mScrapeIntervalSeconds)});
        labels.Push(
            Label{prometheus::SCRAPE_TIMEOUT_LABEL_NAME, SecondToDuration(mScrapeConfigPtr->mScrapeTimeoutSeconds)});
        for (const auto& pair : mScrapeConfigPtr->mParams) {
            if (!pair.second.empty()) {
                labels.Push(Label{prometheus::PARAM_LABEL_NAME + pair.first, pair.second[0]});
            }
        }

        if (element.isMember(prometheus::LABELS) && element[prometheus::LABELS].isObject()) {
            for (const auto& labelKey : element[prometheus::LABELS].getMemberNames()) {
                labels.Push(Label{labelKey, element[prometheus::LABELS][labelKey].asString()});
            }
        }
        scrapeSchedulerGroup.push_back(labels);
    }
    return true;
}

std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>>
TargetSubscriberScheduler::BuildScrapeSchedulerSet(std::vector<Labels>& targetGroups) {
    std::unordered_map<std::string, std::shared_ptr<ScrapeScheduler>> scrapeSchedulerMap;
    for (const auto& labels : targetGroups) {
        // Relabel Config
        Labels resultLabel = Labels();
        bool keep = prometheus::Process(labels, mScrapeConfigPtr->mRelabelConfigs, resultLabel);
        if (!keep) {
            continue;
        }
        resultLabel.RemoveMetaLabels();
        if (resultLabel.Size() == 0) {
            continue;
        }

        string address = resultLabel.Get(prometheus::ADDRESS_LABEL_NAME);
        auto m = address.find(':');
        if (m == string::npos) {
            continue;
        }
        int32_t port = 0;
        try {
            port = stoi(address.substr(m + 1));
        } catch (...) {
            continue;
        }

        string host = address.substr(0, m);
        auto scrapeScheduler
            = std::make_shared<ScrapeScheduler>(mScrapeConfigPtr, host, port, resultLabel, mQueueKey, mInputIndex);

        scrapeScheduler->SetTimer(mTimer);
        auto firstExecTime
            = std::chrono::steady_clock::now() + std::chrono::nanoseconds(scrapeScheduler->GetRandSleep());

        scrapeScheduler->SetFirstExecTime(firstExecTime);

        scrapeSchedulerMap[scrapeScheduler->GetId()] = scrapeScheduler;
    }
    return scrapeSchedulerMap;
}

void TargetSubscriberScheduler::SetTimer(shared_ptr<Timer> timer) {
    mTimer = std::move(timer);
}

string TargetSubscriberScheduler::GetId() const {
    return mJobName;
}

void TargetSubscriberScheduler::ScheduleNext() {
    auto future = std::make_shared<PromFuture>();
    future->AddDoneCallback([this](const HttpResponse& response) {
        this->OnSubscription(response);
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

    auto event = BuildSubscriberTimerEvent(GetNextExecTime());
    mTimer->PushEvent(std::move(event));
}

void TargetSubscriberScheduler::Cancel() {
    mFuture->Cancel();
    {
        WriteLock lock(mLock);
        mValidState = false;
    }
    CancelAllScrapeScheduler();
}

std::unique_ptr<TimerEvent>
TargetSubscriberScheduler::BuildSubscriberTimerEvent(std::chrono::steady_clock::time_point execTime) {
    map<string, string> httpHeader;
    httpHeader[prometheus::ACCEPT] = prometheus::APPLICATION_JSON;
    httpHeader[prometheus::X_PROMETHEUS_REFRESH_INTERVAL_SECONDS] = ToString(prometheus::RefeshIntervalSeconds);
    httpHeader[prometheus::USER_AGENT] = prometheus::PROMETHEUS_PREFIX + mPodName;
    if (!mETag.empty()) {
        httpHeader[prometheus::IF_NONE_MATCH] = mETag;
    }
    auto request = std::make_unique<PromHttpRequest>(sdk::HTTP_GET,
                                                     false,
                                                     mServiceHost,
                                                     mServicePort,
                                                     "/jobs/" + URLEncode(GetId()) + "/targets",
                                                     "collector_id=" + mPodName,
                                                     httpHeader,
                                                     "",
                                                     prometheus::RefeshIntervalSeconds,
                                                     1,
                                                     this->mFuture);
    auto timerEvent = std::make_unique<HttpRequestTimerEvent>(execTime, std::move(request));

    return timerEvent;
}

void TargetSubscriberScheduler::CancelAllScrapeScheduler() {
    ReadLock lock(mRWLock);
    for (const auto& [k, v] : mScrapeSchedulerMap) {
        v->Cancel();
    }
}


} // namespace logtail
