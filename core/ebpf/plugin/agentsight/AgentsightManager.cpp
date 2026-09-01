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

#include <cctype>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#include "rapidjson/document.h"

#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/StringView.h"
#include "common/UUIDUtil.h"
#include "common/magic_enum.hpp"
#include "constants/TagConstants.h"
#include "container_manager/ContainerManager.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "ebpf/plugin/agentsight/AgentsightMessageUtil.h"
#include "ebpf/type/table/BaseElements.h"
#include "file_server/ContainerInfo.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail::ebpf {

namespace {

/// Splits `host` / `port` out of either a full URL (`https://h:p/path?q`) or a bare authority
/// (`h:p`, `[::1]:p`) — the scheme is optional, so raw HTTP events can pass the `Host` header
/// value straight in. Handles IPv6 brackets and `user@` prefixes.
bool ParseHostAndPortFromUrlOrAuthority(const std::string& url, std::string& host, std::string& port) {
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

/// Request headers that are safe to emit and worth emitting, as `http.request.header.<name>`.
///
/// The request side uses an ALLOWLIST, the opposite of the response side's denylist
/// (kSensitiveResponseHeaders), because the risk profiles are mirror images: request headers are
/// where credentials live (`authorization`, `x-api-key`, `cookie`, plus whatever a vendor invents),
/// while their diagnostic value is concentrated in a handful of well-known names. An open set would
/// be a credential leak waiting for the next vendor header; here an unknown header is simply dropped.
///
/// Why each one earns its place — this fallback exists to answer "there were calls, why no logs":
///   - content-type: says what the body actually was, i.e. directly explains why it could not be
///     parsed as GenAI semantics.
///   - content-length: the only cross-check for whether the emitted body was mangled by the upstream
///     from_utf8_lossy conversion (see EmitHttpBody) — `http.request.body.size` alone proves nothing.
///   - traceparent: W3C trace context, the only way to join these logs to application-side traces.
///
/// `host` / `:authority` and `user-agent` are deliberately NOT here: they are salvaged into the
/// stable OTel attributes `server.address` / `server.port` and `user_agent.original` instead, the
/// same way this path already prefers `url.path` over a header echo.
constexpr std::array<const char*, 3> kAllowedRequestHeaders = {
    "content-type",
    "content-length",
    "traceparent",
};

/// The subset of a raw request's headers that survives filtering. Everything else — credentials
/// included — stays in memory and is dropped with the record.
struct RawRequestHeaderFields {
    /// `host`, falling back to the HTTP/2 `:authority` pseudo-header. Feeds server.address/.port.
    std::string host;
    /// → `user_agent.original` (OTel Stable), not `http.request.header.user-agent`; emitting both
    /// would duplicate the value under two keys.
    std::string userAgent;
    /// Allowlisted headers, lowercased names, in `kAllowedRequestHeaders` order-independent
    /// iteration order. → `http.request.header.<name>`.
    std::vector<std::pair<std::string, std::string>> allowed;
};

/// Pulls everything worth keeping out of a raw HTTP request's headers JSON — the flat,
/// lowercase-keyed string→string map that AgentSight's FFI layer produces for `AgentsightHttpsData`.
///
/// One rapidjson DOM parse for host + user-agent + allowlisted headers, because the host extraction
/// had to parse anyway. Runs on the poller thread (via FillAgentsightHttpRequestLog), not on the
/// OnHttpsCallback path, so it is not subject to that callback's non-blocking constraint.
///
/// Header names are lowercased here rather than trusting the producer: upstream does lowercase them
/// (`parser/http/parser.rs` keys its HashMap by `h.name.to_lowercase()`) but the allowlist match is
/// only sound *because* of this normalization, and a producer change must not silently turn a
/// dropped header into an emitted one. `host` wins over `:authority` when both are present.
RawRequestHeaderFields ExtractRequestHeaderFields(const std::string& headersJson) {
    RawRequestHeaderFields fields;
    if (headersJson.empty()) {
        return fields;
    }
    rapidjson::Document doc;
    if (doc.Parse(headersJson.c_str(), headersJson.size()).HasParseError() || !doc.IsObject()) {
        return fields;
    }

    std::string authority;
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        if (!it->name.IsString() || !it->value.IsString() || it->value.GetStringLength() == 0) {
            continue;
        }
        std::string name(it->name.GetString(), it->name.GetStringLength());
        std::transform(
            name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name.empty()) {
            continue;
        }
        std::string value(it->value.GetString(), it->value.GetStringLength());

        if (name == "host") {
            fields.host = std::move(value);
            continue;
        }
        if (name == ":authority") {
            authority = std::move(value);
            continue;
        }
        if (name == "user-agent") {
            fields.userAgent = std::move(value);
            continue;
        }
        const bool allowed = std::any_of(kAllowedRequestHeaders.begin(),
                                         kAllowedRequestHeaders.end(),
                                         [&name](const char* candidate) { return name == candidate; });
        if (allowed) {
            fields.allowed.emplace_back(std::move(name), std::move(value));
        }
    }
    if (fields.host.empty()) {
        fields.host = std::move(authority);
    }
    return fields;
}

/// Emits one captured body under `<prefix>.content` / `<prefix>.size`.
///
/// No binary handling here on purpose. AgentSight's analyzer stores bodies as Rust `String`
/// (`analyzer/result.rs`: `request_body: Option<String>`) built with `String::from_utf8_lossy`
/// (`analyzer/unified.rs`), so non-UTF-8 payloads are already destroyed before the FFI hands them
/// over: every invalid byte has become U+FFFD. Measured with a 256-byte 0x00..0xFF request body,
/// which arrived as 512 bytes (128 ASCII + 128 x 3-byte replacement chars).
///
/// Consequences the consumer must know: bodies are faithful for text (verified against
/// `content-length`), silently lossy for binary, and `.size` is the post-conversion length, so it
/// overstates the wire size for binary payloads. Fixing this requires carrying `Vec<u8>` through
/// the analyzer and FFI upstream.
void EmitHttpBody(logtail::LogEvent* log, const std::string& prefix, const std::string& body) {
    if (body.empty()) {
        return;
    }
    log->SetContent(prefix + ".size", std::to_string(body.size()));
    log->SetContent(prefix + ".content", body);
}

/// Builtin cmdline allow rules used when the user does not configure any whitelist/blacklist.
/// `agent_type` values follow the LoongSuite naming convention (lowercase + hyphen) and are
/// kept in sync with the recommended template in
/// `docs/cn/plugins/input/native/input_agentsight.md`.
struct BuiltinCmdlineAllowRule {
    const char* agent_type;
    std::vector<std::string> argv_globs;
};

static const std::vector<BuiltinCmdlineAllowRule>& GetBuiltinCmdlineAllowRules() {
    static const std::vector<BuiltinCmdlineAllowRule> kRules = {
        {"hermes", {"hermes*"}},
        {"hermes", {"*python*", "*hermes*"}},
        {"hermes", {"*python*", "-m", "*hermes*"}},
        {"cosh", {"node*", "*/usr/bin/co*"}},
        {"cosh", {"node*", "*/usr/bin/cosh*"}},
        {"cosh", {"node*", "*/usr/bin/copliot*"}},
        {"cosh", {"node*", "*copilot-shell*"}},
        {"openclaw", {"*openclaw-gatewa*"}},
        {"openclaw", {"node*", "*openclaw*"}},
    };
    return kRules;
}

static const std::vector<const char*>& GetBuiltinHttpsAllowRules() {
    static const std::vector<const char*> kRules = {
        "api.openai.com",
        "api.anthropic.com",
        // DashScope/Bailian shared domains (pay-as-you-go)
        "dashscope.aliyuncs.com",
        "dashscope-intl.aliyuncs.com",
        "dashscope-us.aliyuncs.com",
        // Coding Plan domain
        "coding.dashscope.aliyuncs.com",
        // Workspace-dedicated / trial / Token Plan domains (dynamic prefixes)
        "*.maas.aliyuncs.com",
    };
    return kRules;
}

void ApplyAgentsightRulesToConfig(AgentsightConfigHandle* cfg,
                                  const AgentSightSymbolTable* sym,
                                  const SecurityOptions& opts) {
    // Built-in cmdline rules are injected only when the user did not supply either whitelist
    // or blacklist. Once any user rule is present, we use the user configuration verbatim so
    // strict matching scenarios are not silently broadened.
    // Http 目标列表为空时不注入默认值，等价于明文 HTTP 采集关闭。
    const bool injectBuiltinCmdlineAllow
        = opts.mAgentsightCmdlineWhitelist.empty() && opts.mAgentsightCmdlineBlacklist.empty();
    const bool injectBuiltinHttpsAllow = opts.mAgentsightHttps.empty();

    if (!sym || !sym->config_add_cmdline_rule) {
        LOG_WARNING(sLogger,
                    ("AgentSight",
                     "cmdline rules configured but agentsight_config_add_cmdline_rule symbol not found; skipping")(
                        "user_whitelist_rows", opts.mAgentsightCmdlineWhitelist.size())(
                        "user_blacklist_rows", opts.mAgentsightCmdlineBlacklist.size())("builtin_cmdline_injected",
                                                                                        injectBuiltinCmdlineAllow));
    }
    if (!sym || !sym->config_add_https) {
        LOG_WARNING(
            sLogger,
            ("AgentSight",
             "AgentSight https rules configured but agentsight_config_add_https symbol not found; skipping")(
                "user_https_rows", opts.mAgentsightHttps.size())("builtin_https_injected", injectBuiltinHttpsAllow));
    }
    if (!sym || !sym->config_add_http) {
        LOG_WARNING(sLogger,
                    ("AgentSight",
                     "AgentSight http targets configured but agentsight_config_add_http symbol not found; skipping")(
                        "user_http_rows", opts.mAgentsightHttp.size()));
    }

    std::vector<std::pair<std::string, std::vector<std::string>>> allowRowsToApply;
    if (injectBuiltinCmdlineAllow) {
        const auto& builtins = GetBuiltinCmdlineAllowRules();
        allowRowsToApply.reserve(builtins.size());
        for (const auto& br : builtins) {
            allowRowsToApply.emplace_back(std::string(br.agent_type), br.argv_globs);
        }
    } else {
        allowRowsToApply.reserve(opts.mAgentsightCmdlineWhitelist.size());
        for (const auto& rule : opts.mAgentsightCmdlineWhitelist) {
            allowRowsToApply.emplace_back(rule.agentType, rule.patterns);
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

    size_t httpsRowsApplied = 0;
    if (sym && sym->config_add_https) {
        if (injectBuiltinHttpsAllow) {
            for (const char* d : GetBuiltinHttpsAllowRules()) {
                sym->config_add_https(cfg, d);
                ++httpsRowsApplied;
            }
        } else {
            for (const auto& d : opts.mAgentsightHttps) {
                sym->config_add_https(cfg, d.c_str());
                ++httpsRowsApplied;
            }
        }
    }

    size_t httpRowsApplied = 0;
    if (sym && sym->config_add_http) {
        for (const auto& t : opts.mAgentsightHttp) {
            const int rc = sym->config_add_http(cfg, t.c_str());
            if (rc < 0) {
                const char* err = sym->last_error ? sym->last_error() : nullptr;
                LOG_WARNING(sLogger, ("AgentSight http target rejected", t)("last_error", err ? err : ""));
            } else {
                ++httpRowsApplied;
            }
        }
    }

    LOG_INFO(
        sLogger,
        ("AgentSight", "applied config rules")("user_cmdline_whitelist", opts.mAgentsightCmdlineWhitelist.size())(
            "user_cmdline_blacklist", opts.mAgentsightCmdlineBlacklist.size())("builtin_cmdline_allow_injected",
                                                                               injectBuiltinCmdlineAllow)(
            "cmdline_allow_rows_applied", allowRowsToApply.size())("user_https_rows", opts.mAgentsightHttps.size())(
            "builtin_https_allow_injected", injectBuiltinHttpsAllow)("https_rows_applied", httpsRowsApplied)(
            "user_http_rows", opts.mAgentsightHttp.size())("http_rows_applied", httpRowsApplied)(
            "cmdline_api", sym && sym->config_add_cmdline_rule)("https_api", sym && sym->config_add_https)(
            "http_api", sym && sym->config_add_http));
}

using SetLogStrFn = std::function<void(StringView, const std::string&)>;

struct AgentsightLlmEmitPayload {
    std::string precomputedDelta;
    std::string systemInstructionsJson;
    std::string systemInstructionsHash;
    bool emitSystemInstructions = false;
    std::string toolDefinitionsHash;
    bool emitToolDefinitions = false;
    std::string stepId;
    size_t eventSequenceRequest = 0;
    size_t eventSequenceResponse = 0;
};

void SetLogTimestampFromNs(logtail::LogEvent* log, uint64_t timestampNs) {
    const auto sec = static_cast<int64_t>(timestampNs / 1000000000ULL);
    const auto nsec = static_cast<int64_t>(timestampNs % 1000000000ULL);
    log->SetTimestamp(sec, nsec);
}

void FillAgentsightOtlpTimeFields(logtail::LogEvent* log, uint64_t timestampNs) {
    const std::string timestampStr = std::to_string(timestampNs);
    log->SetContent("time_unix_nano", timestampStr);
    log->SetContent("observed_time_unix_nano", timestampStr);
}

void FillAgentsightCommonCorrelation(const AgentsightLlmRecord& rec,
                                     SetLogStrFn setStr,
                                     logtail::LogEvent* log,
                                     const std::string& eventId = {}) {
    if (!eventId.empty()) {
        setStr(StringView("event.id"), eventId);
    }
    setStr(StringView("gen_ai.session.id"), rec.mSessionId);
    setStr(StringView("gen_ai.turn.id"), rec.mConversationId);
    if (rec.mPid != 0) {
        log->SetContent("pid", std::to_string(rec.mPid));
    }
    setStr(StringView("comm"), rec.mProcessName);
    setStr(StringView("cmdline"), rec.mCmdline);
    setStr(StringView("container.id"), rec.mContainerId);
    setStr(StringView("gen_ai.agent.type"), rec.mAgentType);
}

void FillAgentsightServerFromUrl(const AgentsightLlmRecord& rec, SetLogStrFn setStr) {
    if (rec.mRequestUrl.empty()) {
        return;
    }
    std::string host;
    std::string port;
    if (ParseHostAndPortFromUrlOrAuthority(rec.mRequestUrl, host, port)) {
        setStr(StringView("server.address"), host);
        setStr(StringView("server.port"), port);
    }
}

void FillAgentsightRequestInputFields(const AgentsightLlmRecord& rec,
                                      SetLogStrFn setStr,
                                      bool messageDeltaOnly,
                                      const AgentsightLlmEmitPayload& payload) {
    if (!messageDeltaOnly) {
        setStr(StringView("gen_ai.input.messages"), rec.mRequestMessagesJson);
    }
    if (!payload.systemInstructionsHash.empty()) {
        setStr(StringView("gen_ai.system_instructions_hash"), payload.systemInstructionsHash);
    }
    if (payload.emitSystemInstructions) {
        setStr(StringView("gen_ai.system_instructions"), payload.systemInstructionsJson);
    }
    if (!payload.toolDefinitionsHash.empty()) {
        setStr(StringView("gen_ai.tool.definitions_hash"), payload.toolDefinitionsHash);
    }
    if (payload.emitToolDefinitions) {
        setStr(StringView("gen_ai.tool.definitions"), rec.mToolDefinitionsJson);
    }
    setStr(StringView("gen_ai.input.messages_delta"), payload.precomputedDelta);
}

void FillAgentsightCombinedLlmLog(const AgentsightLlmRecord& rec,
                                  logtail::LogEvent* log,
                                  bool messageDeltaOnly,
                                  const AgentsightLlmEmitPayload& payload) {
    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    SetLogTimestampFromNs(log, rec.mTimestampNs);
    FillAgentsightOtlpTimeFields(log, rec.mTimestampNs);
    FillAgentsightCommonCorrelation(rec, setStr, log);
    setStr(StringView("gen_ai.response.id"), rec.mResponseId);

    log->SetContent("gen_ai.response.duration", std::to_string(rec.mDurationNs / 1000000ULL));

    FillAgentsightServerFromUrl(rec, setStr);

    log->SetContent("gen_ai.provider.name", rec.mProvider);
    log->SetContent("gen_ai.request.model", rec.mModel);
    log->SetContent("status_code", std::to_string(rec.mStatusCode));
    log->SetContent(StringView("is_sse"), StringView(rec.mIsSse ? "1" : "0"));
    setStr(StringView("gen_ai.response.finish_reasons"),
           FormatFinishReasonsJson(rec.mResponseMessagesJson, rec.mFinishReason));
    log->SetContent(std::string("is_usage_from_api"), std::string(rec.mLlmUsage ? "true" : "false"));

    log->SetContent("gen_ai.usage.input_tokens", std::to_string(rec.mInputTokens));
    log->SetContent("gen_ai.usage.output_tokens", std::to_string(rec.mOutputTokens));
    log->SetContent("gen_ai.usage.total_tokens", std::to_string(rec.mTotalTokens));
    log->SetContent("gen_ai.usage.cache_creation.input_tokens", std::to_string(rec.mCacheCreationInputTokens));
    log->SetContent("gen_ai.usage.cache_read.input_tokens", std::to_string(rec.mCacheReadInputTokens));

    FillAgentsightRequestInputFields(rec, setStr, messageDeltaOnly, payload);
    setStr(StringView("gen_ai.output.messages"), rec.mResponseMessagesJson);
}

void FillAgentsightModelRequestLog(const AgentsightLlmRecord& rec,
                                   logtail::LogEvent* log,
                                   bool messageDeltaOnly,
                                   const AgentsightLlmEmitPayload& payload,
                                   const std::string& eventId) {
    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    SetLogTimestampFromNs(log, rec.mTimestampNs);
    FillAgentsightOtlpTimeFields(log, rec.mTimestampNs);
    log->SetContent(StringView("event.name"), StringView("gen_ai.model.request"));
    FillAgentsightCommonCorrelation(rec, setStr, log, eventId);

    FillAgentsightServerFromUrl(rec, setStr);

    log->SetContent("gen_ai.provider.name", rec.mProvider);
    log->SetContent("gen_ai.request.model", rec.mModel);
    setStr(StringView("gen_ai.step.id"), payload.stepId);
    if (payload.eventSequenceRequest > 0) {
        log->SetContent("gen_ai.event.sequence", std::to_string(payload.eventSequenceRequest));
    }
    FillAgentsightRequestInputFields(rec, setStr, messageDeltaOnly, payload);
}

void FillAgentsightModelResponseLog(const AgentsightLlmRecord& rec,
                                    logtail::LogEvent* log,
                                    const AgentsightLlmEmitPayload& payload,
                                    const std::string& eventId) {
    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    const uint64_t responseEndNs = rec.mTimestampNs + rec.mDurationNs;
    SetLogTimestampFromNs(log, responseEndNs);
    FillAgentsightOtlpTimeFields(log, responseEndNs);
    log->SetContent(StringView("event.name"), StringView("gen_ai.model.response"));
    FillAgentsightCommonCorrelation(rec, setStr, log, eventId);
    setStr(StringView("gen_ai.response.id"), rec.mResponseId);
    setStr(StringView("gen_ai.step.id"), payload.stepId);
    if (payload.eventSequenceResponse > 0) {
        log->SetContent("gen_ai.event.sequence", std::to_string(payload.eventSequenceResponse));
    }

    log->SetContent("gen_ai.response.duration", std::to_string(rec.mDurationNs / 1000000ULL));
    log->SetContent("gen_ai.provider.name", rec.mProvider);
    if (!rec.mModel.empty()) {
        log->SetContent("gen_ai.response.model", rec.mModel);
    }
    log->SetContent("status_code", std::to_string(rec.mStatusCode));
    log->SetContent(StringView("is_sse"), StringView(rec.mIsSse ? "1" : "0"));
    setStr(StringView("gen_ai.response.finish_reasons"),
           FormatFinishReasonsJson(rec.mResponseMessagesJson, rec.mFinishReason));
    log->SetContent(std::string("is_usage_from_api"), std::string(rec.mLlmUsage ? "true" : "false"));

    log->SetContent("gen_ai.usage.input_tokens", std::to_string(rec.mInputTokens));
    log->SetContent("gen_ai.usage.output_tokens", std::to_string(rec.mOutputTokens));
    log->SetContent("gen_ai.usage.total_tokens", std::to_string(rec.mTotalTokens));
    log->SetContent("gen_ai.usage.cache_creation.input_tokens", std::to_string(rec.mCacheCreationInputTokens));
    log->SetContent("gen_ai.usage.cache_read.input_tokens", std::to_string(rec.mCacheReadInputTokens));

    setStr(StringView("gen_ai.output.messages"), rec.mResponseMessagesJson);
}

/// `event.sequence` values ordering the two halves of one raw HTTP exchange.
constexpr uint32_t kHttpRequestEventSequence = 1;
constexpr uint32_t kHttpResponseEventSequence = 2;

/// Fields shared by the `http.request` / `http.response` pair of a raw HTTP exchange — the traffic
/// AgentSight could not map onto GenAI semantics.
///
/// No `gen_ai.*` field is emitted here, by design: this event kind exists precisely because the
/// payload carried no GenAI semantics, so putting anything in that namespace would misrepresent it.
/// The agent type is therefore reported as plain `agent.type`, matching the unprefixed `pid` /
/// `comm` / `cmdline` / `container.id` this path already uses. The gen_ai.* path keeps
/// `gen_ai.agent.type` (see FillAgentsightCommonCorrelation) — the value is identical, only the key
/// differs, so a downstream query spanning both streams has to coalesce the two names.
///
/// Correlation follows the same three-way split the gen_ai.* path uses, only with unprefixed keys:
/// `event.id` identifies this one log, `http.exchange.id` is the pairing key shared by the two halves
/// of one exchange (the gen_ai.* counterpart is `gen_ai.step.id`), and `event.sequence` orders them
/// within the exchange (`gen_ai.event.sequence` there). Sequence matters because mDurationNs can be 0
/// — request-only records, or a response returned inside the same second — which makes the two
/// timestamps identical and the order otherwise undecidable.
void FillAgentsightHttpCommon(const AgentsightHttpsRecord& rec,
                              SetLogStrFn setStr,
                              logtail::LogEvent* log,
                              const std::string& eventId,
                              const std::string& exchangeId,
                              uint32_t eventSequence,
                              uint64_t timestampNs) {
    SetLogTimestampFromNs(log, timestampNs);
    FillAgentsightOtlpTimeFields(log, timestampNs);
    log->SetContent(StringView("event.id"), StringView(eventId));
    log->SetContent(StringView("http.exchange.id"), StringView(exchangeId));
    log->SetContent("event.sequence", std::to_string(eventSequence));
    if (rec.mPid != 0) {
        log->SetContent("pid", std::to_string(rec.mPid));
    }
    setStr(StringView("comm"), rec.mProcessName);
    setStr(StringView("cmdline"), rec.mCmdline);
    setStr(StringView("container.id"), rec.mContainerId);
    setStr(StringView("agent.type"), rec.mAgentType);
    log->SetContent(StringView("url.scheme"), StringView("https"));
}

/// Request half of a raw HTTP exchange.
///
/// Request headers are filtered through an allowlist (kAllowedRequestHeaders), never emitted
/// wholesale: they carry `Authorization` / `x-api-key` / `cookie`, and this path reports verbatim
/// bytes with no redaction, so emitting them writes live credentials to disk (observed in testing).
/// Two of them are salvaged into stable OTel attributes instead of the header namespace — the host as
/// `server.address` / `server.port` (matching the field names the gen_ai.* logs already use) and the
/// user agent as `user_agent.original`.
void FillAgentsightHttpRequestLog(const AgentsightHttpsRecord& rec,
                                  logtail::LogEvent* log,
                                  const std::string& eventId,
                                  const std::string& exchangeId) {
    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    FillAgentsightHttpCommon(rec, setStr, log, eventId, exchangeId, kHttpRequestEventSequence, rec.mTimestampNs);
    log->SetContent(StringView("event.name"), StringView("http.request"));
    setStr(StringView("http.request.method"), rec.mMethod);
    setStr(StringView("url.path"), rec.mPath);

    const RawRequestHeaderFields headerFields = ExtractRequestHeaderFields(rec.mRequestHeaders);
    std::string host;
    std::string port;
    if (ParseHostAndPortFromUrlOrAuthority(headerFields.host, host, port)) {
        setStr(StringView("server.address"), host);
        setStr(StringView("server.port"), port);
    }
    setStr(StringView("user_agent.original"), headerFields.userAgent);
    for (const auto& header : headerFields.allowed) {
        log->SetContent("http.request.header." + header.first, header.second);
    }

    EmitHttpBody(log, "http.request.body", rec.mRequestBody);
}

/// Response headers that carry credentials or credential challenges. Never emitted.
///
/// This applies to the response side the same reasoning that already removed *request* headers
/// wholesale (see FillAgentsightHttpRequestLog): this path reports verbatim bytes with no redaction,
/// so emitting a credential-bearing header writes a live credential to disk. `set-cookie` is a
/// directly replayable session credential, the `*-authenticate` headers carry challenges/nonces, and
/// some gateways echo tokens back in `x-*` headers.
///
/// Deliberately a denylist, not an allowlist: response headers are high-value for diagnosis and
/// vendors invent their own, so an allowlist would hide most of what this fallback exists to surface.
/// The trade-off is that this is an OPEN set — an unknown vendor's token-bearing response header is
/// emitted by default. Add newly discovered ones here.
constexpr std::array<const char*, 10> kSensitiveResponseHeaders = {
    "set-cookie",
    "set-cookie2",
    "www-authenticate",
    "proxy-authenticate",
    "authorization",
    "proxy-authorization",
    "x-api-key",
    "api-key",
    "x-auth-token",
    "x-amz-security-token",
};

/// Whether `c` is an RFC 7230 token character — `tchar`, the only bytes a real header name can
/// contain. Explicit ranges rather than std::isalnum: that one is locale-sensitive, and a locale
/// where an extra byte counts as alphanumeric would widen what passes as a header name.
bool IsHttpTokenChar(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        case '\'':
        case '*':
        case '+':
        case '-':
        case '.':
        case '^':
        case '_':
        case '`':
        case '|':
        case '~':
            return true;
        default:
            return false;
    }
}

/// Whether `name` is a syntactically valid header name.
///
/// This is a security control, not input hygiene. HTTP/2 response header names reach us from
/// `Http2Stream::response_headers_json()` → `decode_headers_stateless()`, which — as the name says —
/// decodes without HPACK dynamic-table state. When a name is given as a dynamic-table index,
/// `decode_literal_header` cannot resolve it and substitutes the literal `<unknown:N>`, while the
/// *value* is decoded normally and is the real value. Only fully-indexed fields degrade to
/// `<dynamic:N>` with no value and get dropped upstream.
///
/// That breaks a denylist in the worst possible way. Of kSensitiveResponseHeaders, only set-cookie,
/// www-authenticate, proxy-authenticate, authorization and proxy-authorization are in the HPACK
/// static table; set-cookie2, x-api-key, api-key, x-auth-token and x-amz-security-token are not — and
/// reusing a custom header via the dynamic table is precisely what HPACK is for. So on a long-lived
/// h2 connection the second and later `x-api-key: <secret>` would arrive as `<unknown:62>` and sail
/// past the denylist with its real value. The five names that fail are exactly the five added to
/// cover vendor tokens.
///
/// When the name is untrustworthy there is no way to tell whether the header is sensitive, so under a
/// denylist the only safe default is to drop the entry, value included: better to lose a diagnostic
/// field than to leak one credential. This rule also catches Huffman-decode fallout (frame.rs falls
/// back to `from_utf8_lossy` over the raw compressed bytes, producing garbage names) and pseudo-
/// headers such as `:status`, whose ':' is not a tchar — the status code is already emitted as
/// `http.response.status_code`, so `http.response.header.:status` would only duplicate it under a
/// malformed field name.
///
/// The request side needs no equivalent check: it is an allowlist, and no allowlisted name is
/// unparseable, so an undecodable name simply fails to match and is dropped. Note the same h2
/// mechanism costs a diagnostic there — `traceparent` is not in the static table either, so it
/// degrades to `<unknown:N>` and is lost. Failing closed is the point.
bool IsHttpTokenName(const std::string& name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) { return IsHttpTokenChar(c); });
}

