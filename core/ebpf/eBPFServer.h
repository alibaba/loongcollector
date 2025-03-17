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
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <variant>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/SourceManager.h"
#include "ebpf/include/export.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "monitor/metric_models/MetricTypes.h"
#include "runner/InputRunner.h"
#include "type/CommonDataEvent.h"
#include "util/FrequencyManager.h"

namespace logtail {
namespace ebpf {

class EnvManager {
public:
    void InitEnvInfo();
    bool IsSupportedEnv(PluginType type);
    bool AbleToLoadDyLib();

private:
    volatile bool mInited = false;

    std::atomic_bool mArchSupport = false;
    std::atomic_bool mBTFSupport = false;
    std::atomic_bool m310Support = false;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

class eBPFServer : public InputRunner {
public:
    eBPFServer(const eBPFServer&) = delete;
    eBPFServer& operator=(const eBPFServer&) = delete;

    void Init() override;

    static eBPFServer* GetInstance() {
        static eBPFServer instance;
        return &instance;
    }

    void Stop() override;

    std::string CheckLoadedPipelineName(PluginType type);

    void UpdatePipelineName(PluginType type, const std::string& name, const std::string& project);

    bool EnablePlugin(const std::string& pipeline_name,
                      uint32_t plugin_index,
                      PluginType type,
                      const logtail::CollectionPipelineContext* ctx,
                      const std::variant<SecurityOptions*, ObserverNetworkOption*> options,
                      PluginMetricManagerPtr mgr);

    bool DisablePlugin(const std::string& pipeline_name, PluginType type);

    bool SuspendPlugin(const std::string& pipeline_name, PluginType type);

    bool HasRegisteredPlugins() const override;

    bool IsSupportedEnv(PluginType type);

    std::string GetAllProjects();

    bool CheckIfNeedStopProcessCacheManager() const;

    void PollPerfBuffers();
    void HandlerEvents();

    std::shared_ptr<AbstractManager> GetPluginManager(PluginType type);
    void UpdatePluginManager(PluginType type, std::shared_ptr<AbstractManager>);

private:
    bool StartPluginInternal(const std::string& pipeline_name,
                             uint32_t plugin_index,
                             PluginType type,
                             const logtail::CollectionPipelineContext* ctx,
                             const std::variant<SecurityOptions*, ObserverNetworkOption*> options,
                             PluginMetricManagerPtr mgr);
    eBPFServer() : mSourceManager(std::make_shared<SourceManager>()), mDataEventQueue(4096) {}
    ~eBPFServer() = default;

    void
    UpdateCBContext(PluginType type, const logtail::CollectionPipelineContext* ctx, logtail::QueueKey key, int idx);

    std::shared_ptr<SourceManager> mSourceManager;

    mutable std::mutex mMtx;
    std::array<std::string, static_cast<size_t>(PluginType::MAX)> mLoadedPipeline = {};
    std::array<std::string, static_cast<size_t>(PluginType::MAX)> mPluginProject = {};
    std::array<std::shared_ptr<AbstractManager>, static_cast<size_t>(PluginType::MAX)> mPlugins = {};

    eBPFAdminConfig mAdminConfig;
    std::atomic_bool mInited = false;
    std::atomic_bool mRunning = false;

    std::string mHostIp;
    std::string mHostName;
    std::filesystem::path mHostPathPrefix;

    EnvManager mEnvMgr;
    MetricsRecordRef mRef;

    CounterPtr mStartPluginTotal;
    CounterPtr mStopPluginTotal;
    CounterPtr mSuspendPluginTotal;
    CounterPtr mPollProcessEventsTotal;
    CounterPtr mLossProcessEventsTotal;
    CounterPtr mProcessCacheMissTotal;
    IntGaugePtr mProcessCacheSize;

    // hold some managers ...
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;

    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mDataEventQueue;

    std::future<void> mPoller;
    std::future<void> mHandler;

    FrequencyManager mFrequencyMgr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
