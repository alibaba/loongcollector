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

#include "ebpf/driver/coolbpf/src/security/type.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/type/NetworkObserverEvent.h"

namespace logtail {
namespace ebpf {
class ProcessSecurityManager : public AbstractManager {
public:
    // static std::shared_ptr<ProcessSecurityManager> Create(std::shared_ptr<BaseManager>& mgr,
    // std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper) {
    //     return std::make_shared<ProcessSecurityManager>(mgr, wrapper);
    // }
    ProcessSecurityManager() = delete;
    ProcessSecurityManager(std::shared_ptr<BaseManager>& baseMgr, std::shared_ptr<SourceManager> sourceManager, moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue)
        : AbstractManager(baseMgr, sourceManager, queue) {}

    static std::shared_ptr<ProcessSecurityManager>
    Create(std::shared_ptr<BaseManager>& mgr,
           std::shared_ptr<SourceManager> sourceManager,
           moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue) {
        return std::make_shared<ProcessSecurityManager>(mgr, sourceManager, queue);
    }

    ~ProcessSecurityManager() {}
    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;

    virtual PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override;

    // process perfbuffer was polled by baseManager ...
    virtual int PollPerfBuffer() override { return 0; }

    int EnableCallName(const std::string& call_name, const configType config) override {return 0;}
    int DisableCallName(const std::string& call_name) override {return 0;}

private:
    std::unique_ptr<SIZETAggTree<ProcessEventGroup, std::shared_ptr<ProcessEvent>>> mAggregateTree;

};

} // namespace ebpf
} // namespace logtail