/// Cap on emitted response headers, mirroring the bound the HTTP/1 path gets for free from httparse
/// (`parser/http/parser.rs`: `MAX_HEADERS = 64`). The h2 path has no such bound, and because every
/// header now becomes its own field name, an unbounded producer would grow field-name cardinality
/// without limit — a real operational problem for storage that builds a column per field name. This
/// only became a concern once headers stopped being one JSON blob under one field name.
constexpr size_t kMaxEmittedResponseHeaders = 64;

/// Emits response headers one attribute per header, as `http.response.header.<name>` (the OTel
/// semconv attribute template), skipping kSensitiveResponseHeaders and anything IsHttpTokenName
/// rejects, and stopping at kMaxEmittedResponseHeaders.
///
/// Per-header rather than one JSON blob because the denylist has to parse the JSON anyway: emitting
/// each header separately makes the filtering structural instead of a parse → filter → re-serialize
/// round trip, and the result is directly queryable.
///
/// Names are lowercased here even though both upstream producers already lowercase them
/// (`parser/http/parser.rs` collects into a HashMap keyed by `h.name.to_lowercase()`, and HPACK
/// header names are lowercase by spec) — the normalization is what makes the denylist match sound,
/// so it must not depend on the producer getting it right.
void EmitHttpResponseHeaders(logtail::LogEvent* log, const std::string& headersJson) {
    if (headersJson.empty()) {
        return;
    }
    rapidjson::Document doc;
    if (doc.Parse(headersJson.c_str(), headersJson.size()).HasParseError() || !doc.IsObject()) {
        return;
    }
    size_t emitted = 0;
    size_t dropped = 0;
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        if (!it->name.IsString() || !it->value.IsString() || it->value.GetStringLength() == 0) {
            continue;
        }
        std::string name(it->name.GetString(), it->name.GetStringLength());
        std::transform(
            name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // Order matters: the name must be proven trustworthy before the denylist verdict means
        // anything, so this check comes first.
        if (!IsHttpTokenName(name)) {
            ++dropped;
            continue;
        }
        const bool sensitive = std::any_of(kSensitiveResponseHeaders.begin(),
                                           kSensitiveResponseHeaders.end(),
                                           [&name](const char* denied) { return name == denied; });
        if (sensitive) {
            continue;
        }
        if (emitted >= kMaxEmittedResponseHeaders) {
            ++dropped;
            continue;
        }
        log->SetContent("http.response.header." + name,
                        std::string(it->value.GetString(), it->value.GetStringLength()));
        ++emitted;
    }
    if (dropped != 0) {
        // Not silent: an undecodable name or a truncated header set is exactly the situation where a
        // reader would otherwise conclude the response simply had no such header.
        LOG_DEBUG(sLogger,
                  ("Agentsight raw HTTP response headers dropped",
                   dropped)("emitted", emitted)("reason", "non-token name or per-log cap"));
    }
}

