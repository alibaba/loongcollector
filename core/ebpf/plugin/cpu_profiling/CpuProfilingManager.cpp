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
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/queue/blockingconcurrentqueue.h"
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

    return ProcessScanner::GetInstance()->RegisterScan({
        .mName = mConfigName,
        .mRegexs = opts->mCmdlines,
        .mCallback =
            [this](std::vector<uint32_t> pids) {
                std::string pidsStr;
                for (auto pid : pids) {
                    pidsStr += std::to_string(pid) + ",";
                }
                LOG_DEBUG(sLogger, ("CpuProfilingManager update config",
                                    "")("pids", pidsStr));
                mEBPFAdapter->UpdatePlugin(
                    PluginType::CPU_PROFILING,
                    buildCpuProfilingConfig(std::move(pids), nullptr, nullptr));
            },
    });
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

using StackCnt = std::pair<std::string, uint32_t>;

static void parseStackCnt(char const *symbol, std::vector<StackCnt> &result) {
    // Format: "<comm>:<pid>;<stacks> <cnt>\n"
    // Example: "bash:1234;func1;func2;func3 10\n"

    std::istringstream ssymbol;
    ssymbol.str(symbol);
    std::string line;
    while (std::getline(ssymbol, line)) {
        auto pos1 = line.find(';');
        if (pos1 == std::string::npos) {
            LOG_ERROR(sLogger, ("Invalid symbol format", line));
            continue;
        }
        auto pos2 = line.find(' ');
        if (pos2 == std::string::npos || pos2 < pos1) {
            LOG_ERROR(sLogger, ("Invalid symbol format", line));
            continue;
        }

        auto stack = line.substr(pos1 + 1, pos2 - pos1 - 1);
        auto cntStr = line.substr(pos2 + 1);
        uint32_t cnt = std::stoul(cntStr);

        result.push_back(std::make_pair(stack, cnt));
    }
}

void CpuProfilingManager::HandleCpuProfilingEvent(uint32_t pid,
                                                  const char *comm,
                                                  const char *stack,
                                                  uint32_t cnt) {
    LOG_DEBUG(sLogger, ("CpuProfilingEvent", "")("pid", pid)("comm", comm)(
                           "stack", stack)("cnt", cnt));

    std::vector<StackCnt> stacks;
    parseStackCnt(stack, stacks);

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    for (auto &[stack, cnt] : stacks) {
        auto* event = eventGroup.AddLogEvent();
        event->SetContent("pid", std::to_string(pid));
        event->SetContent("comm", std::string(comm));
        event->SetContent("stack", std::string(stack));
        event->SetContent("cnt", std::to_string(cnt));
    }

    {
        if (!mPipelineCtx) {
            return;
        }

        std::unique_ptr<ProcessQueueItem> item =
            std::make_unique<ProcessQueueItem>(std::move(eventGroup),
                                               mPluginIndex);
        int maxRetry = 5;
        for (int retry = 0; retry < maxRetry; ++retry) {
            if (QueueStatus::OK ==
                ProcessQueueManager::GetInstance()->PushQueue(
                    mQueueKey, std::move(item))) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (retry == maxRetry - 1) {
                LOG_WARNING(
                    sLogger,
                    ("configName", info.mPipelineCtx->GetConfigName())(
                        "pluginIdx", info.mPluginIndex)(
                        "[CpuProfilingEvent] push queue failed!", ""));
                // TODO: Alarm discard data
            }
        }
    }
};

} // namespace ebpf
} // namespace logtail