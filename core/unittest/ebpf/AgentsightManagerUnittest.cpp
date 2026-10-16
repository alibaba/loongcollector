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

#include <cstring>
#include <sys/eventfd.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/StringView.h"
#include "constants/TagConstants.h"
#include "container_manager/ContainerManager.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "ebpf/plugin/agentsight/AgentsightManager.h"
#include "ebpf/type/FileEvent.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ManagerUnittestBase.h"

using namespace logtail;
using namespace logtail::ebpf;

namespace {

AgentsightConfigHandle* gFakeCfg = reinterpret_cast<AgentsightConfigHandle*>(0x10U);
AgentsightHandle* gFakeHandle = reinterpret_cast<AgentsightHandle*>(0x20U);

struct FakeReadControl {
    int start_ret = 0;
    /// 0: always 0; 1: return 1 once then 0; 2: call LLM callback once then 0;
    /// 3: call the raw HTTPS callback once then 0;
    /// 4/5: deliver `payload_events` synthetic LLM/HTTPS events, each carrying a `payload_size`
    ///      byte request payload (one event per handle_read call, so a single OnEpollReadable
    ///      delivers the whole batch) — used by the memory-limit tests
    int read_mode = 0;
    int read_step = 0;
    uint32_t payload_size = 0;
    int payload_events = 0;
} gRead;

bool g_config_new_null = false;

int gFakeAgentSightEventFd = -1;
bool gForceInvalidAgentSightEventFd = false;

const char* fake_last_error() {
    return "ut";
}

AgentsightConfigHandle* fake_config_new() {
    return g_config_new_null ? nullptr : gFakeCfg;
}

void fake_config_free(AgentsightConfigHandle* c) {
    (void)c;
}

void fake_config_set_verbose(AgentsightConfigHandle* c, int v) {
    (void)c;
    (void)v;
}

void fake_config_set_log_path(AgentsightConfigHandle* c, const char* p) {
    (void)c;
    (void)p;
}

int g_ut_cmdline_allow_calls = 0;
int g_ut_cmdline_deny_calls = 0;
int g_ut_https_calls = 0;
int g_ut_http_calls = 0;

int g_ut_raw_https_calls = 0;
int g_ut_raw_https_last_enabled = -1;
/// Simulates libagentsight < 0.9.0, where agentsight_config_set_enable_raw_https does not exist and
/// EBPFAdapter leaves the (optional) symbol null.
bool g_ut_omit_raw_https_symbol = false;
/// Whether the last handle_read call was handed a non-null raw HTTPS callback. A null callback is
/// how the manager tells the Rust dispatcher to drop raw events outright.
bool g_ut_last_read_had_https_cb = false;

void fake_config_set_enable_raw_https(AgentsightConfigHandle* cfg, int enabled) {
    (void)cfg;
    ++g_ut_raw_https_calls;
    g_ut_raw_https_last_enabled = enabled;
}

int g_ut_load_config_calls = 0;
std::string g_ut_load_config_json;
int g_ut_load_config_ret = 0;
/// Simulates a libagentsight without agentsight_config_load_config (pre-0.4.x), where EBPFAdapter
/// leaves the optional symbol null.
bool g_ut_omit_load_config_symbol = false;
/// Version reported by fake_agentsight_version. Tests lower it to simulate libraries that HAVE the
/// load_config symbol but predate (parts of) the runtime_limits schema — symbol presence alone must
/// not be trusted, because old serde ignores unknown JSON keys and still returns success.
const char* g_ut_agentsight_version = "0.12.1";
/// Simulates a library without the agentsight_version symbol (capability unverifiable → degrade).
bool g_ut_omit_version_symbol = false;

int fake_config_load_config(AgentsightConfigHandle* cfg, const char* json) {
    (void)cfg;
    ++g_ut_load_config_calls;
    g_ut_load_config_json = json ? json : "";
    return g_ut_load_config_ret;
}

const char* fake_agentsight_version() {
    return g_ut_agentsight_version;
}

void fake_config_add_cmdline_rule(AgentsightConfigHandle* cfg,
                                  const char* const* rule,
                                  const char* agent_name,
                                  int allow) {
    (void)cfg;
    (void)rule;
    (void)agent_name;
    if (allow != 0) {
        ++g_ut_cmdline_allow_calls;
    } else {
        ++g_ut_cmdline_deny_calls;
    }
}

void fake_config_add_https(AgentsightConfigHandle* cfg, const char* rule) {
    (void)cfg;
    (void)rule;
    ++g_ut_https_calls;
}

int fake_config_add_http(AgentsightConfigHandle* cfg, const char* target) {
    (void)cfg;
    (void)target;
    ++g_ut_http_calls;
    return 0;
}

AgentsightHandle* fake_handle_new(AgentsightConfigHandle* cfg) {
    (void)cfg;
    return gFakeHandle;
}

void fake_handle_free(AgentsightHandle* h) {
    (void)h;
}

int fake_handle_start(AgentsightHandle* h) {
    (void)h;
    return gRead.start_ret;
}

int fake_handle_stop(AgentsightHandle* h) {
    (void)h;
    return 0;
}

int fake_get_eventfd(AgentsightHandle* h) {
    (void)h;
    if (gForceInvalidAgentSightEventFd) {
        return -1;
    }
    return gFakeAgentSightEventFd;
}

// Static so pointers remain valid in LLM callback
static AgentsightLLMData sUtLlmData{};
// Static for the same reason on the raw HTTPS path.
static AgentsightHttpsData sUtHttpsData{};

int fake_handle_read(AgentsightHandle* h,
                     agentsight_https_callback_fn https,
                     void* https_user_data,
                     agentsight_llm_callback_fn llm,
                     void* user_data,
                     int flags) {
    (void)h;
    (void)flags;
    g_ut_last_read_had_https_cb = (https != nullptr);
    if (gRead.read_mode == 0) {
        return 0;
    }
    if (gRead.read_mode == 1) {
        if (gRead.read_step == 0) {
            gRead.read_step = 1;
            return 1;
        }
        return 0;
    }
    if (gRead.read_mode == 2) {
        static const char s[] = "conv-ut";
        static const char sid[] = "sess-ut";
        static const char rid[] = "resp-ut";
        std::memset(sUtLlmData.process_name, 0, sizeof(sUtLlmData.process_name));
        std::memcpy(sUtLlmData.process_name, "utp", 3U);
        sUtLlmData.conversation_id = s;
        sUtLlmData.session_id = sid;
        sUtLlmData.response_id = rid;
        sUtLlmData.timestamp_ns = 1U;
        if (llm != nullptr) {
            llm(&sUtLlmData, user_data);
        }
        gRead.read_mode = 0;
        return 1;
    }
    if (gRead.read_mode == 3) {
        std::memset(sUtHttpsData.process_name, 0, sizeof(sUtHttpsData.process_name));
        std::memcpy(sUtHttpsData.process_name, "raw", 3U);
        sUtHttpsData.method = "GET";
        sUtHttpsData.path = "/v1/unparsed";
        sUtHttpsData.timestamp_ns = 7U;
        // The manager must not invoke a callback it did not register; when raw fallback is off it
        // passes nullptr and the event has to be dropped here, exactly as the Rust side would.
        if (https != nullptr) {
            https(&sUtHttpsData, https_user_data);
        }
        gRead.read_mode = 0;
        return 1;
    }
    if (gRead.read_mode == 4 || gRead.read_mode == 5) {
        // Memory-limit batch mode: deliver payload_events synthetic events, one per call. The
        // payload buffer is static so the (ptr, len) pair stays valid through the deep copy.
        if (gRead.payload_events <= 0) {
            gRead.read_mode = 0;
            return 0;
        }
        --gRead.payload_events;
        static std::vector<char> sPayload;
        if (sPayload.size() != gRead.payload_size) {
            sPayload.assign(gRead.payload_size, 'x');
        }
        if (gRead.read_mode == 4) {
            static const char memSid[] = "sess-mem";
            std::memset(&sUtLlmData, 0, sizeof(sUtLlmData));
            std::memcpy(sUtLlmData.process_name, "utp", 3U);
            sUtLlmData.session_id = memSid;
            sUtLlmData.timestamp_ns = 1U;
            sUtLlmData.request_messages = sPayload.data();
            sUtLlmData.request_messages_len = static_cast<uint32_t>(sPayload.size());
            if (llm != nullptr) {
                llm(&sUtLlmData, user_data);
            }
        } else {
            std::memset(&sUtHttpsData, 0, sizeof(sUtHttpsData));
            std::memcpy(sUtHttpsData.process_name, "raw", 3U);
            sUtHttpsData.method = "POST";
            sUtHttpsData.path = "/v1/mem";
            sUtHttpsData.timestamp_ns = 7U;
            sUtHttpsData.request_body = sPayload.data();
            sUtHttpsData.request_body_len = static_cast<uint32_t>(sPayload.size());
            // Same contract as mode 3: a null callback means the manager never registered for raw
            // events and the fake Rust side must drop it.
            if (https != nullptr) {
                https(&sUtHttpsData, https_user_data);
            }
        }
        return 1;
    }
    return 0;
}

std::unique_ptr<AgentSightSymbolTable> makeFullSymbolTable() {
    auto t = std::make_unique<AgentSightSymbolTable>();
    t->last_error = fake_last_error;
    t->config_new = fake_config_new;
    t->config_free = fake_config_free;
    t->config_set_verbose = fake_config_set_verbose;
    t->config_set_log_path = fake_config_set_log_path;
    t->config_set_enable_raw_https = g_ut_omit_raw_https_symbol ? nullptr : fake_config_set_enable_raw_https;
    t->config_load_config = g_ut_omit_load_config_symbol ? nullptr : fake_config_load_config;
    t->version = g_ut_omit_version_symbol ? nullptr : fake_agentsight_version;
    t->config_add_cmdline_rule = fake_config_add_cmdline_rule;
    t->config_add_https = fake_config_add_https;
    t->config_add_http = fake_config_add_http;
    t->handle_new = fake_handle_new;
    t->handle_free = fake_handle_free;
    t->handle_start = fake_handle_start;
    t->handle_stop = fake_handle_stop;
    t->handle_get_eventfd = fake_get_eventfd;
    t->handle_read = fake_handle_read;
    return t;
}

std::shared_ptr<AgentsightLlmRecord> makeMinimalLlmRecord(const char* configName, const char* sessionId) {
    static AgentsightLLMData data{};
    std::memset(&data, 0, sizeof(data));
    data.session_id = sessionId;
    data.conversation_id = "turn-ut";
    data.response_id = "resp-ut";
    data.timestamp_ns = 1U;
    return std::make_shared<AgentsightLlmRecord>(std::string(configName), data);
}

/// Fields of a raw HTTP exchange a test cares about. Defaults describe the minimal shape AgentSight
/// produces when it could not parse the traffic: a request with no response yet (status_code == 0).
struct HttpsSpec {
    int32_t pid = 0;
    const char* processName = "";
    const char* cmdline = "";
    const char* agentName = nullptr;
    const char* containerId = nullptr;
    const char* method = "POST";
    const char* path = "/v1/unparsed";
    uint16_t statusCode = 0;
    uint8_t isSse = 0;
    uint64_t timestampNs = 1'000'000'000ULL;
    uint64_t durationNs = 0;
    std::string requestHeaders;
    std::string requestBody;
    std::string responseHeaders;
    std::string responseBody;
};

std::shared_ptr<AgentsightHttpsRecord> makeHttpsRecord(const char* configName, const HttpsSpec& s) {
    AgentsightHttpsData d{};
    d.pid = s.pid;
    std::strncpy(d.process_name, s.processName, sizeof(d.process_name) - 1U);
    std::strncpy(d.cmdline, s.cmdline, sizeof(d.cmdline) - 1U);
    d.agent_name = s.agentName;
    d.container_id = s.containerId;
    d.method = s.method;
    d.path = s.path;
    d.status_code = s.statusCode;
    d.is_sse = s.isSse;
    d.timestamp_ns = s.timestampNs;
    d.duration_ns = s.durationNs;
    d.request_headers = s.requestHeaders.data();
    d.request_headers_len = static_cast<uint32_t>(s.requestHeaders.size());
    d.request_body = s.requestBody.data();
    d.request_body_len = static_cast<uint32_t>(s.requestBody.size());
    d.response_headers = s.responseHeaders.data();
    d.response_headers_len = static_cast<uint32_t>(s.responseHeaders.size());
    d.response_body = s.responseBody.data();
    d.response_body_len = static_cast<uint32_t>(s.responseBody.size());
    // The record deep-copies every field, so `s` may die immediately after this returns.
    return std::make_shared<AgentsightHttpsRecord>(std::string(configName), d);
}

std::string contentOf(const LogEvent& log, const char* key) {
    const StringView v = log.GetContent(StringView(key));
    return std::string(v.data(), v.size());
}

std::string tagOf(const PipelineEventGroup& group, const char* key) {
    const StringView v = group.GetTag(StringView(key));
    return v.empty() ? std::string() : std::string(v.data(), v.size());
}

/// Fixture container for the metadata-enrichment tests. The ContainerManager singleton is
/// process-global and never torn down in this binary, so the seeded entry stays visible to later
/// tests; the ID is distinct enough that no other test fabricates it by accident.
const char* kUtContainerId = "c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00";

/// Seeds the process-wide ContainerManager singleton through the public persistence path (the same
/// one ContainerManagerUnittest::TestSaveLoadContainerInfo exercises): write a v1.0.0
/// docker_path_config.json and reload it, which swaps the whole container map. MetaDatas carries
/// the six standard keys (routed to RawContainerInfo::mMetadatas by AddMetadata) plus one custom
/// key (routed to mCustomMetadatas), covering both tag sources the enricher reads.
void seedContainerInventory() {
    Json::Value meta(Json::objectValue);
    meta["_container_name_"] = "ut-agent-container";
    meta["_image_name_"] = "registry.example.com/agent:ut";
    meta["_container_ip_"] = "172.17.0.7";
    meta["_pod_name_"] = "agent-pod-ut";
    meta["_namespace_"] = "ut-ns";
    meta["_pod_uid_"] = "ut-pod-uid";
    meta["app"] = "ut-custom-tag";
    Json::Value container(Json::objectValue);
    container["ID"] = kUtContainerId;
    container["Name"] = "ut-agent-container";
    container["MetaDatas"] = meta;
    Json::Value arr(Json::arrayValue);
    arr.append(container);
    Json::Value root(Json::objectValue);
    root["version"] = "1.0.0";
    root["Containers"] = arr;
    OverwriteFile(PathJoin(GetAgentDataDir(), "docker_path_config.json"), root.toStyledString());
    ContainerManager::GetInstance()->LoadContainerInfo();
}

/// The six standard container metadata tag keys, in default-key-name form.
const char* kUtContainerTagKeys[] = {
    "_container_name_",
    "_image_name_",
    "_container_ip_",
    "_pod_name_",
    "_namespace_",
    "_pod_uid_",
};

void expectUtContainerTags(const PipelineEventGroup& group) {
    APSARA_TEST_EQUAL("ut-agent-container", tagOf(group, "_container_name_"));
    APSARA_TEST_EQUAL("registry.example.com/agent:ut", tagOf(group, "_image_name_"));
    APSARA_TEST_EQUAL("172.17.0.7", tagOf(group, "_container_ip_"));
    APSARA_TEST_EQUAL("agent-pod-ut", tagOf(group, "_pod_name_"));
    APSARA_TEST_EQUAL("ut-ns", tagOf(group, "_namespace_"));
    APSARA_TEST_EQUAL("ut-pod-uid", tagOf(group, "_pod_uid_"));
    APSARA_TEST_EQUAL("ut-custom-tag", tagOf(group, "app"));
}

class AgentSightTestEBPFAdapter : public EBPFAdapter {
public:
    void setAgentSightSymbols(std::unique_ptr<AgentSightSymbolTable> sym) { mTestSyms = std::move(sym); }

