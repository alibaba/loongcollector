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

#include "ebpf/plugin/cpu_profiling/CpuProfilingManager.h"
#include "CpuProfilingManager.h"
#include "ebpf/plugin/cpu_profiling/ProcessScanner.h"

namespace logtail {
namespace ebpf {

std::unique_ptr<PluginConfig>
buildCpuProfilingConfig(std::vector<uint32_t> pids, CpuProfilingHandler handler,
                        void *ctx) {
    CpuProfilingConfig config = {
        .mPids = std::move(pids), .mHandler = handler, .mCtx = ctx};
    auto pc = std::make_unique<PluginConfig>();
    pc->mPluginType = PluginType::CPU_PROFILING;
    pc->mConfig = std::move(config);
    return pc;
}

void handleCpuProfilingEvent(uint32_t pid, const char *comm, const char *stack,
                             uint32_t cnt, void *ctx) {
    auto *self = static_cast<CpuProfilingManager *>(ctx);
    assert(self != nullptr);
    self->HandleCpuProfilingEvent(pid, comm, stack, cnt);
}

CpuProfilingManager::CpuProfilingManager(
    const std::shared_ptr<ProcessCacheManager> &processCacheManager,
    const std::shared_ptr<EBPFAdapter> &eBPFAdapter,
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> &queue,
    EventPool* pool)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, pool) {}

int CpuProfilingManager::Init() {
    if (mInited) {
        return 0;
    }
    mInited = true;
    mEBPFAdapter->StartPlugin(
        PluginType::CPU_PROFILING,
        buildCpuProfilingConfig({}, handleCpuProfilingEvent, this));
    ProcessScanner::GetInstance()->Start();
    LOG_INFO(sLogger, ("CpuProfilingManager initialized", ""));
    return 0;
}

int CpuProfilingManager::Destroy() {
    if (!mInited) {
        return 0;
    }
    mInited = false;
    ProcessScanner::GetInstance()->Stop();
    mEBPFAdapter->StopPlugin(PluginType::CPU_PROFILING);
    LOG_INFO(sLogger, ("CpuProfilingManager destroyed", ""));
    return 0;
}

int CpuProfilingManager::AddOrUpdateConfig(
    const CollectionPipelineContext *context, uint32_t index,
    const PluginMetricManagerPtr &metricManager, const PluginOptions &options) {
    // TODO: add metrics later

    // TODO: support multiple configs
    if (mConfigName.empty()) {
        mConfigName = context->GetConfigName();
        mPluginIndex = index;
        mPipelineCtx = context;
        mQueueKey = context->GetProcessQueueKey();
        mRegisteredConfigCount = 1;
    }

    assert(mConfigName == context->GetConfigName());

    CpuProfilingOption *opts = std::get<CpuProfilingOption *>(options);

    ProcessScanner::GetInstance()->RegisterScan({
        .mName = mConfigName,
        .mRegexs = opts->mCmdlines,
        .mCallback =
            [this](std::vector<uint32_t> pids) {
                mEBPFAdapter->UpdatePlugin(
                    PluginType::CPU_PROFILING,
                    buildCpuProfilingConfig(std::move(pids), nullptr, nullptr));
            },
    });

    return 0;
}

int CpuProfilingManager::RemoveConfig(const std::string &configName) {
    // TODO: support multiple configs
    assert(mConfigName == configName);
    ProcessScanner::GetInstance()->RemoveScan(mConfigName);
    mConfigName.clear();
    mPipelineCtx = nullptr;
    mQueueKey = 0;
    mPluginIndex = 0;
    mRegisteredConfigCount = 0;
    return 0;
}

int CpuProfilingManager::Suspend() {
    ProcessScanner::GetInstance()->RemoveScan(mConfigName);
    mEBPFAdapter->SuspendPlugin(PluginType::CPU_PROFILING);
    LOG_INFO(sLogger, ("CpuProfilingManager suspended", ""));
    return 0;
}

void CpuProfilingManager::HandleCpuProfilingEvent(uint32_t pid,
                                                  const char *comm,
                                                  const char *stack,
                                                  uint32_t cnt) {
    LOG_INFO(sLogger,
             ("CpuProfilingManager HandleCpuProfilingEvent",
              "")("pid", std::to_string(pid))("comm", std::string(comm))(
                 "stack", std::string(stack))("cnt", std::to_string(cnt)));
};

} // namespace ebpf
} // namespace logtail