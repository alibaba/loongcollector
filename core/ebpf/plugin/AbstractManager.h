// Copyright 2023 iLogtail Authors
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

#include <array>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <set>
#include <unordered_map>

#include "common/Lock.h"
#include "common/magic_enum.hpp"
#include "common/queue/blockingconcurrentqueue.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/SourceManager.h"
#include "ebpf/include/export.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/util/AggregateTree.h"

namespace logtail {
namespace ebpf {

class AbstractManager {
public:
    static const std::string sCallNameKey;
    static const std::string sEventTypeKey;
    static const std::string sKprobeValue;

    AbstractManager() = delete;
    explicit AbstractManager(std::shared_ptr<ProcessCacheManager>,
                             std::shared_ptr<SourceManager> sourceManager,
                             moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                             std::shared_ptr<Timer> scheduler);
    virtual ~AbstractManager() {}

    virtual int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) = 0;

    virtual int Destroy() = 0;

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) = 0;

    virtual int PollPerfBuffer() {
        int zero = 0;
        // TODO @qianlu.kk do we need to hold some events for a while and enqueue bulk??
        // the max_events doesn't work so far
        // and if there is no managers at all, this thread will occupy the cpu
        return mSourceManager->PollPerfBuffers(GetPluginType(), 1024, &zero, 200);
    }

    bool IsRunning() { return mFlag && !mSuspendFlag; }

    int GetCallNameIdx(const std::string& call_name);

    virtual logtail::ebpf::PluginType GetPluginType() = 0;

    virtual int Suspend() {
        WriteLock lock(mMtx);
        mSuspendFlag = true;
        bool ret = mSourceManager->SuspendPlugin(GetPluginType());
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to suspend plugin", magic_enum::enum_name(GetPluginType())));
            return 1;
        }
        return 0;
    }

    virtual int Resume(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
        {
            WriteLock lock(mMtx);
            mSuspendFlag = false;
        }
        bool ret = mSourceManager->ResumePlugin(GetPluginType(), GeneratePluginConfig(options));
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to resume plugin", magic_enum::enum_name(GetPluginType())));
            return 1;
        }
        return 0;
    }

    virtual std::unique_ptr<PluginConfig>
    GeneratePluginConfig(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) = 0;

    virtual int Update(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
        bool ret = mSourceManager->UpdatePlugin(GetPluginType(), GeneratePluginConfig(options));
        if (!ret) {
            LOG_ERROR(sLogger, ("failed to resume plugin", magic_enum::enum_name(GetPluginType())));
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

    void UpdateBaseManager(std::shared_ptr<ProcessCacheManager> other) {
        WriteLock lk(mBaseMgrLock);
        mBaseManager = other;
    }

    std::shared_ptr<ProcessCacheManager> GetBaseManager() const {
        ReadLock lk(mBaseMgrLock);
        return mBaseManager;
    }

    mutable ReadWriteLock mMtx;
    std::atomic<bool> mFlag = false;
    std::atomic<bool> mSuspendFlag = false;

private:
    mutable ReadWriteLock mBaseMgrLock;
    std::shared_ptr<ProcessCacheManager> mBaseManager;

protected:
    std::shared_ptr<SourceManager> mSourceManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;
    std::shared_ptr<Timer> mScheduler;

    mutable std::mutex mContextMutex;
    // mPipelineCtx/mQueueKey/mPluginIndex is guarded by mContextMutex
    const CollectionPipelineContext* mPipelineCtx{nullptr};
    logtail::QueueKey mQueueKey = 0;
    uint32_t mPluginIndex{0};

    // static ...
    std::chrono::nanoseconds mTimeDiff;
    std::atomic_int mStartUid = 0;
};

} // namespace ebpf
} // namespace logtail
