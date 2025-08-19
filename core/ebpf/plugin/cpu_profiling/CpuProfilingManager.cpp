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
#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"
#include "ebpf/type/table/ProfileTable.h"

namespace logtail {
namespace ebpf {

std::unique_ptr<PluginConfig>
buildCpuProfilingConfig(std::unordered_set<uint32_t> pids, CpuProfilingHandler handler,
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
    ProcessDiscoveryManager::GetInstance()->Start([this](auto v) {
        HandleProcessDiscoveryEvent(std::move(v));
    });
    LOG_INFO(sLogger, ("CpuProfilingManager", "init"));
    return 0;
}

int CpuProfilingManager::Destroy() {
    if (!mInited) {
        return 0;
    }
    mInited = false;
    ProcessDiscoveryManager::GetInstance()->Stop();
    mEBPFAdapter->StopPlugin(PluginType::CPU_PROFILING);
    LOG_INFO(sLogger, ("CpuProfilingManager", "destroy"));
    return 0;
}

int CpuProfilingManager::AddOrUpdateConfig(
    const CollectionPipelineContext *context, uint32_t index,
    const PluginMetricManagerPtr &metricManager, const PluginOptions &options) {
    // TODO: add metrics later

    auto configName = context->GetConfigName();
    auto it = mConfigNameToKey.find(configName);
    if (it == mConfigNameToKey.end()) {
        auto key = mNextKey++;
        it = mConfigNameToKey.emplace(configName, key).first;
    }
    auto key = it->second;

    auto info = ConfigInfo{
        .mPipelineCtx = context,
        .mQueueKey = context->GetProcessQueueKey(),
        .mPluginIndex = index,
    };
    mConfigInfoMap.insert_or_assign(key, info);

    CpuProfilingOption *opts = std::get<CpuProfilingOption *>(options);

    ProcessDiscoveryManager::GetInstance()->AddOrUpdateDiscovery(
        configName, [&](ProcessDiscoveryConfig& config) {
            config.mConfigKey = key;
            config.mFullDiscovery = false;
            config.mRegexs.clear();
            if (opts->mCmdlines.empty()) {
                config.mFullDiscovery = true;
            }
            for (auto& cmdStr : opts->mCmdlines) {
                try {
                    config.mRegexs.emplace_back(cmdStr);
                } catch (boost::regex_error& e) {
                    LOG_ERROR(sLogger,
                        ("CpuProfilingManager", "failed to compile regex")
                        ("pattern", cmdStr)("error", e.what()));
                    continue;
                }
            }
        });

    LOG_DEBUG(sLogger, ("CpuProfilingManager", "add or update config")("config", configName));
    
    return 0;
}

int CpuProfilingManager::RemoveConfig(const std::string &configName) {
    auto it = mConfigNameToKey.find(configName);
    assert(it != mConfigNameToKey.end());
    auto key = it->second;
    mConfigNameToKey.erase(it);

    ProcessDiscoveryManager::GetInstance()->RemoveDiscovery(configName);

    auto hit = mConfigInfoMap.erase(key);
    assert(hit);

    LOG_DEBUG(sLogger, ("CpuProfilingManager", "remove config")("config", configName));

    return 0;
}

int CpuProfilingManager::Suspend() {
    // Do nothing
    LOG_INFO(sLogger, ("CpuProfilingManager", "suspend"));
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
    std::unordered_set<ConfigKey> targets;
    {
        std::lock_guard guard(mMutex);
        auto it = mRouter.find(pid);
        if (it != mRouter.end()) {
            targets = it->second;
        }
    }

    LOG_DEBUG(sLogger, ("CpuProfilingEvent", "")("pid", pid)("comm", comm)(
                           "stack", stack)("cnt", cnt)("send to queues num", targets.size()));

    if (targets.empty()) {
        return;
    }

    std::vector<StackCnt> stacks;
    parseStackCnt(stack, stacks);

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    
    auto pidSb = sourceBuffer->CopyString(std::to_string(pid));
    auto commSb = sourceBuffer->CopyString(std::string(comm));
    for (auto &[stack, cnt] : stacks) {
        auto* event = eventGroup.AddLogEvent();
        auto stackSb = sourceBuffer->CopyString(stack);
        auto cntSb = sourceBuffer->CopyString(std::to_string(cnt));
        event->SetContentNoCopy(kPid.LogKey(), StringView(pidSb.data, pidSb.size));
        event->SetContentNoCopy(kComm.LogKey(), StringView(commSb.data, commSb.size));
        event->SetContentNoCopy(kStack.LogKey(), StringView(stackSb.data, stackSb.size));
        event->SetContentNoCopy(kCnt.LogKey(), StringView(cntSb.data, cntSb.size));
    }

    for (auto& key : targets) {
        auto it = mConfigInfoMap.find(key);
        if (it == mConfigInfoMap.end()) {
            continue;
        }
        ConfigInfo& info = it->second;

        std::unique_ptr<ProcessQueueItem> item =
            std::make_unique<ProcessQueueItem>(
                eventGroup.Copy(),
                info.mPluginIndex);

        int maxRetry = 5;
        for (int retry = 0; retry < maxRetry; ++retry) {
            if (QueueStatus::OK ==
                ProcessQueueManager::GetInstance()->PushQueue(
                    info.mQueueKey, std::move(item))) {
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

void CpuProfilingManager::HandleProcessDiscoveryEvent(ProcessDiscoveryManager::DiscoverResult result) {
    std::unordered_set<uint32_t> totalPids;
    {
        std::lock_guard guard(mMutex);
        mRouter.clear();
        for (auto& [configKey, pids] : result) {
            for (auto& pid : pids) {
                totalPids.insert(pid);
                auto it = mRouter.emplace(pid, std::unordered_set<ConfigKey>{}).first;
                auto& configSet = it->second;
                configSet.insert(configKey);
            }
        }
    }

    mEBPFAdapter->UpdatePlugin(
        PluginType::CPU_PROFILING,
        buildCpuProfilingConfig(std::move(totalPids), nullptr, nullptr));
}

} // namespace ebpf
} // namespace logtail