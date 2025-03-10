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

#include <coolbpf/security/type.h>

#include <memory>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"

namespace logtail {
namespace ebpf {
class ProcessSecurityManager : public AbstractManager {
public:
    static const std::string kExitTidKey;
    static const std::string kExitCodeKey;
    static const std::string kExecveValue;
    static const std::string kCloneValue;
    static const std::string kExitValue;

    ProcessSecurityManager() = delete;
    ProcessSecurityManager(std::shared_ptr<ProcessCacheManager>& baseMgr,
                           std::shared_ptr<SourceManager> sourceManager,
                           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                           PluginMetricManagerPtr mgr);

    static std::shared_ptr<ProcessSecurityManager>
    Create(std::shared_ptr<ProcessCacheManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           PluginMetricManagerPtr metricMgr) {
        return std::make_shared<ProcessSecurityManager>(mgr, sourceManager, queue, metricMgr);
    }

    ~ProcessSecurityManager() {}
    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>& options) override;
    int Destroy() override;

    PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    int HandleEvent(const std::shared_ptr<CommonEvent>& event) override;

    bool ConsumeAggregateTree(const std::chrono::steady_clock::time_point& execTime);

    // process perfbuffer was polled by processCacheManager ...
    int PollPerfBuffer() override { return 0; }

    std::unique_ptr<PluginConfig> GeneratePluginConfig(
        [[maybe_unused]] const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>& options)
        override {
        auto ebpfConfig = std::make_unique<PluginConfig>();
        ebpfConfig->mPluginType = PluginType::PROCESS_SECURITY;
        return ebpfConfig;
    }

    int Update([[maybe_unused]] const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*>& options)
        override {
        // do nothing ...
        LOG_WARNING(sLogger, ("would do nothing", ""));
        return 0;
    }

private:
    ReadWriteLock mLock;
    SIZETAggTree<ProcessEventGroup, std::shared_ptr<CommonEvent>> mAggregateTree;
};

} // namespace ebpf
} // namespace logtail
