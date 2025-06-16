// Copyright 2025 iLogtail Authors
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

#pragma once

#include <atomic>
#include <vector>

#include "common/Lock.h"
#include "common/magic_enum.hpp"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/include/export.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/AggregateEvent.h"
#include "ebpf/type/CommonDataEvent.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {

class AbstractManager {
public:
    inline static constexpr StringView kKprobeValue = "kprobe";

    AbstractManager() = delete;
    explicit AbstractManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                             const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                             moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                             const PluginMetricManagerPtr& metricManager);
    virtual ~AbstractManager();

    virtual int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) = 0;

    virtual int Destroy() = 0;

    virtual int HandleEvent(const std::shared_ptr<CommonEvent>& event) = 0;

    virtual int PollPerfBuffer() {
        int zero = 0;
        // TODO(@qianlu.kk): do we need to hold some events for a while and enqueue bulk??
        // the max_events doesn't work so far
        // and if there is no managers at all, this thread will occupy the cpu
        return mEBPFAdapter->PollPerfBuffers(
            GetPluginType(), kDefaultMaxBatchConsumeSize, &zero, kDefaultMaxWaitTimeMS);
    }

    bool IsRunning() { return mFlag && !mSuspendFlag; }

    bool IsExists() { return mFlag; }

    virtual bool ScheduleNext(const std::chrono::steady_clock::time_point& execTime,
                              const std::shared_ptr<ScheduleConfig>& config)
        = 0;

    virtual PluginType GetPluginType() = 0;

    virtual int Suspend() {
        WriteLock lock(mMtx);
        mSuspendFlag = true;
        bool ret = mEBPFAdapter->SuspendPlugin(GetPluginType());
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to suspend plugin", magic_enum::enum_name(GetPluginType())));
            return 1;
        }
        return 0;
    }

    virtual int Resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
        {
            WriteLock lock(mMtx);
            mSuspendFlag = false;
        }
        bool ret = mEBPFAdapter->ResumePlugin(GetPluginType(), GeneratePluginConfig(options));
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to resume plugin", magic_enum::enum_name(GetPluginType())));
            return 1;
        }
        return 0;
    }

    virtual std::unique_ptr<PluginConfig>
    GeneratePluginConfig([[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) = 0;

    virtual int Update([[maybe_unused]] const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) {
        bool ret = mEBPFAdapter->UpdatePlugin(GetPluginType(), GeneratePluginConfig(options));
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to update plugin", magic_enum::enum_name(GetPluginType())));
            return 1;
        }
        return 0;
    }

    void UpdateContext(const CollectionPipelineContext* ctx, logtail::QueueKey key, uint32_t index) {
        std::lock_guard lk(mContextMutex);
        mPipelineCtx = ctx;
        mQueueKey = key;
        mPluginIndex = index;
    }

    std::shared_ptr<ProcessCacheManager> GetProcessCacheManager() const { return mProcessCacheManager; }

private:
    mutable ReadWriteLock mMtx; // lock
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;

protected:
    std::atomic<bool> mFlag = false;
    std::atomic<bool> mSuspendFlag = false;
    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;
    PluginMetricManagerPtr mMetricMgr;

    mutable std::mutex mContextMutex;
    // mPipelineCtx/mQueueKey/mPluginIndex is guarded by mContextMutex
    const CollectionPipelineContext* mPipelineCtx{nullptr};
    logtail::QueueKey mQueueKey = 0;
    uint32_t mPluginIndex{0};

    CounterPtr mRecvKernelEventsTotal;
    CounterPtr mLossKernelEventsTotal;
    CounterPtr mPushLogsTotal;
    CounterPtr mPushLogGroupTotal;

    std::vector<MetricLabels> mRefAndLabels;
};

} // namespace logtail::ebpf
