// Copyright 2026 iLogtail Authors
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

#include "ebpf/plugin/agentsight/AgentsightManager.h"

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/StringView.h"
#include "common/magic_enum.hpp"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {

namespace {

bool ParseHostAndPortFromRequestUrl(const std::string& url, std::string& host, std::string& port) {
    host.clear();
    port.clear();
    const auto schemePos = url.find("://");
    const size_t authorityStart = (schemePos == std::string::npos) ? 0 : schemePos + 3;
    const size_t pathPos = url.find('/', authorityStart);
    const size_t queryPos = url.find('?', authorityStart);
    const size_t fragmentPos = url.find('#', authorityStart);
    size_t authorityEnd = url.size();
    if (pathPos != std::string::npos) {
        authorityEnd = std::min(authorityEnd, pathPos);
    }
    if (queryPos != std::string::npos) {
        authorityEnd = std::min(authorityEnd, queryPos);
    }
    if (fragmentPos != std::string::npos) {
        authorityEnd = std::min(authorityEnd, fragmentPos);
    }
    if (authorityEnd <= authorityStart) {
        return false;
    }
    std::string authority = url.substr(authorityStart, authorityEnd - authorityStart);
    const size_t atPos = authority.rfind('@');
    if (atPos != std::string::npos) {
        if (atPos + 1 >= authority.size()) {
            return false;
        }
        authority = authority.substr(atPos + 1);
    }
    if (authority.empty()) {
        return false;
    }
    if (authority[0] == '[') {
        const size_t closingBracket = authority.find(']');
        if (closingBracket == std::string::npos || closingBracket <= 1) {
            return false;
        }
        host = authority.substr(1, closingBracket - 1);
        if (closingBracket + 1 == authority.size()) {
            return !host.empty();
        }
        if (authority[closingBracket + 1] != ':') {
            return false;
        }
        if (closingBracket + 2 >= authority.size()) {
            return false;
        }
        port = authority.substr(closingBracket + 2);
        return !host.empty();
    }
    const size_t colonPos = authority.rfind(':');
    if (colonPos == std::string::npos) {
        host = authority;
        return !host.empty();
    }
    if (colonPos == 0) {
        return false;
    }
    host = authority.substr(0, colonPos);
    if (colonPos + 1 >= authority.size()) {
        return false;
    }
    port = authority.substr(colonPos + 1);
    return !host.empty();
}

/// Builtin cmdline allow rules — keep in sync with
/// `core/_thirdparty/coolbpf/src/agentsight/agentsight.json` (`cmdline.allow`).
struct BuiltinCmdlineAllowRule {
    const char* agent_name;
    std::vector<std::string> argv_globs;
};

static const std::vector<BuiltinCmdlineAllowRule>& GetBuiltinCmdlineAllowRules() {
    static const std::vector<BuiltinCmdlineAllowRule> kRules = {
        {"Hermes", {"hermes*"}},
        {"Hermes", {"*python*", "*hermes*"}},
        {"Hermes", {"*python*", "-m", "*hermes*"}},
        {"Cosh", {"node*", "*/usr/bin/co*"}},
        {"Cosh", {"node*", "*/usr/bin/cosh*"}},
        {"Cosh", {"node*", "*/usr/bin/copliot*"}},
        {"Cosh", {"node*", "*copilot-shell*"}},
        {"OpenClaw", {"*openclaw-gatewa*"}},
        {"OpenClaw", {"node*", "*openclaw*"}},
    };
    return kRules;
}

static const std::vector<const char*>& GetBuiltinDomainAllowRules() {
    static const std::vector<const char*> kRules = {
        "api.openai.com",
        "api.anthropic.com",
        "dashscope.aliyuncs.com",
        "dashscope-intl.aliyuncs.com",
    };
    return kRules;
}

/// Join argv glob patterns for a stable FFI `agent_name` label (not used for matching).
std::string DeriveAgentsightAliasBase(const std::vector<std::string>& patterns) {
    std::string s;
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) {
            s += '|';
        }
        s += patterns[i];
    }
    return s;
}

