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

#include <unordered_map>
#include <atomic>
#include <array>
#include <set>
#include <queue>
#include <condition_variable>

#include "ebpf/SourceManager.h"
#include "BaseManager.h"
#include "ebpf/Config.h"
#include "common/Lock.h"
#include "ebpf/include/export.h"
#include "ebpf/util/AggregateTree.h"
#include "ebpf/type/SecurityEvent.h"
#include "common/timer/Timer.h"
#include "ebpf/type/CommonDataEvent.h"

// #include "driver/bpf_wrapper.h"
// #include "common/agg_tree.h"
// #include "type/security_event.h"

namespace logtail {
namespace ebpf {

class AbstractManager {
public:
    using configType = std::variant<std::monostate, logtail::ebpf::SecurityFileFilter,
                     logtail::ebpf::SecurityNetworkFilter>;
    AbstractManager() = delete;
    explicit AbstractManager(std::shared_ptr<BaseManager>, std::shared_ptr<SourceManager> sourceManager, moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue, std::shared_ptr<Timer> scheduler);
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

    bool IsRunning();

    int GetCallNameIdx(const std::string& call_name);

    virtual logtail::ebpf::PluginType GetPluginType() = 0;

    // virtual void InitAggregateTree() {
    //     aggregate_tree_ = nullptr;
    // }

    virtual void Suspend() {
        WriteLock lock(mMtx);
        // flag_ = false;
        mSuspendFlag = true;
        mRunnerCV.notify_all();
        return;
    }

    virtual void Resume() {
        WriteLock lock(mMtx);
        mSuspendFlag = false;
        mRunnerCV.notify_all();
        return;
    }

    void UpdateContext(const logtail::PipelineContext* ctx, logtail::QueueKey key, uint32_t index) {
        std::lock_guard lk(mContextMutex);
        mPipelineCtx = ctx;
        mQueueKey = key;
        mPluginIndex = index;
    }

    mutable ReadWriteLock mMtx;
    std::atomic<bool> mFlag = false;
    std::atomic<bool> mSuspendFlag = false;
protected:
    std::shared_ptr<BaseManager> mBaseManager;
    std::shared_ptr<SourceManager> mSourceManager;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& mCommonEventQueue;
    std::shared_ptr<Timer> mScheduler;

    mutable std::mutex mContextMutex;
    // mPipelineCtx/mQueueKey/mPluginIndex is guarded by mContextMutex
    const PipelineContext* mPipelineCtx{nullptr};
    logtail::QueueKey mQueueKey = 0;
    uint32_t mPluginIndex{0};

    // std::unique_ptr<SIZETAggTree<BaseSecurityNode, std::unique_ptr<BaseSecurityEvent>>> mAggregateTree;
    // static ...
    std::chrono::nanoseconds mTimeDiff;
    std::condition_variable mRunnerCV;

    // used for timer event ...
};

}
}
