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

#include <variant>
#include <atomic>
#include <map>
#include <array>
#include <memory>
#include <mutex>

#include "runner/InputRunner.h"
#include "pipeline/PipelineContext.h"
#include "ebpf/SourceManager.h"
#include "ebpf/config.h"
#include "ebpf/include/export.h"
#include "ebpf/handler/AbstractHandler.h"
#include "ebpf/handler/ObserveHandler.h"
#include "ebpf/handler/SecurityHandler.h"

namespace logtail {
namespace ebpf {

class EnvManager {
public:
    void InitEnvInfo();
    bool IsSupportedEnv(nami::PluginType type);
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

    std::string CheckLoadedPipelineName(nami::PluginType type);

    void UpdatePipelineName(nami::PluginType type, const std::string& name);

    bool EnablePlugin(const std::string& pipeline_name, uint32_t plugin_index,
                        nami::PluginType type, 
                        const logtail::PipelineContext* ctx, 
                        const std::variant<SecurityOptions*, nami::ObserverNetworkOption*> options);

    bool DisablePlugin(const std::string& pipeline_name, nami::PluginType type);

    bool SuspendPlugin(const std::string& pipeline_name, nami::PluginType type);

    bool HasRegisteredPlugins() const override;

    bool IsSupportedEnv(nami::PluginType type);

private:
    bool StartPluginInternal(const std::string& pipeline_name, uint32_t plugin_index,
                        nami::PluginType type, 
                        const logtail::PipelineContext* ctx, 
                        const std::variant<SecurityOptions*, nami::ObserverNetworkOption*> options);
    eBPFServer() = default;
    ~eBPFServer() = default;

    void UpdateCBContext(nami::PluginType type, const logtail::PipelineContext* ctx, logtail::QueueKey key, int idx);

    std::unique_ptr<SourceManager> mSourceManager;
    // source manager
    std::unique_ptr<EventHandler> mEventCB;
    std::unique_ptr<MeterHandler> mMeterCB;
    std::unique_ptr<SpanHandler> mSpanCB;
    std::unique_ptr<SecurityHandler> mNetworkSecureCB;
    std::unique_ptr<SecurityHandler> mProcessSecureCB;
    std::unique_ptr<SecurityHandler> mFileSecureCB;

    mutable std::mutex mMtx;
    std::array<std::string, (int)nami::PluginType::MAX> mLoadedPipeline = {};

    eBPFAdminConfig mAdminConfig;
    volatile bool mInited = false;

    EnvManager mEnvMgr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class eBPFServerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