void ApplyAgentsightRulesToConfig(AgentsightConfigHandle* cfg,
                                  const AgentSightSymbolTable* sym,
                                  const SecurityOptions& opts) {
    const bool injectBuiltinCmdlineAllow
        = opts.mAgentsightCmdlineWhitelist.empty() && opts.mAgentsightCmdlineBlacklist.empty();
    const bool injectBuiltinDomainAllow = opts.mAgentsightDomainWhitelist.empty();

    if (!sym || !sym->config_add_cmdline_rule) {
        LOG_WARNING(sLogger,
                    ("AgentSight", "cmdline rules required but agentsight_config_add_cmdline_rule is missing; skipped")(
                        "user_whitelist_rows", opts.mAgentsightCmdlineWhitelist.size())(
                        "user_blacklist_rows", opts.mAgentsightCmdlineBlacklist.size())("builtin_allow_injected",
                                                                                        injectBuiltinCmdlineAllow));
    }
    if (!sym || !sym->config_add_domain_rule) {
        LOG_WARNING(sLogger,
                    ("AgentSight", "domain rules required but agentsight_config_add_domain_rule is missing; skipped")(
                        "user_domain_rows", opts.mAgentsightDomainWhitelist.size())("builtin_domain_injected",
                                                                                    injectBuiltinDomainAllow));
    }

    std::vector<std::pair<std::string, std::vector<std::string>>> allowRowsToApply;
    size_t aliasCollisions = 0;

    if (injectBuiltinCmdlineAllow) {
        const auto& builtins = GetBuiltinCmdlineAllowRules();
        allowRowsToApply.reserve(builtins.size());
        for (const auto& br : builtins) {
            allowRowsToApply.emplace_back(std::string(br.agent_name), br.argv_globs);
        }
    } else {
        std::unordered_map<std::string, int> aliasOrdinal;
        std::vector<std::string> whitelistAliases;
        whitelistAliases.reserve(opts.mAgentsightCmdlineWhitelist.size());
        for (const auto& row : opts.mAgentsightCmdlineWhitelist) {
            const std::string base = DeriveAgentsightAliasBase(row);
            int& n = aliasOrdinal[base];
            ++n;
            if (n == 1) {
                whitelistAliases.push_back(base);
            } else {
                ++aliasCollisions;
                whitelistAliases.push_back(base + "_" + std::to_string(n));
            }
        }
        allowRowsToApply.reserve(opts.mAgentsightCmdlineWhitelist.size());
        for (size_t i = 0; i < opts.mAgentsightCmdlineWhitelist.size(); ++i) {
            allowRowsToApply.emplace_back(std::move(whitelistAliases[i]), opts.mAgentsightCmdlineWhitelist[i]);
        }
    }

    if (sym && sym->config_add_cmdline_rule) {
        for (const auto& entry : allowRowsToApply) {
            const auto& row = entry.second;
            std::vector<const char*> ptrs;
            ptrs.reserve(row.size() + 1U);
            for (const auto& p : row) {
                ptrs.push_back(p.c_str());
            }
            ptrs.push_back(nullptr);
            sym->config_add_cmdline_rule(cfg, ptrs.data(), entry.first.c_str(), 1);
        }
        for (const auto& row : opts.mAgentsightCmdlineBlacklist) {
            std::vector<const char*> ptrs;
            ptrs.reserve(row.size() + 1U);
            for (const auto& p : row) {
                ptrs.push_back(p.c_str());
            }
            ptrs.push_back(nullptr);
            sym->config_add_cmdline_rule(cfg, ptrs.data(), nullptr, 0);
        }
    }

    size_t domainRowsApplied = 0;
    if (sym && sym->config_add_domain_rule) {
        if (injectBuiltinDomainAllow) {
            for (const char* d : GetBuiltinDomainAllowRules()) {
                sym->config_add_domain_rule(cfg, d);
                ++domainRowsApplied;
            }
        } else {
            for (const auto& d : opts.mAgentsightDomainWhitelist) {
                sym->config_add_domain_rule(cfg, d.c_str());
                ++domainRowsApplied;
            }
        }
    }

    LOG_INFO(sLogger,
             ("AgentSight", "applied config rules")("user_cmdline_whitelist", opts.mAgentsightCmdlineWhitelist.size())(
                 "user_cmdline_blacklist", opts.mAgentsightCmdlineBlacklist.size())("builtin_cmdline_allow_injected",
                                                                                    injectBuiltinCmdlineAllow)(
                 "cmdline_allow_rows_applied", allowRowsToApply.size())("user_domain_whitelist",
                                                                        opts.mAgentsightDomainWhitelist.size())(
                 "builtin_domain_allow_injected", injectBuiltinDomainAllow)("domain_rows_applied", domainRowsApplied)(
                 "whitelist_alias_collisions", aliasCollisions)("cmdline_api", sym && sym->config_add_cmdline_rule)(
                 "domain_api", sym && sym->config_add_domain_rule));
}

} // namespace