/// Response half of a raw HTTP exchange. Duration is carried by the gap between this event's
/// timestamp and the request event's, so no separate duration field is emitted.
void FillAgentsightHttpResponseLog(const AgentsightHttpsRecord& rec,
                                   logtail::LogEvent* log,
                                   const std::string& eventId,
                                   const std::string& exchangeId) {
    auto setStr = [&](StringView k, const std::string& v) {
        if (!v.empty()) {
            log->SetContent(k, StringView(v.data(), v.size()));
        }
    };

    FillAgentsightHttpCommon(
        rec, setStr, log, eventId, exchangeId, kHttpResponseEventSequence, rec.mTimestampNs + rec.mDurationNs);
    log->SetContent(StringView("event.name"), StringView("http.response"));
    log->SetContent("http.response.status_code", std::to_string(rec.mStatusCode));
    log->SetContent(StringView("is_sse"), StringView(rec.mIsSse ? "1" : "0"));

    EmitHttpResponseHeaders(log, rec.mResponseHeaders);
    EmitHttpBody(log, "http.response.body", rec.mResponseBody);
}

/// Resolves @containerId against the process-wide container inventory maintained by
/// ContainerManager (its polling loop is started by InputAgentSight in containerized deployments)
/// and attaches the standard container metadata tags to @group — the same channel and keys
/// input_file / input_container_stdio use, so downstream can join agentsight data with
/// file/stdio data on identical tag names.
///
/// Silently no-ops when the id is empty (host process) or unknown (container already removed from
/// the snapshot, or the event raced the first snapshot after startup): the record keeps its
/// `container.id` content field and nothing else changes. Deliberately no IsPurageContainerMode
/// gate here — the inventory is simply empty when the manager was never started, and the mode flag
/// is fixed at AppConfig construction, which would keep this path out of unit tests.
void AttachAgentsightContainerTags(PipelineEventGroup& group, const std::string& containerId) {
    if (containerId.empty()) {
        return;
    }
    const auto info = ContainerManager::GetInstance()->GetContainerInfoById(containerId);
    if (!info) {
        LOG_DEBUG(sLogger, ("Agentsight container meta not found", "")("containerId", containerId));
        return;
    }
    AttachAgentsightContainerTagsFromInfo(group, *info);
}

} // namespace