    const AgentSightSymbolTable* GetAgentSightSymbols() const override { return mTestSyms.get(); }

    bool UpdatePlugin(PluginType, std::unique_ptr<PluginConfig>) override { return true; }

    bool ResumePlugin(PluginType, std::unique_ptr<PluginConfig>) override { return true; }

    bool SuspendPlugin(PluginType) override { return true; }

private:
    std::unique_ptr<AgentSightSymbolTable> mTestSyms;
};

} // namespace

// Expose protected resume (branch mRegisteredConfigCount == 0, invalid options).
class TestableAgentsightManager : public AgentsightManager {
public:
    using AgentsightManager::AgentsightManager;
    using AgentsightManager::resume;
};

class AgentsightManagerUnittest : public ManagerUnittestWithProcessCacheManager {
public:
    void SetUp() override {
        ManagerUnittestWithProcessCacheManager::SetUp();
        gFakeAgentSightEventFd = static_cast<int>(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
        APSARA_TEST_TRUE(gFakeAgentSightEventFd >= 0);
        gForceInvalidAgentSightEventFd = false;
        // Reset the fake library's globals before building the symbol table: makeFullSymbolTable()
        // reads g_ut_omit_raw_https_symbol, so a table built first would inherit the previous test's
        // value and silently drop config_set_enable_raw_https.
        gRead = decltype(gRead){};
        gRead.start_ret = 0;
        g_config_new_null = false;
        g_ut_cmdline_allow_calls = 0;
        g_ut_cmdline_deny_calls = 0;
        g_ut_https_calls = 0;
        g_ut_http_calls = 0;
        g_ut_raw_https_calls = 0;
        g_ut_raw_https_last_enabled = -1;
        g_ut_omit_raw_https_symbol = false;
        g_ut_last_read_had_https_cb = false;
        g_ut_load_config_calls = 0;
        g_ut_load_config_json.clear();
        g_ut_load_config_ret = 0;
        g_ut_omit_load_config_symbol = false;
        g_ut_agentsight_version = "0.12.1";
        g_ut_omit_version_symbol = false;
        mAgentSightAdapter = std::make_shared<AgentSightTestEBPFAdapter>();
        mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
        auto& o = agentsightOptions();
        o.mAgentsightCmdlineWhitelist.clear();
        o.mAgentsightCmdlineBlacklist.clear();
        o.mAgentsightHttps.clear();
        o.mAgentsightHttp.clear();
        o.mAgentsightRawHttpsFallback = false;
        o.mAgentsightMemoryLimitEnabled = false;
        o.mAgentsightMemoryLimitBytes = static_cast<uint64_t>(kAgentsightMemoryLimitMBDefault) * 1024 * 1024;
        o.mAgentsightRuntimeLimitsJson.clear();
    }

    void TearDown() override {
        if (gFakeAgentSightEventFd >= 0) {
            ::close(gFakeAgentSightEventFd);
            gFakeAgentSightEventFd = -1;
        }
        mAgentSightAdapter.reset();
        ManagerUnittestWithProcessCacheManager::TearDown();
    }

    std::shared_ptr<AbstractManager> createManagerInstance() override { return makeManager(); }

    PluginOptions createTestOptions() override { return asVariant(); }

    static SecurityOptions& agentsightOptions() {
        static SecurityOptions o;
        o.mProbeType = SecurityProbeType::AGENTSIGHT_OBSERVE;
        o.mVerbose = 0;
        o.mLogPath.clear();
        return o;
    }

    PluginOptions asVariant() { return &agentsightOptions(); }

    std::shared_ptr<AgentsightManager> makeManager(const size_t sessionInputCacheMaxSize = 4096) {
        // "/" is the host-mode root: the procfs-root setter path is skipped for it.
        auto m = std::make_shared<AgentsightManager>(mProcessCacheManager,
                                                     std::static_pointer_cast<EBPFAdapter>(mAgentSightAdapter),
                                                     *mEventQueue,
                                                     mEventPool.get(),
                                                     "/",
                                                     sessionInputCacheMaxSize);
        APSARA_TEST_EQUAL(0, m->Init());
        return m;
    }

    void registerConfig(AgentsightManager& mgr, const char* configName) {
        CollectionPipelineContext ctx;
        ctx.SetConfigName(configName);
        ctx.SetProcessQueueKey(1);
        APSARA_TEST_EQUAL(0, mgr.AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    }

    void populateSessionInputCache(AgentsightManager& mgr, const char* configName, const char* const* sessionIds) {
        for (const char* const* it = sessionIds; *it != nullptr; ++it) {
            APSARA_TEST_EQUAL(0, mgr.HandleEvent(makeMinimalLlmRecord(configName, *it)));
        }
    }

    /// Registers a config against a real, poppable process queue so a test can read back the log
    /// events the manager actually pushed, rather than only asserting the push succeeded.
    QueueKey registerConfigWithPoppableQueue(AgentsightManager& mgr, const char* configName) {
        const QueueKey key = QueueKeyManager::GetInstance()->GetKey(configName);
        CollectionPipelineContext ctx;
        ctx.SetConfigName(configName);
        ctx.SetProcessQueueKey(key);
        ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key, 0, ctx);
        ProcessQueueManager::GetInstance()->EnablePop(configName);
        APSARA_TEST_EQUAL(0, mgr.AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
        // ProcessQueueManager is a singleton and queues from earlier tests in this binary are never
        // torn down, so PopItem(0, ...) could otherwise return someone else's leftovers.
        drainProcessQueues();
        return key;
    }

    static void drainProcessQueues() {
        std::unique_ptr<ProcessQueueItem> item;
        std::string configName;
        while (ProcessQueueManager::GetInstance()->PopItem(0, item, configName)) {
        }
    }

    /// Pops the single group the manager pushed for one record. Returns an empty group if nothing was
    /// pushed, so callers can assert on the drop path too.
    static std::vector<LogEvent*> popLogEvents(std::unique_ptr<ProcessQueueItem>& item) {
        std::vector<LogEvent*> logs;
        std::string configName;
        if (!ProcessQueueManager::GetInstance()->PopItem(0, item, configName)) {
            return logs;
        }
        for (auto& e : item->mEventGroup.MutableEvents()) {
            logs.push_back(&e.Cast<LogEvent>());
        }
        return logs;
    }

    /// Runs one record through the manager and returns the emitted logs. `item` keeps the popped
    /// group (and thus the LogEvent storage) alive for the caller's assertions.
    std::vector<LogEvent*> emitHttps(AgentsightManager& mgr,
                                     const char* configName,
                                     const HttpsSpec& spec,
                                     std::unique_ptr<ProcessQueueItem>& item) {
        APSARA_TEST_EQUAL(0, mgr.HandleEvent(makeHttpsRecord(configName, spec)));
        return popLogEvents(item);
    }

    /// server.address extracted from a request-headers JSON blob, or "" when none was emitted.
    std::string serverAddressFor(AgentsightManager& mgr, const char* configName, const std::string& headersJson) {
        HttpsSpec spec;
        spec.requestHeaders = headersJson;
        std::unique_ptr<ProcessQueueItem> item;
        const auto logs = emitHttps(mgr, configName, spec, item);
        if (logs.size() != 1U) {
            return "<no-log>";
        }
        return contentOf(*logs[0], "server.address");
    }

    void TestAddOrUpdateValidation();
    void TestAddOrUpdateNoSymbols();
    void TestRestartStartFailure();
    void TestConfigNewNull();
    void TestAddRemoveDestroy();
    void TestSecondAddOrUpdate();
    void TestOnEpollDrain();
    void TestOnEpollNoHandle();
    void TestAddOrUpdateInvalidEventFd();
    void TestHandleEventBranches();
    void TestResumeInvalidOptions();
    void TestResumeWithNoRegistration();
    void TestSuspend();
    void TestDestroyTwice();
    void TestGetPluginType();
    void TestCmdlineHttpsHttpRulesInvokedOnAddOrUpdate();
    void TestBuiltinCmdlineRulesInjectedWhenCmdlineOmitted();
    void TestUserBlacklistOnlySkipsBuiltinAllowInjection();
    void TestRemoveConfigClearsSessionInputCache();
    void TestDestroyClearsSessionInputCache();
    void TestSessionInputCacheLruEviction();
    void TestRawHttpsFallbackOffByDefault();
    void TestRawHttpsFallbackEnabledRegistersCallback();
    void TestRawHttpsFallbackMissingSymbolDisablesReporting();
    void TestRawHttpsFallbackResetOnRemoveAndDestroy();
    void TestHttpsEventRequestOnlyEmitsOneLog();
    void TestHttpsEventPairCorrelationFields();
    void TestHttpsEventRequestHeaderAllowlist();
    void TestHttpsEventWithoutPipelineContextDrops();
    void TestHttpsEventFiltersSensitiveResponseHeaders();
    void TestHttpsEventDropsUndecodableHeaderNames();
    void TestHttpsEventUnparsableStatusStillEmitsResponse();
    void TestStaleConfigRecordDroppedNotRerouted();
    void TestHttpsHostExtractionVariants();
    void TestHttpsEventOmitsEmptyOptionalFields();
    void TestLlmServerAddressFromRequestUrl();
    void TestAttachAgentsightContainerTagsFromInfo();
    void TestContainerTagsAttachedForLlmEvent();
    void TestContainerTagsAttachedForHttpsEvent();
    void TestContainerTagsMissDegradesSilently();
    void TestMemoryLimitDisabledByDefault();
    void TestRuntimeLimitsJsonPassthrough();
    void TestRuntimeLimitsJsonWrappedAndUnknownKeys();
    void TestRuntimeLimitsJsonBadJsonIgnored();
    void TestRuntimeLimitsJsonIndependentOfByteGate();
    void TestMemoryLimitDropsOversizedNewEvents();
    void TestMemoryLimitRecoversAfterDrain();
    void TestMemoryLimitMissingLoadConfigSymbol();
    void TestMemoryLimitLoadConfigFailureDegrades();
    void TestMemoryLimitHotUpdateKeepsUsedConsistent();
    void TestMemoryLimitStaleConfigDropReleasesBytes();
    void TestMemoryLimitHttpsStreamAccounted();
    void TestMemoryLimitOldLibraryVersionDegrades();
    void TestMemoryLimitVersionGateBoundary();
    void TestMemoryLimitMissingVersionSymbolDegrades();

protected:
    std::shared_ptr<AgentSightTestEBPFAdapter> mAgentSightAdapter;
};

void AgentsightManagerUnittest::TestGetPluginType() {
    auto m = makeManager();
    APSARA_TEST_EQUAL(PluginType::AGENTSIGHT_OBSERVE, m->GetPluginType());
    m->Destroy();
}

void AgentsightManagerUnittest::TestAddOrUpdateValidation() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);

