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
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/UUIDUtil.h"
#include "ebpf/plugin/cpu_profiling/ProcessDiscoveryManager.h"
#include "ebpf/type/table/ProfileTable.h"

namespace logtail {
namespace ebpf {

std::unique_ptr<PluginConfig>
buildCpuProfilingConfig(std::unordered_set<uint32_t> pids,
                        std::optional<std::string> hostRootPath,
                        CpuProfilingHandler handler,
                        void *ctx) {
    CpuProfilingConfig config = {
        .mPids = std::move(pids),
        .mHostRootPath = std::move(hostRootPath),
        .mHandler = handler, .mCtx = ctx};
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
        buildCpuProfilingConfig({}, GetContainerHostPath(), handleCpuProfilingEvent, this));
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

    ProcessDiscoveryConfig config{
        .mConfigKey = key,
        .mFullDiscovery = opts->mCmdlines.empty(),
    };
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

    ProcessDiscoveryManager::GetInstance()->AddDiscovery(configName, std::move(config));

    LOG_DEBUG(sLogger, ("CpuProfilingManager", "add or update config")("config", configName));
    
    return 0;
}

int CpuProfilingManager::RemoveConfig(const std::string &configName) {
    auto it = mConfigNameToKey.find(configName);
    assert(it != mConfigNameToKey.end());
    auto key = it->second;
    mConfigNameToKey.erase(it);

    ProcessDiscoveryManager::GetInstance()->RemoveDiscovery(configName);

    [[maybe_unused]] auto hit = mConfigInfoMap.erase(key);
    assert(hit);

    LOG_DEBUG(sLogger, ("CpuProfilingManager", "remove config")("config", configName));

    return 0;
}

int CpuProfilingManager::Suspend() {
    // Do nothing
    LOG_INFO(sLogger, ("CpuProfilingManager", "suspend"));
    return 0;
}

// stack, cnt, trace_id
using StackCnt = std::tuple<std::vector<std::string>, uint32_t, std::string>;

static void parseStackCnt(char const* symbol, std::vector<StackCnt>& result) {
    // Format: "<comm>:<pid>;<stacks> <cnt> <trace_id>\n"
    // Example: "bash:1234;func1;func2;func3 10 xxxxx\n"

    std::istringstream ssymbol;
    ssymbol.str(symbol);
    std::string line;
    while (std::getline(ssymbol, line)) {
        auto pos1 = line.find(';');
        if (pos1 == std::string::npos) {
            LOG_ERROR(sLogger, ("Invalid symbol format", line));
            continue;
        }
        auto pos2 = line.rfind(' ');
        if (pos2 == std::string::npos || pos2 < pos1) {
            LOG_ERROR(sLogger, ("Invalid symbol format", line));
            continue;
        }
        auto pos3 = line.rfind(' ', pos2 - 1);
        if (pos3 == std::string::npos || pos3 < pos1) {
            LOG_ERROR(sLogger, ("Invalid symbol format", line));
            continue;
        }

        auto stack = line.substr(pos1 + 1, pos3 - pos1 - 1);
        auto cntStr = line.substr(pos3 + 1, pos2 - pos3 - 1);
        uint32_t cnt = std::stoul(cntStr);
        auto traceId = line.substr(pos2 + 1);
        if (traceId == "null") {
            traceId = "";
        }

        std::vector<std::string> stackVec;
        std::istringstream sstack(stack);
        std::string func;
        while (std::getline(sstack, func, ';')) {
            stackVec.push_back(func);
        }

        result.push_back(std::make_tuple(std::move(stackVec), cnt, traceId));
    }
}

static void addContentToEvent(LogEvent *event, SourceBuffer *sourceBuffer,
                              const std::vector<std::string> &fullStack, std::string comm) {
    event->SetContent("dataType", std::string("CallStack"));
    event->SetContent("language", std::string("go"));

    std::string name = fullStack.back();
    std::string stack; // stack without the top function name

    for (size_t i = fullStack.size() - 2; i >= 0; i--) {
        stack += fullStack[i];
        if (i != 0) {
            stack += "\n";
        }
    }

    std::hash<std::string_view> hasher;
    size_t hashStack = hasher(stack);
    AttrHashCombine(hashStack, hasher(name));

    std::string stackId = ToHexString(hashStack);

    event->SetContent("name", name);
    event->SetContent("stack", stack);
    event->SetContent("stackID", stackId);

    event->SetContent("type", std::string("profile_cpu"));
    event->SetContent("type_cn", std::string(""));
    event->SetContent("units", std::string("nanoseconds"));
    event->SetContent("val", std::string("1"));
    event->SetContent("valueType", std::string("cpu"));
    event->SetContent("valueType_cn", std::string(""));

    // {"__name__": "shuizhao-python-profiling-service", "thread": "abcd"}
    std::string jsonLabels;
    jsonLabels += "{\"__name__\": \"";
    jsonLabels += "shuizhao-python-profiling-service";
    jsonLabels += "\", \"thread\": \"";
    jsonLabels += comm;
    jsonLabels += "\"}";
    event->SetContent("labels", jsonLabels);
}

void CpuProfilingManager::HandleCpuProfilingEvent(uint32_t pid,
                                                  const char *comm,
                                                  const char *stack,
                                                  uint32_t cnt) {
    ADD_COUNTER(mRecvKernelEventsTotal, 1);

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

    auto logtime = time(nullptr);
    if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
        logtime += GetTimeDelta();
    }

    std::vector<StackCnt> stacks;
    parseStackCnt(stack, stacks);

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string profileID = CalculateRandomUUID();
    
    // auto pidSb = sourceBuffer->CopyString(std::to_string(pid));
    // auto commSb = sourceBuffer->CopyString(std::string(comm));
    // for (auto& [stack, cnt, traceId] : stacks) {
    //     auto* event = eventGroup.AddLogEvent();
    //     event->SetTimestamp(logtime);
    //     auto stackSb = sourceBuffer->CopyString(stack);
    //     auto cntSb = sourceBuffer->CopyString(std::to_string(cnt));
    //     auto traceIdSb = sourceBuffer->CopyString(traceId);
    //     event->SetContentNoCopy(kPid.LogKey(), StringView(pidSb.data, pidSb.size));
    //     event->SetContentNoCopy(kComm.LogKey(), StringView(commSb.data, commSb.size));
    //     event->SetContentNoCopy(kStack.LogKey(), StringView(stackSb.data, stackSb.size));
    //     event->SetContentNoCopy(kCnt.LogKey(), StringView(cntSb.data, cntSb.size));
    //     event->SetContentNoCopy(kTraceId.LogKey(), StringView(traceIdSb.data, traceIdSb.size));
    // }

    for (auto &[stack, cnt, traceId] : stacks) {
        auto *event = eventGroup.AddLogEvent();
        event->SetTimestamp(logtime);
        event->SetContent("profileID", profileID);

        addContentToEvent(event, sourceBuffer.get(), stack, std::string(comm));
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
        buildCpuProfilingConfig(std::move(totalPids), std::nullopt, nullptr, nullptr));
}

} // namespace ebpf
} // namespace logtail
