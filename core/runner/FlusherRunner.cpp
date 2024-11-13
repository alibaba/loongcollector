// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runner/FlusherRunner.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/http/HttpRequest.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "pipeline/plugin/interface/HttpFlusher.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SenderQueueItem.h"
#include "pipeline/queue/SenderQueueManager.h"
#include "plugin/flusher/sls/DiskBufferWriter.h"
#include "runner/sink/http/HttpSink.h"
// TODO: temporarily used here
#include "plugin/flusher/sls/PackIdManager.h"
#include "plugin/flusher/sls/SLSClientManager.h"

DEFINE_FLAG_INT32(flusher_runner_exit_timeout_secs, "", 60);
DEFINE_FLAG_INT32(check_send_client_timeout_interval, "", 600);

using namespace std;

namespace logtail {

bool FlusherRunner::Init() {
    LoadModuleConfig(true);
    mCallback = [this]() { return LoadModuleConfig(false); };
    AppConfig::GetInstance()->RegisterCallback("max_bytes_per_sec", &mCallback);

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_FLUSHER}});
    mInItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_ITEMS_TOTAL);
    mInItemDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_SIZE_BYTES);
    mOutItemsTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_OUT_ITEMS_TOTAL);
    mTotalDelayMs = mMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_TOTAL_DELAY_MS);
    mLastRunTime = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    mInItemRawDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES);
    mWaitingItemsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_FLUSHER_WAITING_ITEMS_TOTAL);

    mThreadRes = async(launch::async, &FlusherRunner::Run, this);
    mLastCheckSendClientTime = time(nullptr);

    return true;
}

bool FlusherRunner::LoadModuleConfig(bool isInit) {
    auto ValidateFn = [](const std::string& key, const int32_t value) -> bool {
        if (key == "max_bytes_per_sec") {
            if (value < (int32_t)(1024 * 1024)) {
                return false;
            }
            return true;
        }
        return true;
    };
    if (isInit) {
        // Only handle parameters that do not allow hot loading
    }
    auto maxBytePerSec = AppConfig::GetInstance()->MergeInt32(
        kDefaultMaxSendBytePerSec, AppConfig::GetInstance()->GetMaxBytePerSec(), "max_bytes_per_sec", ValidateFn);
    AppConfig::GetInstance()->SetMaxBytePerSec(maxBytePerSec);
    UpdateSendFlowControl();
    return true;
}

void FlusherRunner::UpdateSendFlowControl() {
    // when inflow exceed 30MB/s, FlowControl lose precision
    if (AppConfig::GetInstance()->GetMaxBytePerSec() >= 30 * 1024 * 1024) {
        mEnableRateLimiter = false;
    }
    LOG_INFO(sLogger,
             ("send byte per second limit", AppConfig::GetInstance()->GetMaxBytePerSec())(
                 "send flow control", mEnableRateLimiter ? "enable" : "disable"));
}

void FlusherRunner::Stop() {
    mIsFlush = true;
    SenderQueueManager::GetInstance()->Trigger();
    future_status s = mThreadRes.wait_for(chrono::seconds(INT32_FLAG(flusher_runner_exit_timeout_secs)));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("flusher runner", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("flusher runner", "forced to stopped"));
    }
}

void FlusherRunner::DecreaseHttpSendingCnt() {
    --mHttpSendingCnt;
    SenderQueueManager::GetInstance()->Trigger();
}

void FlusherRunner::PushToHttpSink(SenderQueueItem* item, bool withLimit) {
    // TODO: use semaphore instead
    while (withLimit && !Application::GetInstance()->IsExiting()
           && GetSendingBufferCount() >= AppConfig::GetInstance()->GetSendRequestConcurrency()) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    auto req = static_cast<HttpFlusher*>(item->mFlusher)->BuildRequest(item);
    req->mEnqueTime = item->mLastSendTime = chrono::system_clock::now();
    HttpSink::GetInstance()->AddRequest(std::move(req));
    ++mHttpSendingCnt;
    LOG_DEBUG(sLogger,
              ("send item to http sink, item address", item)("config-flusher-dst",
                                                             QueueKeyManager::GetInstance()->GetName(item->mQueueKey))(
                  "sending cnt", ToString(mHttpSendingCnt.load())));
}

void FlusherRunner::Run() {
    LOG_INFO(sLogger, ("flusher runner", "started"));
    while (true) {
        auto curTime = chrono::system_clock::now();
        mLastRunTime->Set(chrono::duration_cast<chrono::seconds>(curTime.time_since_epoch()).count());

        vector<SenderQueueItem*> items;
        int32_t limit
            = Application::GetInstance()->IsExiting() ? -1 : AppConfig::GetInstance()->GetSendRequestConcurrency();
        SenderQueueManager::GetInstance()->GetAvailableItems(items, limit);
        if (items.empty()) {
            SenderQueueManager::GetInstance()->Wait(1000);
        } else {
            LOG_DEBUG(sLogger, ("got items from sender queue, cnt", items.size()));
            for (auto itr = items.begin(); itr != items.end(); ++itr) {
                mInItemDataSizeBytes->Add((*itr)->mData.size());
                mInItemRawDataSizeBytes->Add((*itr)->mRawSize);
            }
            mInItemsTotal->Add(items.size());
            mWaitingItemsTotal->Add(items.size());
        }

        for (auto itr = items.begin(); itr != items.end(); ++itr) {
            LOG_DEBUG(
                sLogger,
                ("got item from sender queue, item address",
                 *itr)("config-flusher-dst", QueueKeyManager::GetInstance()->GetName((*itr)->mQueueKey))(
                    "wait time",
                    ToString(chrono::duration_cast<chrono::milliseconds>(curTime - (*itr)->mFirstEnqueTime).count())
                        + "ms")("try cnt", ToString((*itr)->mTryCnt)));

            // TODO: use rate limiter instead
            if (!Application::GetInstance()->IsExiting() && mEnableRateLimiter) {
                RateLimiter::FlowControl((*itr)->mRawSize, mSendLastTime, mSendLastByte, true);
            }

            Dispatch(*itr);
            mWaitingItemsTotal->Sub(1);
            mOutItemsTotal->Add(1);
            mTotalDelayMs->Add(chrono::system_clock::now() - curTime);
        }

        // TODO: move the following logic to scheduler
        if ((time(NULL) - mLastCheckSendClientTime) > INT32_FLAG(check_send_client_timeout_interval)) {
            SLSClientManager::GetInstance()->CleanTimeoutClient();
            PackIdManager::GetInstance()->CleanTimeoutEntry();
            mLastCheckSendClientTime = time(NULL);
        }

        if (mIsFlush && SenderQueueManager::GetInstance()->IsAllQueueEmpty()) {
            break;
        }
    }
}

void FlusherRunner::Dispatch(SenderQueueItem* item) {
    switch (item->mFlusher->GetSinkType()) {
        case SinkType::HTTP:
            // TODO: make it common for all http flushers
            if (!BOOL_FLAG(enable_full_drain_mode) && Application::GetInstance()->IsExiting()
                && item->mFlusher->Name() == "flusher_sls") {
                DiskBufferWriter::GetInstance()->PushToDiskBuffer(item, 3);
                SenderQueueManager::GetInstance()->RemoveItem(item->mFlusher->GetQueueKey(), item);
            } else {
                PushToHttpSink(item);
            }
            break;
        default:
            SenderQueueManager::GetInstance()->RemoveItem(item->mFlusher->GetQueueKey(), item);
            break;
    }
}

} // namespace logtail