    ObserverNetworkOption o{};
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&o)));

    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(nullptr, 0, nullptr, PluginOptions(&agentsightOptions())));

    {
        static SecurityOptions wrong;
        wrong = agentsightOptions();
        wrong.mProbeType = SecurityProbeType::FILE;
        APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&wrong)));
    }

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestAddOrUpdateNoSymbols() {
    mAgentSightAdapter->setAgentSightSymbols(nullptr);
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    mgr->Destroy();
    mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
}

void AgentsightManagerUnittest::TestRestartStartFailure() {
    gRead.start_ret = 1;
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, mgr->RegisteredConfigCount());
    gRead.start_ret = 0;
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestConfigNewNull() {
    g_config_new_null = true;
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    g_config_new_null = false;
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestAddRemoveDestroy() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, mgr->RemoveConfig("p1"));
    APSARA_TEST_EQUAL(0, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, mgr->Destroy());
}

void AgentsightManagerUnittest::TestSecondAddOrUpdate() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 1, nullptr, asVariant()));
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    mgr->RemoveConfig("p1");
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestOnEpollNoHandle() {
    auto mgr = makeManager();
    APSARA_TEST_EQUAL(0, mgr->OnEpollReadable());
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    mgr->RemoveConfig("p1");
    APSARA_TEST_EQUAL(0, mgr->OnEpollReadable());
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestOnEpollDrain() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    gRead.read_mode = 0;
    APSARA_TEST_EQUAL(0, mgr->OnEpollReadable());
    gRead.read_mode = 1;
    gRead.read_step = 0;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};

    gRead.read_mode = 2;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};

    APSARA_TEST_EQUAL(0, mgr->PollPerfBuffer(0));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestAddOrUpdateInvalidEventFd() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    gForceInvalidAgentSightEventFd = true;
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    gForceInvalidAgentSightEventFd = false;
    APSARA_TEST_EQUAL(0, mgr->RegisteredConfigCount());
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHandleEventBranches() {
    auto mgr = makeManager();
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(nullptr));

    auto notAgentsight = std::make_shared<FileEvent>(1U, 2U, KernelEventType::FILE_PATH_TRUNCATE, 0ULL);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(notAgentsight));

    static AgentsightLLMData d{}, d0{};
    d0.conversation_id = "c0";
    // No pipeline: mPipelineCtx is null
    auto orphan = std::make_shared<AgentsightLlmRecord>(std::string("orphan"), d0);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(orphan));

    CollectionPipelineContext cctx;
    cctx.SetConfigName("p1");
    cctx.SetProcessQueueKey(99);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&cctx, 0, nullptr, asVariant()));
    d.conversation_id = "c1";
    auto rec2 = std::make_shared<AgentsightLlmRecord>(std::string("p1"), d);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(rec2));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestResumeInvalidOptions() {
    auto p = new TestableAgentsightManager(mProcessCacheManager,
                                           std::static_pointer_cast<EBPFAdapter>(mAgentSightAdapter),
                                           *mEventQueue,
                                           mEventPool.get(),
                                           "/");
    std::shared_ptr<TestableAgentsightManager> mgr(p);
    APSARA_TEST_EQUAL(0, mgr->Init());
    PluginOptions nullSec{static_cast<SecurityOptions*>(nullptr)};
    APSARA_TEST_NOT_EQUAL(0, mgr->resume(nullSec));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestResumeWithNoRegistration() {
    auto p = new TestableAgentsightManager(mProcessCacheManager,
                                           std::static_pointer_cast<EBPFAdapter>(mAgentSightAdapter),
                                           *mEventQueue,
                                           mEventPool.get(),
                                           "/");
    std::shared_ptr<TestableAgentsightManager> mgr(p);
    APSARA_TEST_EQUAL(0, mgr->Init());
    APSARA_TEST_EQUAL(0, mgr->resume(asVariant()));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestSuspend() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, mgr->Suspend());
    APSARA_TEST_EQUAL(0, mgr->RemoveConfig("p1"));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestDestroyTwice() {
    auto mgr = makeManager();
    APSARA_TEST_EQUAL(0, mgr->Destroy());
    APSARA_TEST_EQUAL(0, mgr->Destroy());
}

void AgentsightManagerUnittest::TestCmdlineHttpsHttpRulesInvokedOnAddOrUpdate() {
    auto& o = agentsightOptions();
    o.mAgentsightCmdlineWhitelist = {AgentsightCmdlineAllowRule{"claude-code", {"node", "*claude*"}},
                                     AgentsightCmdlineAllowRule{"claude-code", {"node", "*claude*"}}};
    o.mAgentsightCmdlineBlacklist = {{"node", "*webpack*"}};
    o.mAgentsightHttps = {"*.openai.com", "*.anthropic.com"};
    o.mAgentsightHttp = {":8080", "10.0.0.1:9090", "model-svc.default.svc", "*.internal.svc"};

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(2, g_ut_cmdline_allow_calls);
    APSARA_TEST_EQUAL(1, g_ut_cmdline_deny_calls);
    APSARA_TEST_EQUAL(2, g_ut_https_calls);
    APSARA_TEST_EQUAL(4, g_ut_http_calls);
    mgr->RemoveConfig("p1");
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestBuiltinCmdlineRulesInjectedWhenCmdlineOmitted() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(9, g_ut_cmdline_allow_calls);
    APSARA_TEST_EQUAL(0, g_ut_cmdline_deny_calls);
    APSARA_TEST_EQUAL(7, g_ut_https_calls);
    APSARA_TEST_EQUAL(0, g_ut_http_calls);
    mgr->RemoveConfig("p1");
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestUserBlacklistOnlySkipsBuiltinAllowInjection() {
    auto& o = agentsightOptions();
    o.mAgentsightCmdlineBlacklist = {{"node", "*webpack*"}};

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, g_ut_cmdline_allow_calls);
    APSARA_TEST_EQUAL(1, g_ut_cmdline_deny_calls);
    APSARA_TEST_EQUAL(7, g_ut_https_calls);
    APSARA_TEST_EQUAL(0, g_ut_http_calls);
    mgr->RemoveConfig("p1");
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRemoveConfigClearsSessionInputCache() {
    const char* kConfigName = "p_remove_cache";
    static const char* kSessionIds[] = {"sess-remove-1", "sess-remove-2", nullptr};

    auto mgr = makeManager();
    registerConfig(*mgr, kConfigName);
    populateSessionInputCache(*mgr, kConfigName, kSessionIds);
    APSARA_TEST_EQUAL(2, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-remove-1"));
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-remove-2"));

    APSARA_TEST_EQUAL(0, mgr->RemoveConfig(kConfigName));
    APSARA_TEST_EQUAL(0, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_TRUE(!mgr->SessionInputCacheContainsForTest("sess-remove-1"));
    APSARA_TEST_TRUE(!mgr->SessionInputCacheContainsForTest("sess-remove-2"));

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestDestroyClearsSessionInputCache() {
    const char* kConfigName = "p_destroy_cache";
    static const char* kSessionIds[] = {"sess-destroy-1", "sess-destroy-2", nullptr};

    auto mgr = makeManager();
    registerConfig(*mgr, kConfigName);
    populateSessionInputCache(*mgr, kConfigName, kSessionIds);
    APSARA_TEST_EQUAL(2, mgr->GetSessionInputCacheSizeForTest());

    APSARA_TEST_EQUAL(0, mgr->Destroy());
    APSARA_TEST_EQUAL(0, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_TRUE(!mgr->SessionInputCacheContainsForTest("sess-destroy-1"));
    APSARA_TEST_TRUE(!mgr->SessionInputCacheContainsForTest("sess-destroy-2"));
}

void AgentsightManagerUnittest::TestSessionInputCacheLruEviction() {
    const char* kConfigName = "p_lru_evict";
    static constexpr size_t kCacheCap = 2;

    auto mgr = makeManager(kCacheCap);
    registerConfig(*mgr, kConfigName);

    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeMinimalLlmRecord(kConfigName, "sess-lru-1")));
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeMinimalLlmRecord(kConfigName, "sess-lru-2")));
    APSARA_TEST_EQUAL(kCacheCap, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-lru-1"));
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-lru-2"));

    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeMinimalLlmRecord(kConfigName, "sess-lru-3")));
    APSARA_TEST_EQUAL(kCacheCap, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_TRUE(!mgr->SessionInputCacheContainsForTest("sess-lru-1"));
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-lru-2"));
    APSARA_TEST_TRUE(mgr->SessionInputCacheContainsForTest("sess-lru-3"));

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRawHttpsFallbackOffByDefault() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_raw_off");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    // Never opted in, so the library is never told to enable raw events.
    APSARA_TEST_EQUAL(0, g_ut_raw_https_calls);

    // And handle_read gets a null https callback, which is what makes the Rust side drop raw events
    // before allocating anything.
    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    APSARA_TEST_FALSE(g_ut_last_read_had_https_cb);

    std::shared_ptr<CommonEvent> dequeued;
    APSARA_TEST_FALSE(mEventQueue->try_dequeue(dequeued));

    gRead = decltype(gRead){};
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRawHttpsFallbackEnabledRegistersCallback() {
    agentsightOptions().mAgentsightRawHttpsFallback = true;

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_raw_on");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    APSARA_TEST_EQUAL(1, g_ut_raw_https_calls);
    APSARA_TEST_EQUAL(1, g_ut_raw_https_last_enabled);

    // With the flag accepted, handle_read is handed a real callback and the event it delivers is
    // enqueued as an AGENTSIGHT_HTTPS_RECORD.
    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    APSARA_TEST_TRUE(g_ut_last_read_had_https_cb);

    std::shared_ptr<CommonEvent> dequeued;
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(dequeued));
    APSARA_TEST_TRUE(dequeued != nullptr);
    APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_HTTPS_RECORD, dequeued->GetKernelEventType());
    auto* raw = static_cast<AgentsightHttpsRecord*>(dequeued.get());
    APSARA_TEST_EQUAL("p_raw_on", raw->GetPipelineConfigName());
    APSARA_TEST_EQUAL("raw", raw->mProcessName);
    APSARA_TEST_EQUAL("GET", raw->mMethod);
    APSARA_TEST_EQUAL("/v1/unparsed", raw->mPath);

    gRead = decltype(gRead){};
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRawHttpsFallbackMissingSymbolDisablesReporting() {
    // libagentsight < 0.9.0: the optional symbol is absent. The config must still load — the whole
    // point of resolving it separately is that an old library keeps working — but raw reporting has
    // to stay off rather than registering a callback that can never fire.
    g_ut_omit_raw_https_symbol = true;
    mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
    agentsightOptions().mAgentsightRawHttpsFallback = true;

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_raw_nosym");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, g_ut_raw_https_calls);

    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    APSARA_TEST_FALSE(g_ut_last_read_had_https_cb);

    std::shared_ptr<CommonEvent> dequeued;
    APSARA_TEST_FALSE(mEventQueue->try_dequeue(dequeued));

    gRead = decltype(gRead){};
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRawHttpsFallbackResetOnRemoveAndDestroy() {
    agentsightOptions().mAgentsightRawHttpsFallback = true;

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_raw_reset");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, mgr->RemoveConfig("p_raw_reset"));

    // After RemoveConfig the flag is back to its default, so a later read cannot keep passing a
    // callback for a config that no longer exists.
    gRead.read_mode = 3;
    APSARA_TEST_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};

    // A fresh registration re-enables it, and Destroy clears it again.
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    APSARA_TEST_TRUE(g_ut_last_read_had_https_cb);
    gRead = decltype(gRead){};

    APSARA_TEST_EQUAL(0, mgr->Destroy());
    gRead.read_mode = 3;
    APSARA_TEST_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};
}

void AgentsightManagerUnittest::TestHttpsEventRequestOnlyEmitsOneLog() {
    const char* kConfig = "p_raw_req_only";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.pid = 4242;
    spec.processName = "node";
    spec.cmdline = "node /app/agent.js";
    spec.agentName = "hermes";
    spec.containerId = "abc123def456";
    spec.method = "POST";
    spec.path = "/v1/unparsed";
    spec.timestampNs = 1'700'000'000'000'000'000ULL;
    spec.requestHeaders = R"({"host":"api.example.com:8443","authorization":"Bearer sk-secret"})";
    spec.requestBody = R"({"prompt":"hi"})";
    // status_code 0 *and* an empty response side (no headers, no body) means no response ever
    // arrived — agentsight's RequestOnly path — so only the request half is emitted. status_code 0
    // on its own is not enough; see TestHttpsEventUnparsableStatusStillEmitsResponse.
    spec.statusCode = 0;

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(1UL, logs.size());

    const LogEvent& req = *logs[0];
    APSARA_TEST_EQUAL("http.request", contentOf(req, "event.name"));
    APSARA_TEST_EQUAL("POST", contentOf(req, "http.request.method"));
    APSARA_TEST_EQUAL("/v1/unparsed", contentOf(req, "url.path"));
    APSARA_TEST_EQUAL("https", contentOf(req, "url.scheme"));
    APSARA_TEST_EQUAL("4242", contentOf(req, "pid"));
    APSARA_TEST_EQUAL("node", contentOf(req, "comm"));
    APSARA_TEST_EQUAL("node /app/agent.js", contentOf(req, "cmdline"));
    APSARA_TEST_EQUAL("abc123def456", contentOf(req, "container.id"));
    // Raw events carry no GenAI semantics, so the agent type is plain `agent.type`.
    APSARA_TEST_EQUAL("hermes", contentOf(req, "agent.type"));
    APSARA_TEST_FALSE(req.HasContent(StringView("gen_ai.agent.type")));
    // Host salvaged out of the headers, which are themselves never emitted.
    APSARA_TEST_EQUAL("api.example.com", contentOf(req, "server.address"));
    APSARA_TEST_EQUAL("8443", contentOf(req, "server.port"));
    APSARA_TEST_EQUAL(spec.requestBody, contentOf(req, "http.request.body.content"));
    APSARA_TEST_EQUAL(std::to_string(spec.requestBody.size()), contentOf(req, "http.request.body.size"));
    APSARA_TEST_TRUE(req.HasContent(StringView("event.id")));

    // No response half at all.
    APSARA_TEST_FALSE(req.HasContent(StringView("http.response.status_code")));

    // Nothing else was pushed.
    std::unique_ptr<ProcessQueueItem> extra;
    APSARA_TEST_TRUE(popLogEvents(extra).empty());
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventPairCorrelationFields() {
    const char* kConfig = "p_raw_pair";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.method = "POST";
    spec.path = "/v1/messages";
    spec.statusCode = 503;
    spec.isSse = 1;
    spec.timestampNs = 1'700'000'000'000'000'000ULL;
    spec.durationNs = 250'000'000ULL;
    spec.requestHeaders = R"({"host":"api.example.com"})";
    spec.requestBody = "req-payload";
    spec.responseHeaders = R"({"content-type":"text/event-stream"})";
    spec.responseBody = "res-payload";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());

    const LogEvent& req = *logs[0];
    const LogEvent& res = *logs[1];
    APSARA_TEST_EQUAL("http.request", contentOf(req, "event.name"));
    APSARA_TEST_EQUAL("http.response", contentOf(res, "event.name"));

    // Correlation is split three ways, matching the gen_ai.* path: http.exchange.id pairs the two
    // halves, event.id stays unique per log, event.sequence orders them.
    const std::string exchangeId = contentOf(req, "http.exchange.id");
    APSARA_TEST_TRUE(!exchangeId.empty());
    APSARA_TEST_EQUAL(exchangeId, contentOf(res, "http.exchange.id"));

    // event.id is the per-log identity key: sharing it across the pair would let downstream dedup or
    // idempotent writes discard one half as a duplicate.
    const std::string reqEventId = contentOf(req, "event.id");
    const std::string resEventId = contentOf(res, "event.id");
    APSARA_TEST_TRUE(!reqEventId.empty());
    APSARA_TEST_TRUE(!resEventId.empty());
    APSARA_TEST_NOT_EQUAL(reqEventId, resEventId);
    APSARA_TEST_NOT_EQUAL(exchangeId, reqEventId);
    APSARA_TEST_NOT_EQUAL(exchangeId, resEventId);

    // Order is explicit, not inferred from timestamps — those collide when mDurationNs is 0.
    APSARA_TEST_EQUAL("1", contentOf(req, "event.sequence"));
    APSARA_TEST_EQUAL("2", contentOf(res, "event.sequence"));

    APSARA_TEST_EQUAL("503", contentOf(res, "http.response.status_code"));
    APSARA_TEST_EQUAL("1", contentOf(res, "is_sse"));
    // Headers are emitted one attribute per header, not as the raw JSON blob.
    APSARA_TEST_EQUAL("text/event-stream", contentOf(res, "http.response.header.content-type"));
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header")));
    APSARA_TEST_EQUAL(spec.responseBody, contentOf(res, "http.response.body.content"));
    APSARA_TEST_EQUAL(std::to_string(spec.responseBody.size()), contentOf(res, "http.response.body.size"));

    // Duration is carried by the timestamp gap, not a duration field: the response is stamped at
    // request time + duration.
    APSARA_TEST_EQUAL(static_cast<time_t>(spec.timestampNs / 1'000'000'000ULL), req.GetTimestamp());
    APSARA_TEST_EQUAL(static_cast<time_t>((spec.timestampNs + spec.durationNs) / 1'000'000'000ULL), res.GetTimestamp());

    // Response-half fields must not leak into the request half.
    APSARA_TEST_FALSE(req.HasContent(StringView("http.response.status_code")));
    APSARA_TEST_FALSE(req.HasContent(StringView("is_sse")));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventRequestHeaderAllowlist() {
    // Request headers go through an allowlist, the mirror image of the response side's denylist. Two
    // properties to hold at once: credentials never reach the log (this path reports verbatim bytes
    // with no redaction), and the handful of headers that explain "why was this unparsable" do.
    const char* kConfig = "p_raw_req_hdr_allowlist";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.statusCode = 200;
    spec.requestBody = "req-payload";
    spec.requestHeaders = R"({)"
                          R"("host":"api.example.com",)"
                          R"("content-type":"application/octet-stream",)"
                          R"("content-length":"11",)"
                          R"("traceparent":"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",)"
                          R"("User-Agent":"anthropic-sdk-python/0.39.0",)"
                          R"("authorization":"Bearer sk-leak-me",)"
                          R"("x-api-key":"kk-leak",)"
                          R"("cookie":"session=cookie-leak",)"
                          R"("x-vendor-secret":"unknown-vendor-leak")"
                          R"(})";
    spec.responseHeaders = R"({"content-type":"application/json"})";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());
    const LogEvent& req = *logs[0];

    // Allowlisted headers are emitted, one attribute each, name lowercased.
    APSARA_TEST_EQUAL("application/octet-stream", contentOf(req, "http.request.header.content-type"));
    APSARA_TEST_EQUAL("11", contentOf(req, "http.request.header.content-length"));
    APSARA_TEST_EQUAL("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
                      contentOf(req, "http.request.header.traceparent"));
    // content-length is the cross-check for body fidelity, so it must be comparable to the body size.
    APSARA_TEST_EQUAL(contentOf(req, "http.request.header.content-length"), contentOf(req, "http.request.body.size"));

    // Host and user-agent are salvaged into stable OTel attributes rather than the header namespace,
    // and must not also appear under http.request.header.* — that would duplicate the value.
    APSARA_TEST_EQUAL("api.example.com", contentOf(req, "server.address"));
    APSARA_TEST_EQUAL("anthropic-sdk-python/0.39.0", contentOf(req, "user_agent.original"));
    APSARA_TEST_FALSE(req.HasContent(StringView("http.request.header.host")));
    APSARA_TEST_FALSE(req.HasContent(StringView("http.request.header.user-agent")));

    // Credential-bearing headers are absent, including a vendor header the allowlist has never heard
    // of — an unknown name is dropped, which is the whole point of allowlisting this side.
    for (const char* denied : {"authorization", "x-api-key", "cookie", "x-vendor-secret"}) {
        const std::string key = std::string("http.request.header.") + denied;
        APSARA_TEST_FALSE(req.HasContent(StringView(key.data(), key.size())));
    }

    for (const LogEvent* log : logs) {
        // The old whole-blob field names must never come back.
        APSARA_TEST_FALSE(log->HasContent(StringView("http.request.header")));
        APSARA_TEST_FALSE(log->HasContent(StringView("http.request.headers")));
        for (const auto& kv : *log) {
            const std::string value(kv.second.data(), kv.second.size());
            APSARA_TEST_TRUE(value.find("sk-leak-me") == std::string::npos);
            APSARA_TEST_TRUE(value.find("kk-leak") == std::string::npos);
            APSARA_TEST_TRUE(value.find("cookie-leak") == std::string::npos);
            APSARA_TEST_TRUE(value.find("unknown-vendor-leak") == std::string::npos);
        }
    }
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventWithoutPipelineContextDrops() {
    // No config registered, so mPipelineCtx is null and the record is dropped without a push.
    auto mgr = makeManager();
    drainProcessQueues();
    HttpsSpec spec;
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeHttpsRecord("orphan-raw", spec)));

    std::unique_ptr<ProcessQueueItem> item;
    APSARA_TEST_TRUE(popLogEvents(item).empty());
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventFiltersSensitiveResponseHeaders() {
    // Response headers go through a denylist, unlike request headers which are dropped wholesale.
    // Credential-bearing names must never reach the log; everything else is kept, because response
    // headers are what makes this diagnostic fallback useful.
    const char* kConfig = "p_raw_resp_hdrs";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.statusCode = 200;
    spec.responseHeaders = R"({)"
                           R"("content-type":"application/json",)"
                           R"("content-length":"42",)"
                           R"("x-request-id":"req-abc",)"
                           R"("set-cookie":"session=REPLAYABLE; HttpOnly",)"
                           R"("set-cookie2":"legacy=REPLAYABLE",)"
                           R"("www-authenticate":"Bearer realm=\"x\", error=\"invalid_token\"",)"
                           R"("proxy-authenticate":"Basic realm=\"proxy\"",)"
                           R"("authorization":"Bearer LEAK",)"
                           R"("proxy-authorization":"Basic LEAK",)"
                           R"("x-api-key":"LEAK",)"
                           R"("api-key":"LEAK",)"
                           R"("x-auth-token":"LEAK",)"
                           R"("x-amz-security-token":"LEAK",)"
                           R"("X-Auth-Token":"LEAK-UPPERCASE")"
                           R"(})";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());
    const LogEvent& res = *logs[1];

    // Non-sensitive headers survive, one attribute each, name lowercased.
    APSARA_TEST_EQUAL("application/json", contentOf(res, "http.response.header.content-type"));
    APSARA_TEST_EQUAL("42", contentOf(res, "http.response.header.content-length"));
    APSARA_TEST_EQUAL("req-abc", contentOf(res, "http.response.header.x-request-id"));

    // Every denylisted name is absent...
    for (const char* denied : {"set-cookie",
                               "set-cookie2",
                               "www-authenticate",
                               "proxy-authenticate",
                               "authorization",
                               "proxy-authorization",
                               "x-api-key",
                               "api-key",
                               "x-auth-token",
                               "x-amz-security-token"}) {
        const std::string key = std::string("http.response.header.") + denied;
        APSARA_TEST_FALSE(res.HasContent(StringView(key.data(), key.size())));
    }

    // ...and no credential value leaked under any other key, including via a header whose name
    // arrived with original casing (the denylist matches on the lowercased name).
    for (const auto& kv : res) {
        const std::string value(kv.second.data(), kv.second.size());
        APSARA_TEST_TRUE(value.find("LEAK") == std::string::npos);
        APSARA_TEST_TRUE(value.find("REPLAYABLE") == std::string::npos);
        APSARA_TEST_TRUE(value.find("invalid_token") == std::string::npos);
    }

    // The whole-blob field is gone entirely — it would have carried everything past the filter.
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header")));

    // Malformed or non-object header JSON yields no header attributes rather than a bogus one.
    for (const char* bad : {"not json", R"(["content-type","application/json"])", R"({"content-type":123})"}) {
        HttpsSpec badSpec;
        badSpec.statusCode = 200;
        badSpec.responseHeaders = bad;
        std::unique_ptr<ProcessQueueItem> badItem;
        const auto badLogs = emitHttps(*mgr, kConfig, badSpec, badItem);
        APSARA_TEST_EQUAL(2UL, badLogs.size());
        APSARA_TEST_FALSE(badLogs[1]->HasContent(StringView("http.response.header.content-type")));
    }

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventDropsUndecodableHeaderNames() {
    // The denylist can only be trusted when the header name is trustworthy, and on HTTP/2 it often is
    // not: decode_headers_stateless() has no HPACK dynamic-table state, so a name given as a dynamic
    // index becomes the literal "<unknown:N>" while the value decodes for real. Five of the ten
    // denylisted names are absent from the HPACK static table (x-api-key among them), so without a
    // syntax check on the name they would sail straight through with their secrets.
    const char* kConfig = "p_raw_h2_hdr_names";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.statusCode = 200;
    spec.responseHeaders = R"({)"
                           R"("content-type":"application/grpc",)"
                           R"("<unknown:62>":"kk-leak",)"
                           R"("<dynamic:70>":"dyn-leak",)"
                           R"(":status":"200",)"
                           R"("��garbage":"huffman-leak")"
                           R"(})";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());
    const LogEvent& res = *logs[1];

    // A well-formed name still gets through.
    APSARA_TEST_EQUAL("application/grpc", contentOf(res, "http.response.header.content-type"));

    // Names that are not RFC 7230 tokens are dropped whole, value included.
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header.<unknown:62>")));
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header.<dynamic:70>")));
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header.:status")));

    // The status code is still reported, just through its own field rather than a pseudo-header echo.
    APSARA_TEST_EQUAL("200", contentOf(res, "http.response.status_code"));

    // No dropped value survives under any key.
    for (const auto& kv : res) {
        const std::string value(kv.second.data(), kv.second.size());
        APSARA_TEST_TRUE(value.find("kk-leak") == std::string::npos);
        APSARA_TEST_TRUE(value.find("dyn-leak") == std::string::npos);
        APSARA_TEST_TRUE(value.find("huffman-leak") == std::string::npos);
    }

    // Per-log header cap: h2 has no upstream bound, and each header is now its own field name, so an
    // unbounded producer would grow field-name cardinality without limit.
    std::string many = "{";
    for (int i = 0; i < 80; ++i) {
        if (i != 0) {
            many += ",";
        }
        many += R"("x-h)" + std::to_string(i) + R"(":"v")";
    }
    many += "}";

    HttpsSpec manySpec;
    manySpec.statusCode = 200;
    manySpec.responseHeaders = many;
    std::unique_ptr<ProcessQueueItem> manyItem;
    const auto manyLogs = emitHttps(*mgr, kConfig, manySpec, manyItem);
    APSARA_TEST_EQUAL(2UL, manyLogs.size());
    size_t headerFields = 0;
    for (const auto& kv : *manyLogs[1]) {
        const std::string key(kv.first.data(), kv.first.size());
        if (key.rfind("http.response.header.", 0) == 0) {
            ++headerFields;
        }
    }
    APSARA_TEST_EQUAL(64UL, headerFields);

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventUnparsableStatusStillEmitsResponse() {
    // HTTP/2 whose `:status` pseudo-header could not be HPACK-decoded: Http2Stream::status_code()
    // yields 0 while response headers and body are fully populated. The response half must still be
    // emitted — suppressing it would silently lose an entire gRPC/h2 response, the exact failure mode
    // this raw fallback exists to prevent.
    const char* kConfig = "p_raw_h2_no_status";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.method = "POST";
    spec.path = "/grpc.Service/Call";
    spec.statusCode = 0;
    spec.requestBody = "req-payload";
    spec.responseHeaders = R"({"content-type":"application/grpc"})";
    spec.responseBody = "h2-response-that-must-not-be-dropped";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());

    const LogEvent& res = *logs[1];
    APSARA_TEST_EQUAL("http.response", contentOf(res, "event.name"));
    APSARA_TEST_EQUAL(spec.responseBody, contentOf(res, "http.response.body.content"));
    APSARA_TEST_EQUAL("application/grpc", contentOf(res, "http.response.header.content-type"));
    // Reported as 0 rather than suppressed, so downstream can separate "response present, status
    // unparseable" from "no response at all" — the latter has no http.response log at all.
    APSARA_TEST_EQUAL("0", contentOf(res, "http.response.status_code"));

    // durationNs is 0 here, so both halves carry the same timestamp and event.sequence is the only
    // thing that can order them.
    const LogEvent& req = *logs[0];
    APSARA_TEST_EQUAL(req.GetTimestamp(), res.GetTimestamp());
    APSARA_TEST_EQUAL("1", contentOf(req, "event.sequence"));
    APSARA_TEST_EQUAL("2", contentOf(res, "event.sequence"));
    APSARA_TEST_EQUAL(contentOf(req, "http.exchange.id"), contentOf(res, "http.exchange.id"));
    APSARA_TEST_NOT_EQUAL(contentOf(req, "event.id"), contentOf(res, "event.id"));

    // Either half of the response side alone is enough: headers survived but body was empty...
    HttpsSpec headersOnly;
    headersOnly.statusCode = 0;
    headersOnly.responseHeaders = R"({"content-type":"application/grpc"})";
    std::unique_ptr<ProcessQueueItem> headersItem;
    APSARA_TEST_EQUAL(2UL, emitHttps(*mgr, kConfig, headersOnly, headersItem).size());

    // ...and body survived but the header block was lost to a failed HPACK decode.
    HttpsSpec bodyOnly;
    bodyOnly.statusCode = 0;
    bodyOnly.responseBody = "body-without-headers";
    std::unique_ptr<ProcessQueueItem> bodyItem;
    APSARA_TEST_EQUAL(2UL, emitHttps(*mgr, kConfig, bodyOnly, bodyItem).size());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestStaleConfigRecordDroppedNotRerouted() {
    // Config A is removed and B registered while records stamped under A are still pending. Those must
    // be dropped rather than pushed into B's queue: the raw path does no redaction, so re-routing would
    // hand A's un-redacted body to a different config's pipeline.
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, "p_stale_a");
    APSARA_TEST_EQUAL(0, mgr->RemoveConfig("p_stale_a"));
    registerConfigWithPoppableQueue(*mgr, "p_stale_b");

    HttpsSpec spec;
    spec.requestBody = "body-belonging-to-a";
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeHttpsRecord("p_stale_a", spec)));
    std::unique_ptr<ProcessQueueItem> staleItem;
    APSARA_TEST_TRUE(popLogEvents(staleItem).empty());

    // A record stamped under the live config still flows, so the guard is not a blanket drop.
    std::unique_ptr<ProcessQueueItem> liveItem;
    APSARA_TEST_EQUAL(1U, emitHttps(*mgr, "p_stale_b", spec, liveItem).size());

    // Same guard on the LLM path. Checked through the session cache too: dropping happens before the
    // cache update, so config A's session must not become part of B's delta state.
    const size_t cacheSizeBefore = mgr->GetSessionInputCacheSizeForTest();
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeMinimalLlmRecord("p_stale_a", "sess-stale")));
    APSARA_TEST_EQUAL(cacheSizeBefore, mgr->GetSessionInputCacheSizeForTest());
    APSARA_TEST_FALSE(mgr->SessionInputCacheContainsForTest("sess-stale"));
    std::unique_ptr<ProcessQueueItem> staleLlmItem;
    APSARA_TEST_TRUE(popLogEvents(staleLlmItem).empty());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsHostExtractionVariants() {
    const char* kConfig = "p_raw_hosts";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    // Plain `host`, with and without a port.
    APSARA_TEST_EQUAL("api.example.com", serverAddressFor(*mgr, kConfig, R"({"host":"api.example.com:8443"})"));
    APSARA_TEST_EQUAL("api.example.com", serverAddressFor(*mgr, kConfig, R"({"host":"api.example.com"})"));

    // HTTP/2 sends the target as the `:authority` pseudo-header instead.
    APSARA_TEST_EQUAL("h2.example.com", serverAddressFor(*mgr, kConfig, R"({":authority":"h2.example.com"})"));

    // `host` wins over `:authority` when both are present.
    APSARA_TEST_EQUAL("first.example.com",
                      serverAddressFor(*mgr, kConfig, R"({"host":"first.example.com",":authority":"second.example"})"));

    // Producers that preserve original casing are handled by the case-insensitive fallback scan. The
    // value itself is passed through verbatim — no lowercasing.
    APSARA_TEST_EQUAL("Mixed.Example.COM", serverAddressFor(*mgr, kConfig, R"({"Host":"Mixed.Example.COM:9000"})"));

    // IPv6 literals keep their brackets stripped, port split off.
    APSARA_TEST_EQUAL("::1", serverAddressFor(*mgr, kConfig, R"({"host":"[::1]:8080"})"));

    // Nothing to extract: absent, empty, non-string, non-object, malformed, and empty input all yield
    // no server.address rather than a bogus one.
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, R"({"content-type":"application/json"})"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, R"({"host":""})"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, R"({"host":1234})"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, R"(["host","api.example.com"])"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, "not json at all"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, "{}"));
    APSARA_TEST_EQUAL("", serverAddressFor(*mgr, kConfig, ""));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestHttpsEventOmitsEmptyOptionalFields() {
    // An exited process with no container, no rule match, and no bodies: every empty field is skipped
    // rather than emitted blank, so consumers can distinguish "absent" from "empty".
    const char* kConfig = "p_raw_sparse";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.pid = 0;
    spec.statusCode = 204;

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());

    const LogEvent& req = *logs[0];
    APSARA_TEST_FALSE(req.HasContent(StringView("pid")));
    APSARA_TEST_FALSE(req.HasContent(StringView("comm")));
    APSARA_TEST_FALSE(req.HasContent(StringView("cmdline")));
    APSARA_TEST_FALSE(req.HasContent(StringView("container.id")));
    APSARA_TEST_FALSE(req.HasContent(StringView("agent.type")));
    APSARA_TEST_FALSE(req.HasContent(StringView("server.address")));
    APSARA_TEST_FALSE(req.HasContent(StringView("server.port")));
    APSARA_TEST_FALSE(req.HasContent(StringView("user_agent.original")));
    APSARA_TEST_FALSE(req.HasContent(StringView("http.request.header.content-type")));
    APSARA_TEST_FALSE(req.HasContent(StringView("http.request.body.content")));
    APSARA_TEST_FALSE(req.HasContent(StringView("http.request.body.size")));
    // Method / path / scheme are always present.
    APSARA_TEST_EQUAL("https", contentOf(req, "url.scheme"));
    APSARA_TEST_EQUAL("/v1/unparsed", contentOf(req, "url.path"));

    const LogEvent& res = *logs[1];
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header")));
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.header.content-type")));
    APSARA_TEST_FALSE(res.HasContent(StringView("http.response.body.content")));
    // status_code and is_sse are unconditional, even at their zero values.
    APSARA_TEST_EQUAL("204", contentOf(res, "http.response.status_code"));
    APSARA_TEST_EQUAL("0", contentOf(res, "is_sse"));
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestLlmServerAddressFromRequestUrl() {
    // The LLM path feeds a full URL through the same splitter the raw path uses for a bare authority.
    const char* kConfig = "p_llm_url";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    static AgentsightLLMData d{};
    std::memset(&d, 0, sizeof(d));
    d.session_id = "sess-url";
    d.conversation_id = "turn-url";
    d.response_id = "resp-url";
    d.timestamp_ns = 1'700'000'000'000'000'000ULL;
    d.request_url = "https://user@api.openai.com:8443/v1/chat/completions?stream=true";

    APSARA_TEST_EQUAL(0, mgr->HandleEvent(std::make_shared<AgentsightLlmRecord>(std::string(kConfig), d)));

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = popLogEvents(item);
    APSARA_TEST_TRUE(!logs.empty());
    bool sawServer = false;
    for (const LogEvent* log : logs) {
        if (log->HasContent(StringView("server.address"))) {
            sawServer = true;
            APSARA_TEST_EQUAL("api.openai.com", contentOf(*log, "server.address"));
            APSARA_TEST_EQUAL("8443", contentOf(*log, "server.port"));
        }
    }
    APSARA_TEST_TRUE(sawServer);
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestAttachAgentsightContainerTagsFromInfo() {
    // Pure function: no singleton involved, tags come straight from the info's metadata vectors.
    RawContainerInfo info;
    info.AddMetadata("_container_name_", "pure-container"); // known key -> mMetadatas
    info.AddMetadata("_image_name_", "registry.example.com/pure:1");
    info.AddMetadata("app", "pure-custom"); // unknown key -> mCustomMetadatas

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    AttachAgentsightContainerTagsFromInfo(group, info);

    APSARA_TEST_EQUAL("pure-container", tagOf(group, "_container_name_"));
    APSARA_TEST_EQUAL("registry.example.com/pure:1", tagOf(group, "_image_name_"));
    APSARA_TEST_EQUAL("pure-custom", tagOf(group, "app"));
    APSARA_TEST_FALSE(group.HasTag(StringView("_pod_name_")));

    // Empty info attaches nothing.
    RawContainerInfo empty;
    PipelineEventGroup bareGroup(std::make_shared<SourceBuffer>());
    AttachAgentsightContainerTagsFromInfo(bareGroup, empty);
    APSARA_TEST_EQUAL(0UL, bareGroup.GetTags().size());
}

void AgentsightManagerUnittest::TestContainerTagsAttachedForLlmEvent() {
    seedContainerInventory();
    const char* kConfig = "p_container_tags_llm";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);
    // EventStreamFormat is static state shared across tests; pin the split shape explicitly.
    agentsightOptions().mAgentsightEventStreamFormat = true;

    auto rec = makeMinimalLlmRecord(kConfig, "sess-container");
    rec->mContainerId = kUtContainerId;
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(rec));

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = popLogEvents(item);
    APSARA_TEST_EQUAL(2UL, logs.size());
    expectUtContainerTags(item->mEventGroup);
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestContainerTagsAttachedForHttpsEvent() {
    seedContainerInventory();
    const char* kConfig = "p_container_tags_https";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    HttpsSpec spec;
    spec.containerId = kUtContainerId;
    spec.statusCode = 200;
    spec.responseBody = "ok";

    std::unique_ptr<ProcessQueueItem> item;
    const auto logs = emitHttps(*mgr, kConfig, spec, item);
    APSARA_TEST_EQUAL(2UL, logs.size());
    expectUtContainerTags(item->mEventGroup);
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestContainerTagsMissDegradesSilently() {
    seedContainerInventory();
    const char* kConfig = "p_container_tags_miss";
    auto mgr = makeManager();
    registerConfigWithPoppableQueue(*mgr, kConfig);

    // Unknown container ID (container gone from the snapshot, or a short/variant form): the event
    // is still pushed and simply carries no container tags.
    {
        auto rec = makeMinimalLlmRecord(kConfig, "sess-unknown-container");
        rec->mContainerId = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
        APSARA_TEST_EQUAL(0, mgr->HandleEvent(rec));
        std::unique_ptr<ProcessQueueItem> item;
        const auto logs = popLogEvents(item);
        APSARA_TEST_TRUE(!logs.empty());
        for (const char* key : kUtContainerTagKeys) {
            APSARA_TEST_FALSE(item->mEventGroup.HasTag(StringView(key)));
        }
    }
    // Empty container ID (host process): same degradation, exercised through the raw HTTP path.
    {
        HttpsSpec spec;
        spec.containerId = nullptr;
        std::unique_ptr<ProcessQueueItem> item;
        const auto logs = emitHttps(*mgr, kConfig, spec, item);
        APSARA_TEST_TRUE(!logs.empty());
        for (const char* key : kUtContainerTagKeys) {
            APSARA_TEST_FALSE(item->mEventGroup.HasTag(StringView(key)));
        }
    }
    mgr->Destroy();
}

// ================================================================================================
// Memory limit: C++ byte gate (MemoryLimitEnabled / MemoryLimitMB) and the independent Rust-side
// runtime_limits passthrough (RuntimeLimitsJson)
// ================================================================================================

namespace {
// 4MB payload per event; a 10MB budget accepts exactly 2 and rejects the 3rd regardless of the
// few-hundred-byte fixed record overhead — keeps the drop arithmetic deterministic.
constexpr uint32_t kMemLimitTestPayload = 4U * 1024 * 1024;
constexpr uint64_t kMemLimitTestLimit = 10ULL * 1024 * 1024;

/// Delivers `count` synthetic LLM events of kMemLimitTestPayload bytes through one OnEpollReadable.
void feedLlmBatch(AgentsightManager& mgr, int count) {
    gRead.read_mode = 4;
    gRead.payload_size = kMemLimitTestPayload;
    gRead.payload_events = count;
    mgr.OnEpollReadable();
    gRead = decltype(gRead){};
}
} // namespace

void AgentsightManagerUnittest::TestMemoryLimitDisabledByDefault() {
    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_off");

    // Off by default: no runtime_limits JSON is pushed and the budget stays disarmed.
    APSARA_TEST_EQUAL(0, g_ut_load_config_calls);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Limit());

    feedLlmBatch(*mgr, 1);
    std::shared_ptr<CommonEvent> dequeued;
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(dequeued));
    APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_LLM_RECORD, dequeued->GetKernelEventType());
    // Disabled path never adopts accounting: no bytes held, nothing stamped on the record.
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0),
                      static_cast<AgentsightLlmRecord*>(dequeued.get())->GetAccountedBytesForTest());
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().DroppedEvents());

    dequeued.reset();
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRuntimeLimitsJsonPassthrough() {
    auto& o = agentsightOptions();
    // Verbatim passthrough: exactly the operator's keys are sent, wrapped into the config document
    // shape. Missing sub-keys are NOT completed from any formula — they fall back to the library
    // defaults on the Rust side (load_from_json replaces the whole runtime_limits block).
    o.mAgentsightRuntimeLimitsJson = R"({"event_channel_capacity":2048,"ring_buffer_mb":8})";

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_json");

    APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
    Json::Value root;
    std::string err;
    APSARA_TEST_TRUE(ParseJsonTable(g_ut_load_config_json, root, err));
    const Json::Value& rl = root["runtime_limits"];
    APSARA_TEST_EQUAL(2U, rl.size());
    APSARA_TEST_EQUAL(2048U, rl["event_channel_capacity"].asUInt());
    APSARA_TEST_EQUAL(8U, rl["ring_buffer_mb"].asUInt());
    // Independent of the C++ byte gate: gate untouched → budget stays disarmed.
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Limit());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRuntimeLimitsJsonWrappedAndUnknownKeys() {
    auto& o = agentsightOptions();
    // The wrapped document form (mirroring the library's own agentsight.json) is accepted and
    // re-wrapped; unknown sub-keys pass through verbatim (forward compatibility with newer
    // libraries that add keys) — warned about, never silently dropped.
    o.mAgentsightRuntimeLimitsJson = R"({"runtime_limits":{"max_connection_body_mb":4,"some_future_key":7}})";

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_wrap");

    APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
    Json::Value root;
    std::string err;
    APSARA_TEST_TRUE(ParseJsonTable(g_ut_load_config_json, root, err));
    const Json::Value& rl = root["runtime_limits"];
    APSARA_TEST_EQUAL(2U, rl.size());
    APSARA_TEST_EQUAL(4U, rl["max_connection_body_mb"].asUInt());
    APSARA_TEST_EQUAL(7U, rl["some_future_key"].asUInt());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestRuntimeLimitsJsonBadJsonIgnored() {
    {
        // Malformed JSON: warn and skip the push; registration must still succeed.
        agentsightOptions().mAgentsightRuntimeLimitsJson = "{not-json";
        auto mgr = makeManager();
        registerConfig(*mgr, "p_mem_badjson");
        APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
        APSARA_TEST_EQUAL(0, g_ut_load_config_calls);
        mgr->Destroy();
    }
    {
        // A non-object runtime_limits member is equally unusable — same honest skip.
        agentsightOptions().mAgentsightRuntimeLimitsJson = R"({"runtime_limits":42})";
        auto mgr = makeManager();
        registerConfig(*mgr, "p_mem_badwrap");
        APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
        APSARA_TEST_EQUAL(0, g_ut_load_config_calls);
        mgr->Destroy();
    }
}

void AgentsightManagerUnittest::TestRuntimeLimitsJsonIndependentOfByteGate() {
    auto& o = agentsightOptions();
    // Both halves at once: the JSON rides to the Rust side while the C++ byte gate arms separately.
    o.mAgentsightRuntimeLimitsJson = R"({"pending_genai_max_count":256})";
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_both");

    APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
    Json::Value root;
    std::string err;
    APSARA_TEST_TRUE(ParseJsonTable(g_ut_load_config_json, root, err));
    APSARA_TEST_EQUAL(256U, root["runtime_limits"]["pending_genai_max_count"].asUInt());
    APSARA_TEST_EQUAL(kMemLimitTestLimit, mgr->GetMemoryBudgetForTest().Limit());

    feedLlmBatch(*mgr, 3); // 2 × 4MB fit under 10MB; the 3rd is rejected by the gate
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        e.reset();
    }
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitDropsOversizedNewEvents() {
    auto& o = agentsightOptions();
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_drop");

    feedLlmBatch(*mgr, 3); // 2 × 4MB fit under 10MB; the 3rd is rejected before the deep copy
    const auto& budget = mgr->GetMemoryBudgetForTest();
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), budget.DroppedEvents());
    APSARA_TEST_TRUE(budget.DroppedBytes() >= kMemLimitTestPayload);
    APSARA_TEST_TRUE(budget.Used() >= 2 * static_cast<uint64_t>(kMemLimitTestPayload));
    APSARA_TEST_TRUE(budget.Used() <= kMemLimitTestLimit);

    int accepted = 0;
    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        ++accepted;
        APSARA_TEST_TRUE(static_cast<AgentsightLlmRecord*>(e.get())->GetAccountedBytesForTest()
                         >= kMemLimitTestPayload);
        e.reset();
    }
    APSARA_TEST_EQUAL(2, accepted);
    // All accepted records destroyed → RAII released everything.
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), budget.Used());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitRecoversAfterDrain() {
    auto& o = agentsightOptions();
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    // Success-path HandleEvent pushes ProcessQueueItems holding LogEvents allocated from the
    // fixture's EventPool. They must be popped and destroyed BEFORE the fixture dies: the
    // ProcessQueueManager singleton outlives every fixture, and a residue there is a
    // heap-use-after-free at process exit (caught by ASan in CI). Hence poppable queue + drain.
    registerConfigWithPoppableQueue(*mgr, "p_mem_recover");

    feedLlmBatch(*mgr, 3); // 2 accepted, 1 dropped
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    // Drain through the real consumption path (HandleEvent) and destroy the records.
    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        mgr->HandleEvent(e);
        e.reset();
    }
    drainProcessQueues(); // destroy what HandleEvent pushed, while the EventPool is still alive
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());

    // Usage back under the limit → admission resumes with no hysteresis and no extra state.
    feedLlmBatch(*mgr, 1);
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(e));
    APSARA_TEST_TRUE(static_cast<AgentsightLlmRecord*>(e.get())->GetAccountedBytesForTest() >= kMemLimitTestPayload);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents()); // unchanged
    e.reset();
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitMissingLoadConfigSymbol() {
    // libagentsight without agentsight_config_load_config: registration must still succeed (warn
    // only, RawHttpsFallback discipline) and the C++-side budget must still enforce the limit.
    g_ut_omit_load_config_symbol = true;
    mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
    auto& o = agentsightOptions();
    o.mAgentsightRuntimeLimitsJson = R"({"ring_buffer_mb":8})";
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_mem_nosym");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, g_ut_load_config_calls);
    APSARA_TEST_EQUAL(kMemLimitTestLimit, mgr->GetMemoryBudgetForTest().Limit());

    feedLlmBatch(*mgr, 3);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        e.reset();
    }
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitLoadConfigFailureDegrades() {
    // load_config returns < 0 (JSON parse failure inside the library): warn and continue — the
    // restart must not fail and the C++ budget keeps enforcing.
    g_ut_load_config_ret = -1;
    auto& o = agentsightOptions();
    o.mAgentsightRuntimeLimitsJson = R"({"ring_buffer_mb":8})";
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_loadfail");
    APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
    APSARA_TEST_EQUAL(kMemLimitTestLimit, mgr->GetMemoryBudgetForTest().Limit());

    feedLlmBatch(*mgr, 3);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        e.reset();
    }
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitHotUpdateKeepsUsedConsistent() {
    auto& o = agentsightOptions();
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    // Same lifetime discipline as TestMemoryLimitRecoversAfterDrain: HandleEvent below pushes into
    // a real queue, so use a poppable one and drain it before the fixture's EventPool dies.
    const QueueKey queueKey = registerConfigWithPoppableQueue(*mgr, "p_mem_hot");
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_mem_hot");
    ctx.SetProcessQueueKey(queueKey);

    feedLlmBatch(*mgr, 2); // both accepted, left IN the queue across the hot update
    const uint64_t usedBefore = mgr->GetMemoryBudgetForTest().Used();
    APSARA_TEST_TRUE(usedBefore >= 2 * static_cast<uint64_t>(kMemLimitTestPayload));

    // Hot update turning the limit off (update + resume → RestartAgentSightLocked): SetLimit(0)
    // disarms admission but must NOT clear Used — the in-flight records still own their bytes.
    o.mAgentsightMemoryLimitEnabled = false;
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Limit());
    APSARA_TEST_EQUAL(usedBefore, mgr->GetMemoryBudgetForTest().Used());

    // Draining the old generation returns the accounting to exactly zero — no leak, no underflow.
    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        mgr->HandleEvent(e);
        e.reset();
    }
    drainProcessQueues(); // destroy pushed items while the fixture's EventPool is still alive
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());

    // New events flow unaccounted while disabled.
    feedLlmBatch(*mgr, 1);
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(e));
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), static_cast<AgentsightLlmRecord*>(e.get())->GetAccountedBytesForTest());
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());
    e.reset();

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitStaleConfigDropReleasesBytes() {
    auto& o = agentsightOptions();
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_stale");

    feedLlmBatch(*mgr, 1);
    APSARA_TEST_TRUE(mgr->GetMemoryBudgetForTest().Used() >= kMemLimitTestPayload);

    // RemoveConfig clears mConfigName and disarms the limit; the queued record becomes stale.
    APSARA_TEST_EQUAL(0, mgr->RemoveConfig("p_mem_stale"));

    std::shared_ptr<CommonEvent> e;
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(e));
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(e)); // stale-config drop branch, no push
    // Bytes are still held until the record itself dies — the release point is the destructor.
    APSARA_TEST_TRUE(mgr->GetMemoryBudgetForTest().Used() >= kMemLimitTestPayload);
    e.reset();
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitHttpsStreamAccounted() {
    auto& o = agentsightOptions();
    o.mAgentsightRawHttpsFallback = true; // otherwise the https callback is never registered
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_https");

    gRead.read_mode = 5;
    gRead.payload_size = kMemLimitTestPayload;
    gRead.payload_events = 3;
    mgr->OnEpollReadable();
    gRead = decltype(gRead){};

    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());
    int accepted = 0;
    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        ++accepted;
        APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_HTTPS_RECORD, e->GetKernelEventType());
        APSARA_TEST_TRUE(static_cast<AgentsightHttpsRecord*>(e.get())->GetAccountedBytesForTest()
                         >= kMemLimitTestPayload);
        e.reset();
    }
    APSARA_TEST_EQUAL(2, accepted);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(0), mgr->GetMemoryBudgetForTest().Used());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitOldLibraryVersionDegrades() {
    // 0.6.1 HAS agentsight_config_load_config and even the runtime_limits block, but not
    // event_channel_max_bytes_mb (0.11.1): its serde would silently ignore that key and return
    // success. The version gate must refuse to push the JSON at all and degrade honestly — while
    // the C++-side budget keeps enforcing.
    g_ut_agentsight_version = "0.6.1";
    auto& o = agentsightOptions();
    o.mAgentsightRuntimeLimitsJson = R"({"ring_buffer_mb":8})";
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_oldver");
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, g_ut_load_config_calls); // gated off despite the symbol being present
    APSARA_TEST_EQUAL(kMemLimitTestLimit, mgr->GetMemoryBudgetForTest().Limit());

    feedLlmBatch(*mgr, 3);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        e.reset();
    }
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMemoryLimitVersionGateBoundary() {
    // Just below the floor: rejected. At the floor: accepted and the JSON is pushed.
    {
        g_ut_agentsight_version = "0.11.0";
        agentsightOptions().mAgentsightRuntimeLimitsJson = R"({"ring_buffer_mb":8})";
        auto mgr = makeManager();
        registerConfig(*mgr, "p_mem_ver_lo");
        APSARA_TEST_EQUAL(0, g_ut_load_config_calls);
        mgr->Destroy();
    }
    {
        g_ut_agentsight_version = "0.11.1";
        auto mgr = makeManager();
        registerConfig(*mgr, "p_mem_ver_hi");
        APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
        mgr->Destroy();
    }
    // Unparsable version strings fail closed (treated as too old), never fail open. The call
    // counter is cumulative across this test's blocks: still exactly the one call from "0.11.1".
    {
        g_ut_agentsight_version = "not-a-version";
        auto mgr = makeManager();
        registerConfig(*mgr, "p_mem_ver_bad");
        APSARA_TEST_EQUAL(1, g_ut_load_config_calls);
        mgr->Destroy();
    }
}

