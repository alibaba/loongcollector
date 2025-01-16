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
class FileSecurityManager : public AbstractManager {
public:
    // static std::shared_ptr<FileSecurityManager> Create(std::shared_ptr<BaseManager>& mgr,
    // std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper) {
    //     return std::make_shared<FileSecurityManager>(mgr, wrapper);
    // }
    FileSecurityManager() = delete;
    FileSecurityManager(std::shared_ptr<BaseManager>& baseMgr, std::shared_ptr<SourceManager> sourceManager, moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue, std::shared_ptr<Timer> scheduler)
        : AbstractManager(baseMgr, sourceManager, queue, scheduler) {}

    ~FileSecurityManager();
    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;

    void RecordFileEvent(file_data_t* event);

    virtual int HandleEvent(const std::shared_ptr<CommonEvent> event) override {return 0;}

    void FlushFilekEvent();

    virtual PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    int EnableCallName(const std::string& call_name, const configType config) override;
    int DisableCallName(const std::string& call_name) override;

private:
    mutable moodycamel::BlockingConcurrentQueue<std::unique_ptr<FileSecurityNode>> mEventQueue;
};

} // namespace ebpf
} // namespace logtail