void AttachAgentsightContainerTagsFromInfo(PipelineEventGroup& group, const RawContainerInfo& info) {
    for (const auto& md : info.mMetadatas) {
        group.SetTag(GetDefaultTagKeyString(md.first), md.second);
    }
    for (const auto& md : info.mCustomMetadatas) {
        group.SetTag(md.first, md.second);
    }
}

AgentsightManager::AgentsightManager(const std::shared_ptr<ProcessCacheManager>& processCacheManager,
                                     const std::shared_ptr<EBPFAdapter>& eBPFAdapter,
                                     moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                     EventPool* pool,
                                     std::string hostRootPath,
                                     const size_t sessionInputCacheMaxSize)
    : AbstractManager(processCacheManager, eBPFAdapter, queue, pool),
      mHostRootPath(std::move(hostRootPath)),
      mSessionInputCache(sessionInputCacheMaxSize, 0) {
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

void AgentsightManager::clearSessionInputState() {
    mSessionInputCache.clear();
}

void AgentsightManager::releaseMetricRefs() {
    for (auto& item : mRefAndLabels) {
        if (mMetricMgr) {
            mMetricMgr->ReleaseReentrantMetricsRecordRef(item);
        }
    }
    mRefAndLabels.clear();
    mMetricMgr.reset();
    mRawHttpMetrics.reset();
    mGenAiMetrics.reset();
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

    // In container mode, point the library's pid lookups at the host procfs. Left alone it reads its
    // own /proc, which lists only processes sharing our pid namespace — agents in other pods stay
    // invisible unless the pod runs with hostPID. This mirrors what ProcessCacheManager already does
    // for the driver-based plugins (mHostPathPrefix / "proc"); no user configuration is involved.
    // Skipped on a host install, where mHostRootPath is "/" and the library's own default is /proc.
    if (!mHostRootPath.empty() && mHostRootPath != "/") {
        const std::string procfsRoot = PathJoin(mHostRootPath, "proc");
        if (sym->config_set_procfs_root) {
            sym->config_set_procfs_root(cfg, procfsRoot.c_str());
            LOG_INFO(sLogger, ("AgentSight", "pid lookups resolve through")("procfs_root", procfsRoot));
        } else {
            LOG_WARNING(sLogger,
                        ("AgentSight",
                         "running in container mode but agentsight_config_set_procfs_root symbol not found; pid "
                         "lookups keep using our own /proc and require hostPID to discover processes in other pid "
                         "namespaces (requires libagentsight >= 0.11.0)")("procfs_root", procfsRoot));
        }
    }

    // Raw HTTPS fallback is opt-in on both sides: the Rust FfiEventSender drops these events unless
    // the config flag is set, and handle_read below only registers the callback when it is on. Keep
    // mRawHttpsFallback in sync with what the library actually accepted so a missing symbol on an
    // older library does not leave us registering a callback that can never fire.
    mRawHttpsFallback = false;
    if (opts.mAgentsightRawHttpsFallback) {
        if (sym->config_set_enable_raw_https) {
            sym->config_set_enable_raw_https(cfg, 1);
            mRawHttpsFallback = true;
        } else {
            LOG_WARNING(sLogger,
                        ("AgentSight",
                         "RawHttpsFallback requested but agentsight_config_set_enable_raw_https symbol not found; "
                         "raw HTTP reporting disabled (requires libagentsight >= 0.9.0)"));
        }
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
    // A null https callback makes the Rust dispatcher drop raw HTTP events outright, which is what we
    // want when the fallback is off — no allocation, no enqueue.
    agentsight_https_callback_fn httpsCb = mRawHttpsFallback ? &AgentsightManager::OnHttpsCallback : nullptr;
    void* httpsUd = mRawHttpsFallback ? this : nullptr;
    int total = 0;
    for (;;) {
        const int r = sym->handle_read(mHandle, httpsCb, httpsUd, &AgentsightManager::OnLlmCallback, this, 0);
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
        ADD_COUNTER(self->mGenAiMetrics.inEventsTotal, 1);
    } else {
        ADD_COUNTER(self->mLossKernelEventsTotal, 1);
        ADD_COUNTER(self->mGenAiMetrics.lossEventsTotal, 1);
        LOG_WARNING(sLogger, ("AgentSight LLM event enqueue failed", ""));
    }
}

void AgentsightManager::OnHttpsCallback(const AgentsightHttpsData* data, void* user_data) {
    if (!data || !user_data) {
        return;
    }
    auto* self = static_cast<AgentsightManager*>(user_data);
    // Same locking contract as OnLlmCallback: runs inside handle_read while OnEpollReadable already
    // holds mLibMutex, so taking it here would deadlock.
    const std::string configName = self->mConfigName;
    auto evt = std::make_shared<AgentsightHttpsRecord>(configName, *data);
    if (self->mCommonEventQueue.try_enqueue(evt)) {
        ADD_COUNTER(self->mRawHttpMetrics.inEventsTotal, 1);
    } else {
        ADD_COUNTER(self->mLossKernelEventsTotal, 1);
        ADD_COUNTER(self->mRawHttpMetrics.lossEventsTotal, 1);
        LOG_WARNING(sLogger, ("AgentSight raw HTTP event enqueue failed", ""));
    }
}

int AgentsightManager::AddOrUpdateConfig(const CollectionPipelineContext* ctx,
                                         uint32_t index,
                                         const PluginMetricManagerPtr& metricMgr,
                                         const PluginOptions& opt) {
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
        // One ref per stream so raw HTTP and gen_ai counters are told apart by `record_type`; see
        // StreamMetrics. Same shape as network_observer's AppDetail, which keeps several labelled refs
        // in mRefAndLabels and releases them together.
        const auto initStreamMetrics = [&metricMgr, this](const std::string& recordType) {
            MetricLabels labels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG},
                                   {METRIC_LABEL_KEY_RECORD_TYPE, recordType}};
            auto ref = metricMgr->GetOrCreateReentrantMetricsRecordRef(labels);
            mRefAndLabels.emplace_back(labels);
            StreamMetrics metrics;
            metrics.inEventsTotal = ref->GetCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL);
            metrics.pushLogsTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL);
            metrics.pushLogGroupTotal = ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL);
            metrics.lossEventsTotal = ref->GetCounter(METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL);
            return metrics;
        };
        mRawHttpMetrics = initStreamMetrics(METRIC_LABEL_VALUE_RECORD_TYPE_RAW_HTTP);
        mGenAiMetrics = initStreamMetrics(METRIC_LABEL_VALUE_RECORD_TYPE_GEN_AI);
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
    mEventStreamFormat = sec->mAgentsightEventStreamFormat;
    mMessageDeltaOnly = sec->mAgentsightMessageDeltaOnly;

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
    clearSessionInputState();
    std::lock_guard<std::mutex> lock(mLibMutex);
    releaseMetricRefs();
    mRegisteredConfigCount = 0;
    mConfigName.clear();
    mPipelineCtx = nullptr;
    mQueueKey = 0;
    mPluginIndex = 0;
    mEventStreamFormat = true;
    mMessageDeltaOnly = true;
    mRawHttpsFallback = false;
    StopAgentSightLocked();
    return 0;
}