void AgentsightManagerUnittest::TestMemoryLimitMissingVersionSymbolDegrades() {
    // No agentsight_version symbol → capability unverifiable → same honest degradation as an old
    // library: no JSON push, C++ accounting still enforced.
    g_ut_omit_version_symbol = true;
    mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
    auto& o = agentsightOptions();
    o.mAgentsightRuntimeLimitsJson = R"({"ring_buffer_mb":8})";
    o.mAgentsightMemoryLimitEnabled = true;
    o.mAgentsightMemoryLimitBytes = kMemLimitTestLimit;

    auto mgr = makeManager();
    registerConfig(*mgr, "p_mem_nover");
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(0, g_ut_load_config_calls);

    feedLlmBatch(*mgr, 3);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(1), mgr->GetMemoryBudgetForTest().DroppedEvents());

    std::shared_ptr<CommonEvent> e;
    while (mEventQueue->try_dequeue(e)) {
        e.reset();
    }
    mgr->Destroy();
}

UNIT_TEST_CASE(AgentsightManagerUnittest, TestGetPluginType);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateValidation);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateNoSymbols);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRestartStartFailure);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestConfigNewNull);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddRemoveDestroy);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecondAddOrUpdate);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollNoHandle);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollDrain);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateInvalidEventFd);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHandleEventBranches);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestResumeInvalidOptions);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestResumeWithNoRegistration);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSuspend);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestDestroyTwice);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestCmdlineHttpsHttpRulesInvokedOnAddOrUpdate);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestBuiltinCmdlineRulesInjectedWhenCmdlineOmitted);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestUserBlacklistOnlySkipsBuiltinAllowInjection);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRemoveConfigClearsSessionInputCache);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestDestroyClearsSessionInputCache);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSessionInputCacheLruEviction);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRawHttpsFallbackOffByDefault);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRawHttpsFallbackEnabledRegistersCallback);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRawHttpsFallbackMissingSymbolDisablesReporting);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRawHttpsFallbackResetOnRemoveAndDestroy);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventRequestOnlyEmitsOneLog);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventPairCorrelationFields);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventRequestHeaderAllowlist);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventWithoutPipelineContextDrops);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventFiltersSensitiveResponseHeaders);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventDropsUndecodableHeaderNames);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventUnparsableStatusStillEmitsResponse);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestStaleConfigRecordDroppedNotRerouted);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsHostExtractionVariants);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventOmitsEmptyOptionalFields);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestLlmServerAddressFromRequestUrl);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAttachAgentsightContainerTagsFromInfo);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestContainerTagsAttachedForLlmEvent);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestContainerTagsAttachedForHttpsEvent);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestContainerTagsMissDegradesSilently);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitDisabledByDefault);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRuntimeLimitsJsonPassthrough);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRuntimeLimitsJsonWrappedAndUnknownKeys);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRuntimeLimitsJsonBadJsonIgnored);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRuntimeLimitsJsonIndependentOfByteGate);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitDropsOversizedNewEvents);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitRecoversAfterDrain);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitMissingLoadConfigSymbol);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitLoadConfigFailureDegrades);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitHotUpdateKeepsUsedConsistent);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitStaleConfigDropReleasesBytes);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitHttpsStreamAccounted);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitOldLibraryVersionDegrades);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitVersionGateBoundary);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMemoryLimitMissingVersionSymbolDegrades);

UNIT_TEST_MAIN
