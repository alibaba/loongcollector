// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/timer/Timer.h"

#include "application/Application.h"
#include "logger/Logger.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"

using namespace std;

namespace logtail {

Timer::~Timer() {
    Stop();
}

void Timer::Init() {
    {
        if (mIsThreadRunning.exchange(true)) {
            return;
        }
    }
    InitMetrics();
    mThreadRes = async(launch::async, &Timer::Run, this);
}

void Timer::Stop() {
    {
        if (!mIsThreadRunning.exchange(false)) {
            return;
        }
    }
    mCV.notify_one();
    if (!mThreadRes.valid()) {
        return;
    }
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("timer", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("timer", "forced to stopped"));
    }
}

void Timer::PushEvent(unique_ptr<TimerEvent>&& e) {
    lock_guard<mutex> lock(mQueueMux);
    if (mQueue.empty() || e->GetExecTime() < mQueue.top()->GetExecTime()) {
        mQueue.push(std::move(e));
        mCV.notify_one();
    } else {
        mQueue.push(std::move(e));
    }
    // Update metrics
    if (mInItemsTotal) {
        mInItemsTotal->Add(1);
    }
    UpdateMetrics();
}

void Timer::Run() {
    LOG_INFO(sLogger, ("timer", "started"));
    while (mIsThreadRunning.load()) {
        unique_lock<mutex> queueLock(mQueueMux);
        if (mQueue.empty()) {
            mCV.wait(queueLock, [this]() { return !mIsThreadRunning.load() || !mQueue.empty(); });
        } else {
            auto now = chrono::steady_clock::now();
            while (!mQueue.empty()) {
                auto& e = mQueue.top();
                if (now < e->GetExecTime()) {
                    auto timeout = e->GetExecTime() - now + chrono::milliseconds(1);
                    mCV.wait_for(queueLock, timeout);
                    break;
                } else {
                    auto e = std::move(const_cast<unique_ptr<TimerEvent>&>(mQueue.top()));
                    auto execTime = e->GetExecTime();
                    mQueue.pop();
                    queueLock.unlock();

                    if (mLatencyTimeMs) {
                        auto latency = chrono::duration_cast<chrono::milliseconds>(now - execTime);
                        mLatencyTimeMs->Add(chrono::duration_cast<chrono::nanoseconds>(latency));
                    }
                    if (!e->IsValid()) {
                        LOG_INFO(sLogger, ("invalid timer event", "task is cancelled"));
                    } else {
                        e->Execute();
                        // Update metrics
                        if (mOutItemsTotal) {
                            mOutItemsTotal->Add(1);
                        }
                    }
                    queueLock.lock();
                    UpdateMetrics();
                }
            }
        }
    }
}

void Timer::InitMetrics() {
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, "timer");
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels));

    mInItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_TIMER_IN_ITEMS_TOTAL);
    mOutItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_TIMER_OUT_ITEMS_TOTAL);
    mQueueItemsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_TIMER_QUEUE_ITEMS_TOTAL);
    mLatencyTimeMs = mMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_TIMER_LATENCY_TIME_MS);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

void Timer::UpdateMetrics() {
    if (mQueueItemsTotal) {
        mQueueItemsTotal->Set(mQueue.size());
    }
}

#ifdef APSARA_UNIT_TEST_MAIN
void Timer::Clear() {
    lock_guard<mutex> lock(mQueueMux);
    while (!mQueue.empty()) {
        mQueue.pop();
    }
}
#endif

} // namespace logtail