AgentsightManager::AgentsightManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                     const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                     moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                     EventPool* pool)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, pool) {
}

int AgentsightManager::Init() {
    if (mInited) {
        return 0;
    }
    mInited = true;
    return 0;
}

void AgentsightManager::LogAgentSightError(const char* what) {
    const auto* sym = mEBPFAdapter->GetAgentSightSymbols();
    const char* err = sym && sym->last_error ? sym->last_error() : nullptr;
    LOG_ERROR(sLogger, ("AgentSight", what)("last_error", err ? err : ""));
}

void AgentsightManager::releaseMetricRefs() {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
    mRefAndLabels.clear();
    mMetricMgr.reset();
    mPluginInEventsTotal.reset();
    mPushLogsTotal.reset();
    mPushLogGroupTotal.reset();
}

void AgentsightManager::StopAgentSightLocked() {
    const auto* sym = mEBPFAdapter->GetAgentSightSymbols();
    if (mEventFd >= 0) {
        EBPFServer::GetInstance()->UnregisterExternalEpollFd(PluginType::AGENTSIGHT_OBSERVE, mEventFd);
    }
    mEventFd = -1;
    if (mHandle && sym && sym->handle_stop) {
        (void)sym->handle_stop(mHandle);
    }
    if (mHandle && sym && sym->handle_free) {
        sym->handle_free(mHandle);
    }
    mHandle = nullptr;
    mRunning = false;
}

bool AgentsightManager::RestartAgentSightLocked(const SecurityOptions& opts) {
    const auto* sym = mEBPFAdapter->GetAgentSightSymbols();
    if (!sym || !sym->config_new || !sym->handle_new || !sym->handle_start || !sym->handle_read
        || !sym->handle_get_eventfd) {
        StopAgentSightLocked();
        LOG_ERROR(sLogger, ("AgentSight", "symbols not available"));
        return false;
    }

    StopAgentSightLocked();

    AgentsightConfigHandle* cfg = sym->config_new();
    if (!cfg) {
        LogAgentSightError("config_new returned null");
        return false;
    }
    sym->config_set_verbose(cfg, static_cast<int>(opts.mVerbose));
    if (!opts.mLogPath.empty()) {
        sym->config_set_log_path(cfg, opts.mLogPath.c_str());
    }

    ApplyAgentsightRulesToConfig(cfg, sym, opts);

    mHandle = sym->handle_new(cfg);
    if (sym->config_free) {
        sym->config_free(cfg);
    }
    cfg = nullptr;

    if (!mHandle) {
        LogAgentSightError("agentsight_new failed");
        return false;
    }

    if (sym->handle_start(mHandle) != 0) {
        LogAgentSightError("agentsight_start failed");
        StopAgentSightLocked();
        return false;
    }

    mEventFd = sym->handle_get_eventfd(mHandle);
    if (mEventFd < 0) {
        LogAgentSightError("agentsight_get_eventfd returned invalid fd");
        StopAgentSightLocked();
        return false;
    }
    EBPFServer::GetInstance()->RegisterExternalEpollFd(PluginType::AGENTSIGHT_OBSERVE, mEventFd);
    mRunning = true;
    return true;
}

int AgentsightManager::DrainReadsLocked() {
    const auto* sym = mEBPFAdapter->GetAgentSightSymbols();
    if (!mHandle || !sym || !sym->handle_read) {
        return 0;
    }
    int total = 0;
    for (;;) {
        const int r = sym->handle_read(mHandle, nullptr, nullptr, &AgentsightManager::OnLlmCallback, this, 0);
        if (r <= 0) {
            break;
        }
        total += r;
    }
    LOG_DEBUG(sLogger, ("AgentSight DrainReadsLocked", total));
    return total;
}

int AgentsightManager::OnEpollReadable() {
    std::lock_guard<std::mutex> lock(mLibMutex);
    if (!mRunning || !mHandle) {
        LOG_DEBUG(sLogger, ("AgentSight OnEpollReadable", "not running or handle not available"));
        return 0;
    }
    return DrainReadsLocked();
}

// AgentSight I/O is epoll-driven only (OnEpollReadable); no perf-buffer poll path.
int AgentsightManager::PollPerfBuffer(int maxWaitTimeMs) {
    (void)maxWaitTimeMs;
    return 0;
}

