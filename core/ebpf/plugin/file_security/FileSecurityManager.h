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

#include <coolbpf/security/type.h>

#include <memory>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/util/AggregateTree.h"

namespace logtail::ebpf {

class FileSecurityManager : public AbstractManager {
public:
    static const std::string kMmapValue;
    static const std::string kTruncateValue;
    static const std::string kPermissionValue;
    static const std::string kPermissionReadValue;
    static const std::string kPermissionWriteValue;

    FileSecurityManager() = delete;
    FileSecurityManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                        const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                        const PluginMetricManagerPtr& metricManager);

    static std::shared_ptr<FileSecurityManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           const PluginMetricManagerPtr& metricMgr) {
        return std::make_shared<FileSecurityManager>(processCacheManager, eBPFAdapter, queue, metricMgr);
    }

    ~FileSecurityManager() {}
    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override;
    int Destroy() override;

    void RecordFileEvent(file_data_t* event);
    void UpdateLossKernelEventsTotal(uint64_t cnt);

    bool ConsumeAggregateTree(const std::chrono::steady_clock::time_point& execTime);

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override;

    int SendEvents() override;
    int PollPerfBuffer(int maxWaitTimeMs) override;

    PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    bool ScheduleNext(const std::chrono::steady_clock::time_point&, const std::shared_ptr<ScheduleConfig>&) override {
        return true;
    }

    std::unique_ptr<PluginConfig>
    GeneratePluginConfig(const std::variant<SecurityOptions*, ObserverNetworkOption*>& options) override {
        std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
        pc->mPluginType = PluginType::FILE_SECURITY;
        FileSecurityConfig config;
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        config.mOptions = opts->mOptionList;
        pc->mConfig = std::move(config);
        return pc;
    }

    FileRetryableEvent* CreateFileRetryableEvent(file_data_t* eventPtr);
    RetryableEventCache& EventCache() { return mRetryableEventCache; }

private:
    RetryableEventCache mRetryableEventCache;
    int64_t mLastEventCacheRetryTime = 0;

    ReadWriteLock mLock;
    int64_t mSendIntervalMs = 400;
    int64_t mLastSendTimeMs = 0;
    SIZETAggTree<FileEventGroup, std::shared_ptr<CommonEvent>> mAggregateTree;
};

} // namespace logtail::ebpf
