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

#include <memory>
#include <mutex>
#include <thread>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/driver/coolbpf/src/security/type.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/NetworkObserverEvent.h"

namespace logtail {
namespace ebpf {
class FileSecurityManager : public AbstractManager {
public:
    FileSecurityManager() = delete;
    FileSecurityManager(std::shared_ptr<BaseManager>& baseMgr,
                        std::shared_ptr<SourceManager> sourceManager,
                        moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                        std::shared_ptr<Timer> scheduler);
                        
    static std::shared_ptr<FileSecurityManager>
    Create(std::shared_ptr<BaseManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
           std::shared_ptr<Timer> scheduler) {
        return std::make_shared<FileSecurityManager>(mgr, sourceManager, queue, scheduler);
    }

    ~FileSecurityManager() {}
    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;

    void RecordFileEvent(file_data_t* event);

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override;

    virtual PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    virtual std::unique_ptr<PluginConfig> GeneratePluginConfig(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
        std::unique_ptr<PluginConfig> pc = std::make_unique<PluginConfig>();
        pc->mPluginType = PluginType::FILE_SECURITY;
        FileSecurityConfig config;
        SecurityOptions* opts = std::get<SecurityOptions*>(options);
        config.options_ = opts->mOptionList;
        pc->mConfig = std::move(config);
        return pc;
    }

private:
    ReadWriteLock mLock;
    SIZETAggTree<FileEventGroup, std::shared_ptr<FileEvent>> mAggregateTree;
};

} // namespace ebpf
} // namespace logtail