int AgentsightManager::Destroy() {
    clearSessionInputState();
    std::lock_guard<std::mutex> lock(mLibMutex);
    releaseMetricRefs();
    StopAgentSightLocked();
    mRegisteredConfigCount = 0;
    mConfigName.clear();
    mPipelineCtx = nullptr;
    mQueueKey = 0;
    mPluginIndex = 0;
    mEventStreamFormat = true;
    mMessageDeltaOnly = true;
    mRawHttpsFallback = false;
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

int AgentsightManager::update(const PluginOptions& opt) {
    const auto* secPtr = std::get_if<SecurityOptions*>(&opt);
    if (!secPtr || !*secPtr) {
        return 1;
    }
    std::lock_guard<std::mutex> lock(mLibMutex);
    mEventStreamFormat = (*secPtr)->mAgentsightEventStreamFormat;
    mMessageDeltaOnly = (*secPtr)->mAgentsightMessageDeltaOnly;
    return 0;
}

int AgentsightManager::resume(const PluginOptions& opt) {
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
    mEventStreamFormat = (*secPtr)->mAgentsightEventStreamFormat;
    mMessageDeltaOnly = (*secPtr)->mAgentsightMessageDeltaOnly;
    if (!RestartAgentSightLocked(**secPtr)) {
        return 1;
    }
    return 0;
}

std::unique_ptr<PluginConfig> AgentsightManager::GeneratePluginConfig(const PluginOptions&) {
    auto c = std::make_unique<PluginConfig>();
    c->mPluginType = PluginType::AGENTSIGHT_OBSERVE;
    c->mConfig = ProcessConfig{};
    return c;
}

int AgentsightManager::HandleEvent(const std::shared_ptr<CommonEvent>& event) {
    if (!event) {
        return 0;
    }
    switch (event->GetKernelEventType()) {
        case KernelEventType::AGENTSIGHT_LLM_RECORD:
            return HandleLlmEvent(static_cast<AgentsightLlmRecord*>(event.get()));
        case KernelEventType::AGENTSIGHT_HTTPS_RECORD:
            return HandleHttpsEvent(static_cast<AgentsightHttpsRecord*>(event.get()));
        default:
            return 0;
    }
}

int AgentsightManager::HandleHttpsEvent(const AgentsightHttpsRecord* rec) {
    if (!rec) {
        return 1;
    }

    logtail::QueueKey queueKey;
    uint32_t pluginIndex;
    {
        std::lock_guard<std::mutex> lock(mLibMutex);
        // mPipelineCtx != nullptr only proves *some* config is live, not that it is the one this record
        // was stamped under. mConfigName/mQueueKey are rebound on config switch, so a record enqueued
        // under config A that is still pending when A is removed and B registers would be pushed into
        // B's queue — and this path does no redaction, so that leaks A's raw body to B. Drop instead.
        if (mPipelineCtx == nullptr || rec->GetPipelineConfigName() != mConfigName) {
            ADD_COUNTER(mLossKernelEventsTotal, 1);
            ADD_COUNTER(mRawHttpMetrics.lossEventsTotal, 1);
            LOG_DEBUG(sLogger,
                      ("Agentsight raw HTTP event dropped", "config no longer registered")(
                          "recordConfig", rec->GetPipelineConfigName())("currentConfig", mConfigName));
            return 0;
        }
        queueKey = mQueueKey;
        pluginIndex = mPluginIndex;
    }

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    // Correlation mirrors the gen_ai.* path's split of responsibilities (see FillAgentsightHttpCommon):
    // one shared http.exchange.id pairs the two halves, each log gets its own event.id, and
    // event.sequence fixes their order. event.id must stay unique per log — downstream treats it as the
    // log's identity key, so sharing it across the pair would make dedup/join/idempotent-write drop one
    // half as a duplicate.
    //
    // Duration is represented by the two timestamps — there is no matching OTel semconv attribute.
    //
    // "Had a response" is decided by the response payload being present, NOT by status_code != 0.
    // Only agentsight's RequestOnly path deliberately means "no response" by status_code 0, and it
    // also clears response_headers/response_body. Http2StreamComplete reports whatever
    // Http2Stream::status_code() yields, which is 0 whenever the `:status` pseudo-header is missing
    // or its HPACK decode failed — with headers and body still fully populated. Keying off the
    // status code there would silently drop an entire http.response (headers *and* body) for h2
    // traffic, i.e. reintroduce on this path exactly the silent-drop this fallback exists to avoid.
    // status_code 0 is then emitted as-is, so downstream can tell "response present, status
    // unparseable" from "no response at all".
    const std::string exchangeId = CalculateRandomUUID();
    FillAgentsightHttpRequestLog(*rec, eventGroup.AddLogEvent(true, mEventPool), CalculateRandomUUID(), exchangeId);
    const bool hasResponse = rec->mStatusCode != 0 || !rec->mResponseBody.empty() || !rec->mResponseHeaders.empty();
    const size_t logCount = hasResponse ? 2U : 1U;
    if (logCount == 2U) {
        FillAgentsightHttpResponseLog(
            *rec, eventGroup.AddLogEvent(true, mEventPool), CalculateRandomUUID(), exchangeId);
    }

    AttachAgentsightContainerTags(eventGroup, rec->mContainerId);

    std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), pluginIndex);
    if (QueueStatus::OK == ProcessQueueManager::GetInstance()->PushQueue(queueKey, std::move(item))) {
        ADD_COUNTER(mRawHttpMetrics.pushLogsTotal, logCount);
        ADD_COUNTER(mRawHttpMetrics.pushLogGroupTotal, 1);
    } else {
        if (mPushLogFailedTotal) {
            ADD_COUNTER(mPushLogFailedTotal, 1);
        }
        LOG_WARNING(sLogger,
                    ("Agentsight raw HTTP push queue failed", "")("config", rec->GetPipelineConfigName())("pluginIdx",
                                                                                                          pluginIndex));
    }
    return 0;
}

