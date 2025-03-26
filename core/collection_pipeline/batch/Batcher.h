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

#pragma once

#include <cstdint>

#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/batch/BatchItem.h"
#include "collection_pipeline/batch/BatchStatus.h"
#include "collection_pipeline/batch/FlushStrategy.h"
#include "collection_pipeline/batch/TimeoutFlushManager.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "models/PipelineEventGroup.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

template <typename T = EventBatchStatus>
class Batcher {
public:
    bool Init(const Json::Value& config,
              Flusher* flusher,
              const DefaultFlushStrategyOptions& strategy,
              bool enableGroupBatch = false) {
        std::string errorMsg;
        CollectionPipelineContext& ctx = flusher->GetContext();

        uint32_t minSizeBytes = strategy.mMinSizeBytes;
        if (!GetOptionalUIntParam(config, "MinSizeBytes", minSizeBytes, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  minSizeBytes,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        uint32_t minCnt = strategy.mMinCnt;
        if (!GetOptionalUIntParam(config, "MinCnt", minCnt, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  minCnt,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        uint32_t timeoutSecs = strategy.mTimeoutSecs;
        if (!GetOptionalUIntParam(config, "TimeoutSecs", timeoutSecs, errorMsg)) {
            PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                                  ctx.GetAlarm(),
                                  errorMsg,
                                  timeoutSecs,
                                  flusher->Name(),
                                  ctx.GetConfigName(),
                                  ctx.GetProjectName(),
                                  ctx.GetLogstoreName(),
                                  ctx.GetRegion());
        }

        if (enableGroupBatch) {
            uint32_t groupTimeout = timeoutSecs / 2;
            mGroupFlushStrategy = GroupFlushStrategy(minSizeBytes, groupTimeout);
            mGroupQueue = GroupBatchItem();
            mEventFlushStrategy.SetTimeoutSecs(timeoutSecs - groupTimeout);
        } else {
            mEventFlushStrategy.SetTimeoutSecs(timeoutSecs);
        }
        mEventFlushStrategy.SetMaxSizeBytes(strategy.mMaxSizeBytes);
        mEventFlushStrategy.SetMinSizeBytes(minSizeBytes);
        mEventFlushStrategy.SetMinCnt(minCnt);

        mFlusher = flusher;

        std::vector<std::pair<std::string, std::string>> labels{
            {METRIC_LABEL_KEY_PROJECT, ctx.GetProjectName()},
            {METRIC_LABEL_KEY_PIPELINE_NAME, ctx.GetConfigName()},
            {METRIC_LABEL_KEY_COMPONENT_NAME, METRIC_LABEL_VALUE_COMPONENT_NAME_BATCHER},
            {METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID, flusher->GetPluginID()}};
        if (enableGroupBatch) {
            labels.emplace_back(METRIC_LABEL_KEY_GROUP_BATCH_ENABLED, "true");
        } else {
            labels.emplace_back(METRIC_LABEL_KEY_GROUP_BATCH_ENABLED, "false");
        }
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
            mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_COMPONENT, std::move(labels));
        mInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_EVENTS_TOTAL);
        mInGroupDataSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_IN_SIZE_BYTES);
        mOutEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_OUT_EVENTS_TOTAL);
        // mTotalDelayMs = mMetricsRecordRef.CreateCounter(METRIC_COMPONENT_TOTAL_DELAY_MS);
        mEventBatchItemsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_BATCHER_EVENT_BATCHES_TOTAL);
        mBufferedGroupsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_TOTAL);
        mBufferedEventsTotal = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_TOTAL);
        mBufferedDataSizeByte = mMetricsRecordRef.CreateIntGauge(METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES);
        mTotalAddTimeMs = mMetricsRecordRef.CreateTimeCounter(METRIC_COMPONENT_BATCHER_TOTAL_ADD_TIME_MS);

        return true;
    }

    // when group level batch is disabled, there should be only 1 element in BatchedEventsList
    void Add(PipelineEventGroup&& g, std::vector<BatchedEventsList>& res) {
        auto before = std::chrono::system_clock::now();
        std::lock_guard<std::mutex> lock(mMux);
        size_t key = g.GetTagsHash();
        EventBatchItem<T>& item = mEventQueueMap[key];
        ADD_COUNTER(mInEventsTotal, g.GetEvents().size());
        ADD_COUNTER(mInGroupDataSizeBytes, g.DataSize());
        SET_GAUGE(mEventBatchItemsTotal, mEventQueueMap.size());

        if (g.DataSize() > mEventFlushStrategy.GetMinSizeBytes()) {
            // for group size larger than min batch size, separate group only if size is larger than max batch size
            if (!item.IsEmpty()) {
                UpdateMetricsOnFlushingEventQueue(item);
                item.Flush(res);
            }
            for (auto& e : g.MutableEvents()) {
                // should consider time condition here because sls require this
                if (!item.IsEmpty() && mEventFlushStrategy.NeedFlushByTime(item.GetStatus(), e)) {
                    ADD_COUNTER(mOutEventsTotal, item.EventSize());
                    item.Flush(res);
                }
                if (item.IsEmpty()) {
                    item.Reset(g.GetSizedTags(),
                               g.GetSourceBuffer(),
                               g.GetExactlyOnceCheckpoint(),
                               g.GetMetadata(EventGroupMetaKey::SOURCE_ID));
                }
                item.Add(std::move(e));
                if (mEventFlushStrategy.SizeReachingUpperLimit(item.GetStatus())) {
                    ADD_COUNTER(mOutEventsTotal, item.EventSize());
                    item.Flush(res);
                }
            }
            ADD_COUNTER(mOutEventsTotal, item.EventSize());
            item.Flush(res);
        } else {
            size_t eventsSize = g.GetEvents().size();
            for (size_t i = 0; i < eventsSize; ++i) {
                PipelineEventPtr& e = g.MutableEvents()[i];
                if (!item.IsEmpty() && mEventFlushStrategy.NeedFlushByTime(item.GetStatus(), e)) {
                    if (!mGroupQueue) {
                        UpdateMetricsOnFlushingEventQueue(item);
                        item.Flush(res);
                    } else {
                        if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
                            UpdateMetricsOnFlushingGroupQueue();
                            mGroupQueue->Flush(res);
                        }
                        if (mGroupQueue->IsEmpty()) {
                            TimeoutFlushManager::GetInstance()->UpdateRecord(mFlusher->GetContext().GetConfigName(),
                                                                             mFlusher->GetFlusherIndex(),
                                                                             0,
                                                                             mGroupFlushStrategy->GetTimeoutSecs(),
                                                                             mFlusher);
                        }
                        item.Flush(mGroupQueue.value());
                        if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
                            UpdateMetricsOnFlushingGroupQueue();
                            mGroupQueue->Flush(res);
                        }
                    }
                }
                if (item.IsEmpty()) {
                    item.Reset(g.GetSizedTags(),
                               g.GetSourceBuffer(),
                               g.GetExactlyOnceCheckpoint(),
                               g.GetMetadata(EventGroupMetaKey::SOURCE_ID));
                    TimeoutFlushManager::GetInstance()->UpdateRecord(mFlusher->GetContext().GetConfigName(),
                                                                     mFlusher->GetFlusherIndex(),
                                                                     key,
                                                                     mEventFlushStrategy.GetTimeoutSecs(),
                                                                     mFlusher);
                    ADD_GAUGE(mBufferedGroupsTotal, 1);
                    ADD_GAUGE(mBufferedDataSizeByte, item.DataSize());
                } else if (i == 0) {
                    item.AddSourceBuffer(g.GetSourceBuffer());
                }
                ADD_GAUGE(mBufferedEventsTotal, 1);
                ADD_GAUGE(mBufferedDataSizeByte, e->DataSize());
                item.Add(std::move(e));
                if (mEventFlushStrategy.NeedFlushBySize(item.GetStatus())
                    || mEventFlushStrategy.NeedFlushByCnt(item.GetStatus())) {
                    UpdateMetricsOnFlushingEventQueue(item);
                    item.Flush(res);
                }
            }
        }
        ADD_COUNTER(mTotalAddTimeMs, std::chrono::system_clock::now() - before);
    }

    // key != 0: event level queue
    // key = 0: group level queue
    void FlushQueue(size_t key, BatchedEventsList& res) {
        std::lock_guard<std::mutex> lock(mMux);
        if (key == 0) {
            if (!mGroupQueue) {
                return;
            }
            UpdateMetricsOnFlushingGroupQueue();
            return mGroupQueue->Flush(res);
        }

        auto iter = mEventQueueMap.find(key);
        if (iter == mEventQueueMap.end()) {
            return;
        }

        if (!mGroupQueue) {
            UpdateMetricsOnFlushingEventQueue(iter->second);
            iter->second.Flush(res);
            mEventQueueMap.erase(iter);
            SET_GAUGE(mEventBatchItemsTotal, mEventQueueMap.size());
            return;
        }

        if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
            UpdateMetricsOnFlushingGroupQueue();
            mGroupQueue->Flush(res);
        }
        if (mGroupQueue->IsEmpty()) {
            TimeoutFlushManager::GetInstance()->UpdateRecord(mFlusher->GetContext().GetConfigName(),
                                                             mFlusher->GetFlusherIndex(),
                                                             0,
                                                             mGroupFlushStrategy->GetTimeoutSecs(),
                                                             mFlusher);
        }
        iter->second.Flush(mGroupQueue.value());
        mEventQueueMap.erase(iter);
        SET_GAUGE(mEventBatchItemsTotal, mEventQueueMap.size());
        if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
            UpdateMetricsOnFlushingGroupQueue();
            mGroupQueue->Flush(res);
        }
    }

    void FlushAll(std::vector<BatchedEventsList>& res) {
        std::lock_guard<std::mutex> lock(mMux);
        for (auto& item : mEventQueueMap) {
            if (!mGroupQueue) {
                UpdateMetricsOnFlushingEventQueue(item.second);
                item.second.Flush(res);
            } else {
                if (!mGroupQueue->IsEmpty() && mGroupFlushStrategy->NeedFlushByTime(mGroupQueue->GetStatus())) {
                    UpdateMetricsOnFlushingGroupQueue();
                    mGroupQueue->Flush(res);
                }
                item.second.Flush(mGroupQueue.value());
                if (mGroupFlushStrategy->NeedFlushBySize(mGroupQueue->GetStatus())) {
                    UpdateMetricsOnFlushingGroupQueue();
                    mGroupQueue->Flush(res);
                }
            }
        }
        if (mGroupQueue) {
            UpdateMetricsOnFlushingGroupQueue();
            mGroupQueue->Flush(res);
        }
        SET_GAUGE(mEventBatchItemsTotal, 0);
        mEventQueueMap.clear();
    }