void AgentsightManager::OnLlmCallback(const AgentsightLLMData* data, void* user_data) {
    if (!data || !user_data) {
        return;
    }
    auto* self = static_cast<AgentsightManager*>(user_data);
    // Do not lock mLibMutex here: runs inside handle_read → DrainReadsLocked while OnEpollReadable holds mLibMutex.
    const std::string configName = self->mConfigName;
    auto evt = std::make_shared<AgentsightLlmRecord>(configName, *data);
    if (self->mCommonEventQueue.try_enqueue(evt)) {
        ADD_COUNTER(self->mPluginInEventsTotal, 1);
    } else {
        ADD_COUNTER(self->mLossKernelEventsTotal, 1);
        LOG_WARNING(sLogger, ("AgentSight LLM event enqueue failed", ""));
    }
}

int AgentsightManager::AddOrUpdateConfig(const CollectionPipelineContext* ctx,
                                         uint32_t index,
                                         const PluginMetricManagerPtr& metricMgr,
                                         const std::variant<SecurityOptions*, ObserverNetworkOption*>& opt) {
    const auto* secPtr = std::get_if<SecurityOptions*>(&opt);
    if (!secPtr || !*secPtr) {
        LOG_ERROR(sLogger, ("AgentsightManager AddOrUpdateConfig", "invalid options variant"));
        return 1;
    }
    const SecurityOptions* sec = *secPtr;
    if (sec->mProbeType != SecurityProbeType::AGENTSIGHT_OBSERVE) {
        LOG_ERROR(sLogger, ("AgentsightManager AddOrUpdateConfig", "wrong SecurityProbeType"));
        return 1;
    }
    if (!ctx) {
        LOG_ERROR(sLogger, ("ctx is null", ""));
        return 1;
    }

    if (metricMgr && mRefAndLabels.empty()) {
        MetricLabels eventTypeLabels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG}};
        auto ref = metricMgr->GetOrCreateReentrantMetricsRecordRef(eventTypeLabels);
        mRefAndLabels.emplace_back(eventTypeLabels);
        mPluginInEventsTotal = ref->GetCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
        mPushLogsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
        mPushLogGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
    }

    if (mRegisteredConfigCount != 0) {
        if (update(opt) != 0) {
            std::lock_guard<std::mutex> lock(mLibMutex);
            releaseMetricRefs();
            return 1;
        }
        if (resume(opt) != 0) {
            std::lock_guard<std::mutex> lock(mLibMutex);
            releaseMetricRefs();
            return 1;
        }
        return 0;
    }

    // Retain for releaseMetricRefs() on failure paths before mLibMutex (same thread as EnablePlugin).
    if (metricMgr) {
        mMetricMgr = metricMgr;
    }

    if (!mEBPFAdapter->GetAgentSightSymbols()) {
        releaseMetricRefs();
        LOG_ERROR(sLogger, ("AgentSight shared library not loaded", ""));
        return 1;
    }

    std::lock_guard<std::mutex> lock(mLibMutex);
    mConfigName = ctx->GetConfigName();
    mPluginIndex = index;
    mPipelineCtx = ctx;
    mQueueKey = ctx->GetProcessQueueKey();

    if (!RestartAgentSightLocked(*sec)) {
        releaseMetricRefs();
        mConfigName.clear();
        mPipelineCtx = nullptr;
        mQueueKey = 0;
        mPluginIndex = 0;
        return 1;
    }
    mRegisteredConfigCount = 1;
    return 0;
}

int AgentsightManager::RemoveConfig(const std::string&) {
    std::lock_guard<std::mutex> lock(mLibMutex);
    releaseMetricRefs();
    mRegisteredConfigCount = 0;
    mConfigName.clear();
    mPipelineCtx = nullptr;
    mQueueKey = 0;
    mPluginIndex = 0;
    StopAgentSightLocked();
    return 0;
}

int AgentsightManager::Destroy() {
    std::lock_guard<std::mutex> lock(mLibMutex);
    releaseMetricRefs();
    StopAgentSightLocked();
    mRegisteredConfigCount = 0;
    mConfigName.clear();
    mPipelineCtx = nullptr;
    mQueueKey = 0;
    mPluginIndex = 0;
    mInited = false;
    return 0;
}

int AgentsightManager::Suspend() {
    {
        WriteLock suspendLock(mMtx);
        mSuspendFlag = true;
    }
    std::lock_guard<std::mutex> lock(mLibMutex);
    StopAgentSightLocked();
    return 0;
}

int AgentsightManager::update(const std::variant<SecurityOptions*, ObserverNetworkOption*>&) {
    return 0;
}