int AgentsightManager::HandleLlmEvent(AgentsightLlmRecord* rec) {
    if (!rec) {
        return 1;
    }

    logtail::QueueKey queueKey;
    uint32_t pluginIndex;
    bool eventStreamFormat = true;
    bool messageDeltaOnly = true;
    {
        std::lock_guard<std::mutex> lock(mLibMutex);
        // Same stale-record guard as HandleHttpsEvent: drop what was stamped under a config that is no
        // longer the registered one rather than routing it to whichever config is live now. Dropping
        // before the session-cache update below also keeps config A's sessions out of B's delta state.
        if (mPipelineCtx == nullptr || rec->GetPipelineConfigName() != mConfigName) {
            ADD_COUNTER(mLossKernelEventsTotal, 1);
            ADD_COUNTER(mGenAiMetrics.lossEventsTotal, 1);
            LOG_DEBUG(sLogger,
                      ("Agentsight LLM event dropped", "config no longer registered")(
                          "recordConfig", rec->GetPipelineConfigName())("currentConfig", mConfigName));
            return 0;
        }
        queueKey = mQueueKey;
        pluginIndex = mPluginIndex;
        eventStreamFormat = mEventStreamFormat;
        messageDeltaOnly = mMessageDeltaOnly;
    }

    AgentsightLlmEmitPayload emitPayload;
    const std::string sessionKey = ResolveSessionStateKey(rec->mSessionId, rec->mConversationId);

    AgentsightSessionInputState previousCopy;
    const AgentsightSessionInputState* previous = nullptr;
    if (!sessionKey.empty() && mSessionInputCache.tryGetCopy(sessionKey, previousCopy)) {
        previous = &previousCopy;
    }

    emitPayload.precomputedDelta = ComputeInputMessagesDelta(rec->mRequestMessagesJson, previous);

    emitPayload.systemInstructionsJson = ExtractSystemInstructionsJson(rec->mRequestMessagesJson);
    emitPayload.systemInstructionsHash = ComputeSystemInstructionsHash(rec->mRequestMessagesJson);
    emitPayload.toolDefinitionsHash = ComputeToolDefinitionsHash(rec->mToolDefinitionsJson);
    const bool firstRoundInSession = previous == nullptr;
    emitPayload.emitSystemInstructions = !emitPayload.systemInstructionsHash.empty()
        && (firstRoundInSession || previousCopy.systemInstructionsHash != emitPayload.systemInstructionsHash);
    emitPayload.emitToolDefinitions = !emitPayload.toolDefinitionsHash.empty()
        && (firstRoundInSession || previousCopy.toolDefinitionsHash != emitPayload.toolDefinitionsHash);

    if (!sessionKey.empty()) {
        AgentsightSessionInputState sessionState = previous ? previousCopy : AgentsightSessionInputState{};
        if (rec->mConversationId != sessionState.lastTurnId) {
            sessionState.lastTurnId = rec->mConversationId;
            sessionState.nextStepNumber = 1;
            sessionState.nextEventSequence = 1;
        }
        emitPayload.stepId = FormatGenAiStepId(rec->mConversationId, sessionState.nextStepNumber++);
        if (eventStreamFormat) {
            emitPayload.eventSequenceRequest = sessionState.nextEventSequence++;
            emitPayload.eventSequenceResponse = sessionState.nextEventSequence++;
        }
        // Session/step state is committed before PushQueue. A queue failure drops delta
        // state for this round; use MessageDeltaOnly=false when full input fidelity is required.
        CommitSessionStateAfterEmit(
            rec->mRequestMessagesJson, rec->mResponseMessagesJson, rec->mToolDefinitionsJson, sessionState);
        mSessionInputCache.insert(sessionKey, std::move(sessionState));
    }

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    const size_t logCount = eventStreamFormat ? 2U : 1U;
    for (size_t i = 0; i < logCount; ++i) {
        auto* log = eventGroup.AddLogEvent(true, mEventPool);
        if (eventStreamFormat) {
            const std::string eventId = CalculateRandomUUID();
            if (i == 0) {
                FillAgentsightModelRequestLog(*rec, log, messageDeltaOnly, emitPayload, eventId);
            } else {
                FillAgentsightModelResponseLog(*rec, log, emitPayload, eventId);
            }
        } else {
            FillAgentsightCombinedLlmLog(*rec, log, messageDeltaOnly, emitPayload);
        }
    }

    AttachAgentsightContainerTags(eventGroup, rec->mContainerId);

    std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), pluginIndex);
    if (QueueStatus::OK == ProcessQueueManager::GetInstance()->PushQueue(queueKey, std::move(item))) {
        ADD_COUNTER(mGenAiMetrics.pushLogsTotal, logCount);
        ADD_COUNTER(mGenAiMetrics.pushLogGroupTotal, 1);
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