#ifdef APSARA_UNIT_TEST_MAIN
    EventFlushStrategy<T>& GetEventFlushStrategy() { return mEventFlushStrategy; }
    std::optional<GroupFlushStrategy>& GetGroupFlushStrategy() { return mGroupFlushStrategy; }
#endif

private:
    void UpdateMetricsOnFlushingEventQueue(const EventBatchItem<T>& item) {
        ADD_COUNTER(mOutEventsTotal, item.EventSize());
        // ADD_COUNTER(mTotalDelayMs,
        //             item.EventSize()
        //                     * std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
        //                           .time_since_epoch()
        //                           .count()
        //                 - item.TotalEnqueTimeMs());
        SUB_GAUGE(mBufferedGroupsTotal, 1);
        SUB_GAUGE(mBufferedEventsTotal, item.EventSize());
        SUB_GAUGE(mBufferedDataSizeByte, item.DataSize());
    }

    void UpdateMetricsOnFlushingGroupQueue() {
        ADD_COUNTER(mOutEventsTotal, mGroupQueue->EventSize());
        // ADD_COUNTER(mTotalDelayMs,
        //             mGroupQueue->EventSize()
        //                     * std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
        //                           .time_since_epoch()
        //                           .count()
        //                 - mGroupQueue->TotalEnqueTimeMs());
        SUB_GAUGE(mBufferedGroupsTotal, mGroupQueue->GroupSize());
        SUB_GAUGE(mBufferedEventsTotal, mGroupQueue->EventSize());
        SUB_GAUGE(mBufferedDataSizeByte, mGroupQueue->DataSize());
    }

    std::mutex mMux;
    std::map<size_t, EventBatchItem<T>> mEventQueueMap;
    EventFlushStrategy<T> mEventFlushStrategy;

    std::optional<GroupBatchItem> mGroupQueue;
    std::optional<GroupFlushStrategy> mGroupFlushStrategy;

    Flusher* mFlusher = nullptr;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInEventsTotal;
    CounterPtr mInGroupDataSizeBytes;
    CounterPtr mOutEventsTotal;
    // CounterPtr mTotalDelayMs;
    IntGaugePtr mEventBatchItemsTotal;
    IntGaugePtr mBufferedGroupsTotal;
    IntGaugePtr mBufferedEventsTotal;
    IntGaugePtr mBufferedDataSizeByte;
    TimeCounterPtr mTotalAddTimeMs;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class BatcherUnittest;
#endif
};

} // namespace logtail
