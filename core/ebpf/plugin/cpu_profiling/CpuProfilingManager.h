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

#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"

namespace logtail::ebpf {

class CpuProfilingManager : public AbstractManager {
public:
    CpuProfilingManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                        const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                        EventPool* pool);
    ~CpuProfilingManager() override = default;

    static std::shared_ptr<CpuProfilingManager>
    Create(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
           const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           EventPool* pool) {
        return std::make_shared<CpuProfilingManager>(processCacheManager, eBPFAdapter, queue, pool);
    }

    int Init() override;
    int Destroy() override;

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override { return 0; }

    int SendEvents() override { return 0; }

    int RegisteredConfigCount() override { return mConfigNameToKey.size(); }

    int AddOrUpdateConfig(const CollectionPipelineContext* context,
                          uint32_t configId,
                          const PluginMetricManagerPtr& metricManager,
                          const PluginOptions& options) override;

    int RemoveConfig(const std::string& configName) override;

    PluginType GetPluginType() override { return PluginType::CPU_PROFILING; }

    std::unique_ptr<PluginConfig> GeneratePluginConfig(const PluginOptions& options) override {
        assert(false);
        return nullptr;
    }

    int Suspend() override;

    void HandleCpuProfilingEvent(uint32_t pid, const char* comm, const char* stack, uint32_t cnt);

    void HandleProcessDiscoveryEvent(ProcessDiscoveryManager::DiscoverResult result);

    void SetMetrics(CounterPtr pollEventsTotal) { mRecvKernelEventsTotal = std::move(pollEventsTotal); }

protected:
    int update([[maybe_unused]] const PluginOptions& options) override {
        assert(false);
        return 0;
    }

private:
    std::atomic<bool> mInited = false;

    using ConfigKey = size_t;
    struct ConfigInfo {
        const CollectionPipelineContext* mPipelineCtx{nullptr};
        logtail::QueueKey mQueueKey = 0;
        uint32_t mPluginIndex{0};
    };
    ConfigKey mNextKey = 0;
    std::unordered_map<std::string, ConfigKey> mConfigNameToKey;
    std::unordered_map<ConfigKey, ConfigInfo> mConfigInfoMap;

    std::mutex mMutex;
    std::unordered_map<uint32_t, std::unordered_set<ConfigKey>> mRouter;

    // runner metrics
    CounterPtr mRecvKernelEventsTotal;

    friend class CpuProfilingManagerUnittest;
};

} // namespace logtail::ebpf
