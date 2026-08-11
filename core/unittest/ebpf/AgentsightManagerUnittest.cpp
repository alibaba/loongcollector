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

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "common/StringView.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "ebpf/plugin/agentsight/AgentsightManager.h"
#include "ebpf/type/FileEvent.h"
#include "models/LogEvent.h"
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
    /// 3: call the raw HTTPS callback once then 0
    int read_mode = 0;
    int read_step = 0;
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
        mAgentSightAdapter = std::make_shared<AgentSightTestEBPFAdapter>();
        mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
        auto& o = agentsightOptions();
        o.mAgentsightCmdlineWhitelist.clear();
        o.mAgentsightCmdlineBlacklist.clear();
        o.mAgentsightHttps.clear();
        o.mAgentsightHttp.clear();
        o.mAgentsightRawHttpsFallback = false;
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
        auto m = std::make_shared<AgentsightManager>(mProcessCacheManager,
                                                     std::static_pointer_cast<EBPFAdapter>(mAgentSightAdapter),
                                                     *mEventQueue,
                                                     mEventPool.get(),
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
    void TestHttpsEventUnparsableStatusStillEmitsResponse();
    void TestStaleConfigRecordDroppedNotRerouted();
    void TestHttpsHostExtractionVariants();
    void TestHttpsEventOmitsEmptyOptionalFields();
    void TestLlmServerAddressFromRequestUrl();

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
                                           mEventPool.get());
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
                                           mEventPool.get());
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
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventUnparsableStatusStillEmitsResponse);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestStaleConfigRecordDroppedNotRerouted);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsHostExtractionVariants);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHttpsEventOmitsEmptyOptionalFields);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestLlmServerAddressFromRequestUrl);

UNIT_TEST_MAIN