int AgentsightManager::resume(const std::variant<SecurityOptions*, ObserverNetworkOption*>& opt) {
    const auto* secPtr = std::get_if<SecurityOptions*>(&opt);
    if (!secPtr || !*secPtr) {
        return 1;
    }
    {
        WriteLock lk(mMtx);
        mSuspendFlag = false;
    }
    std::lock_guard<std::mutex> lock(mLibMutex);
    if (mRegisteredConfigCount == 0) {
        return 0;
    }
    if (!RestartAgentSightLocked(**secPtr)) {
        return 1;
    }
    return 0;
}

std::unique_ptr<PluginConfig>
AgentsightManager::GeneratePluginConfig(const std::variant<SecurityOptions*, ObserverNetworkOption*>&) {
    auto c = std::make_unique<PluginConfig>();
    c->mPluginType = PluginType::AGENTSIGHT_OBSERVE;
    c->mConfig = ProcessConfig{};
    return c;
}

int AgentsightManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event || event->GetKernelEventType() != KernelEventType::AGENTSIGHT_LLM_RECORD) {
        return 0;
    }
    auto* rec = static_cast<AgentsightLlmRecord*>(event.get());
    if (!rec) {
        return 1;
    }

    logtail::QueueKey queueKey;
    uint32_t pluginIndex;
    {
        std::lock_guard<std::mutex> lock(mLibMutex);
        if (mPipelineCtx == nullptr) {
            return 0;
        }
        queueKey = mQueueKey;
        pluginIndex = mPluginIndex;
    }

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto* log = eventGroup.AddLogEvent(true, mEventPool);
    const auto sec = static_cast<int64_t>(rec->mTimestampNs / 1000000000ULL);
    const auto nsec = static_cast<int64_t>(rec->mTimestampNs % 1000000000ULL);
    log->SetTimestamp(sec, nsec);

    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    setStr(StringView("gen_ai.session.id"), rec->mSessionId);
    setStr(StringView("gen_ai.turn.id"), rec->mConversationId);
    setStr(StringView("gen_ai.response.id"), rec->mResponseId);

    if (rec->mPid != 0) {
        log->SetContent("pid", std::to_string(rec->mPid));
    }
    setStr(StringView("comm"), rec->mProcessName);
    setStr(StringView("gen_ai.agent.name"), rec->mAgentName);

    log->SetContent("gen_ai.request.timestamp", std::to_string(rec->mTimestampNs / 1000000ULL));
    log->SetContent("gen_ai.response.duration", std::to_string(rec->mDurationNs / 1000000ULL));

    if (!rec->mRequestUrl.empty()) {
        std::string host;
        std::string port;
        if (ParseHostAndPortFromRequestUrl(rec->mRequestUrl, host, port)) {
            setStr(StringView("server.address"), host);
            setStr(StringView("server.port"), port);
        }
    }

    log->SetContent("gen_ai.provider.name", rec->mProvider);
    log->SetContent("gen_ai.request.model", rec->mModel);
    log->SetContent("status_code", std::to_string(rec->mStatusCode));
    log->SetContent(StringView("is_sse"), StringView(rec->mIsSse ? "1" : "0"));
    setStr(StringView("gen_ai.response.finish_reasons"), rec->mFinishReason);
    log->SetContent(std::string("is_usage_from_api"), std::string(rec->mLlmUsage ? "true" : "false"));

    log->SetContent("gen_ai.usage.input_tokens", std::to_string(rec->mInputTokens));
    log->SetContent("gen_ai.usage.output_tokens", std::to_string(rec->mOutputTokens));
    log->SetContent("gen_ai.usage.total_tokens", std::to_string(rec->mTotalTokens));
    log->SetContent("gen_ai.usage.cache_creation.input_tokens", std::to_string(rec->mCacheCreationInputTokens));
    log->SetContent("gen_ai.usage.cache_read.input_tokens", std::to_string(rec->mCacheReadInputTokens));

    setStr(StringView("gen_ai.input.messages"), rec->mRequestMessagesJson);
    setStr(StringView("gen_ai.output.messages"), rec->mResponseMessagesJson);

    std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), pluginIndex);
    if (QueueStatus::OK == ProcessQueueManager::GetInstance()->PushQueue(queueKey, std::move(item))) {
        ADD_COUNTER(mPushLogsTotal, 1);
        ADD_COUNTER(mPushLogGroupTotal, 1);
    } else {
        if (mPushLogFailedTotal) {
            ADD_COUNTER(mPushLogFailedTotal, 1);
        }
        LOG_WARNING(
            sLogger,
            ("Agentsight push queue failed", "")("config", rec->GetPipelineConfigName())("pluginIdx", pluginIndex));
    }
    return 0;
}

} // namespace logtail::ebpf
