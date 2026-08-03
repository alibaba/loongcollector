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
#include <variant>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
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
    /// 0: always 0; 1: return 1 once then 0; 2: LLM once; 3: security once via read_v2
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
int g_ut_security_enable_calls = 0;
int g_ut_security_enabled = 0;
std::string g_ut_enforcer_socket;
int g_ut_read_v2_calls = 0;

void fake_config_set_enable_security_audit(AgentsightConfigHandle* cfg, int enabled) {
    (void)cfg;
    ++g_ut_security_enable_calls;
    g_ut_security_enabled = enabled;
}

void fake_config_set_enforcer_socket(AgentsightConfigHandle* cfg, const char* path) {
    (void)cfg;
    g_ut_enforcer_socket = path ? path : "";
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

int fake_handle_read(AgentsightHandle* h,
                     agentsight_https_callback_fn,
                     void*,
                     agentsight_llm_callback_fn llm,
                     void* user_data,
                     int flags) {
    (void)h;
    (void)flags;
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
    return 0;
}

int fake_handle_read_v2(AgentsightHandle* h,
                        agentsight_https_callback_fn http,
                        void* http_user_data,
                        agentsight_llm_callback_fn llm,
                        void* llm_user_data,
                        agentsight_event_callback_fn event,
                        void* event_user_data,
                        int flags) {
    ++g_ut_read_v2_calls;
    if (gRead.read_mode != 3) {
        return fake_handle_read(h, http, http_user_data, llm, llm_user_data, flags);
    }
    static const char payload[]
        = R"({"event_id":"00000000-0000-0000-0000-000000000001","occurred_at_ns":7,"observed_at_ns":8,"identity":{"agent_id":"agent-1","agent_name":"claude","session_id":"session-1","pid":42},"event_type":"policy_decision","event":{"policy_id":"credential-exfiltration","policy_revision":3,"mode":"audit","risk_score":85,"reason":"test"}})";
    AgentsightEvent data{};
    data.event_type = static_cast<AgentsightEventType>(3);
    data.schema_version = 1;
    data.timestamp_ns = 7;
    data.payload_json = payload;
    data.payload_json_len = sizeof(payload) - 1U;
    if (event) {
        event(&data, event_user_data);
    }
    gRead.read_mode = 0;
    return 1;
}

std::unique_ptr<AgentSightSymbolTable> makeFullSymbolTable() {
    auto t = std::make_unique<AgentSightSymbolTable>();
    t->last_error = fake_last_error;
    t->config_new = fake_config_new;
    t->config_free = fake_config_free;
    t->config_set_verbose = fake_config_set_verbose;
    t->config_set_log_path = fake_config_set_log_path;
    t->config_set_enable_security_audit = fake_config_set_enable_security_audit;
    t->config_set_enforcer_socket = fake_config_set_enforcer_socket;
    t->config_add_cmdline_rule = fake_config_add_cmdline_rule;
    t->config_add_https = fake_config_add_https;
    t->config_add_http = fake_config_add_http;
    t->handle_new = fake_handle_new;
    t->handle_free = fake_handle_free;
    t->handle_start = fake_handle_start;
    t->handle_stop = fake_handle_stop;
    t->handle_get_eventfd = fake_get_eventfd;
    t->handle_read = fake_handle_read;
    t->handle_read_v2 = fake_handle_read_v2;
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
        mAgentSightAdapter = std::make_shared<AgentSightTestEBPFAdapter>();
        mAgentSightAdapter->setAgentSightSymbols(makeFullSymbolTable());
        gRead = decltype(gRead){};
        gRead.start_ret = 0;
        g_config_new_null = false;
        g_ut_cmdline_allow_calls = 0;
        g_ut_cmdline_deny_calls = 0;
        g_ut_https_calls = 0;
        g_ut_http_calls = 0;
        g_ut_security_enable_calls = 0;
        g_ut_security_enabled = 0;
        g_ut_enforcer_socket.clear();
        g_ut_read_v2_calls = 0;
        auto& o = agentsightOptions();
        o.mAgentsightCmdlineWhitelist.clear();
        o.mAgentsightCmdlineBlacklist.clear();
        o.mAgentsightHttps.clear();
        o.mAgentsightHttp.clear();
        o.mAgentsightSecurityAuditEnabled = false;
        o.mAgentsightEnforcerSocket = "/run/agentsight/enforcer.sock";
    }

    void TearDown() override {
        for (const auto key : mProcessQueueKeys) {
            ProcessQueueManager::GetInstance()->DeleteQueue(key);
        }
        mProcessQueueKeys.clear();
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

    void registerConfigWithQueue(AgentsightManager& mgr, const char* configName) {
        const QueueKey key = QueueKeyManager::GetInstance()->GetKey(configName);
        CollectionPipelineContext ctx;
        ctx.SetConfigName(configName);
        ctx.SetProcessQueueKey(key);
        APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(key, 0, ctx));
        ProcessQueueManager::GetInstance()->EnablePop(configName);
        APSARA_TEST_EQUAL(0, mgr.AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
        mProcessQueueKeys.push_back(key);
    }

    void populateSessionInputCache(AgentsightManager& mgr, const char* configName, const char* const* sessionIds) {
        for (const char* const* it = sessionIds; *it != nullptr; ++it) {
            APSARA_TEST_EQUAL(0, mgr.HandleEvent(makeMinimalLlmRecord(configName, *it)));
        }
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
    void TestSecurityAuditUsesReadV2AndEnqueuesSecurityRecord();
    void TestSecurityAuditFallsBackWhenV2SymbolsAreMissing();
    void TestSecurityEventProducesSearchableLog();
    void TestLlmResponseProducesCrossLayerCorrelationIdentity();
    void TestSecurityEventOmitsInvalidCorrelationIdentity();
    void TestMalformedSecurityEventPreservesEnvelope();

protected:
    std::shared_ptr<AgentSightTestEBPFAdapter> mAgentSightAdapter;
    std::vector<QueueKey> mProcessQueueKeys;
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
    auto orphanSecurity = std::make_shared<AgentsightSecurityRecord>("orphan", 1U, 1U, "{}");
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(orphanSecurity));

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

void AgentsightManagerUnittest::TestSecurityAuditUsesReadV2AndEnqueuesSecurityRecord() {
    auto& options = agentsightOptions();
    options.mAgentsightSecurityAuditEnabled = true;
    options.mAgentsightEnforcerSocket = "/tmp/enforcer.sock";
    auto mgr = makeManager();
    registerConfig(*mgr, "security-pipeline");

    APSARA_TEST_EQUAL(1, g_ut_security_enabled);
    APSARA_TEST_EQUAL("/tmp/enforcer.sock", g_ut_enforcer_socket);
    gRead.read_mode = 3;
    APSARA_TEST_EQUAL(1, mgr->OnEpollReadable());
    APSARA_TEST_TRUE(g_ut_read_v2_calls >= 1);

    std::shared_ptr<CommonEvent> event;
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(event));
    APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_SECURITY_RECORD, event->GetKernelEventType());
    auto* security = static_cast<AgentsightSecurityRecord*>(event.get());
    APSARA_TEST_EQUAL(1, security->mSchemaVersion);
    APSARA_TEST_EQUAL(7, security->mTimestampNs);
    APSARA_TEST_TRUE(security->mPayloadJson.find("credential-exfiltration") != std::string::npos);
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestSecurityAuditFallsBackWhenV2SymbolsAreMissing() {
    auto symbols = makeFullSymbolTable();
    symbols->handle_read_v2 = nullptr;
    mAgentSightAdapter->setAgentSightSymbols(std::move(symbols));
    auto& options = agentsightOptions();
    options.mAgentsightSecurityAuditEnabled = true;
    g_ut_security_enabled = 1;
    auto mgr = makeManager();
    registerConfig(*mgr, "legacy-pipeline");

    APSARA_TEST_EQUAL(1, g_ut_security_enable_calls);
    APSARA_TEST_EQUAL(0, g_ut_security_enabled);
    gRead.read_mode = 2;
    APSARA_TEST_EQUAL(1, mgr->OnEpollReadable());
    APSARA_TEST_EQUAL(0, g_ut_read_v2_calls);
    mgr->Destroy();
}

void AgentsightManagerUnittest::TestSecurityEventProducesSearchableLog() {
    static const std::string kConfigName = "security-log-pipeline";
    static const std::string kPayload
        = R"({"event_id":"00000000-0000-0000-0000-000000000001","observed_at_ns":8,"identity":{"binding_id":"10000000-0000-0000-0000-000000000001","agent_id":"agent-1","agent_name":"claude","session_id":"session-1","conversation_id":"conversation-1","tool_call_id":"tool-call-1","pid":2147483648,"process_start_time":101,"ppid":4294967295,"cgroup_id":9001},"event_type":"policy_decision","event":{"policy_id":"credential-exfiltration","policy_revision":3,"mode":"audit","risk_score":85,"large_counter":9223372036854775808,"blocked":true,"confidence":0.75,"details":{"source":"unit-test"}}})";

    auto mgr = makeManager();
    registerConfigWithQueue(*mgr, kConfigName.c_str());

    auto record = std::make_shared<AgentsightSecurityRecord>(kConfigName, 7U, 1U, kPayload);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(record));

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL(kConfigName, configName);
    APSARA_TEST_EQUAL(1U, item->mEventGroup.GetEvents().size());
    const auto& log = item->mEventGroup.GetEvents().at(0).Cast<LogEvent>();
    APSARA_TEST_EQUAL("7", log.GetContent("time_unix_nano").to_string());
    APSARA_TEST_EQUAL("agentsight.security.policy_decision", log.GetContent("event.name").to_string());
    APSARA_TEST_EQUAL("event", log.GetContent("event.kind").to_string());
    APSARA_TEST_EQUAL("security", log.GetContent("event.category").to_string());
    APSARA_TEST_EQUAL("policy_decision", log.GetContent("event.type").to_string());
    APSARA_TEST_EQUAL("00000000-0000-0000-0000-000000000001", log.GetContent("event.id").to_string());
    APSARA_TEST_EQUAL("8", log.GetContent("observed_time_unix_nano").to_string());
    APSARA_TEST_EQUAL("1", log.GetContent("agentsight.schema_version").to_string());
    APSARA_TEST_EQUAL("agent-1", log.GetContent("agent.id").to_string());
    APSARA_TEST_EQUAL("claude", log.GetContent("gen_ai.agent.type").to_string());
    APSARA_TEST_EQUAL("session-1", log.GetContent("gen_ai.session.id").to_string());
    APSARA_TEST_EQUAL("conversation-1", log.GetContent("gen_ai.turn.id").to_string());
    APSARA_TEST_EQUAL("conversation-1", log.GetContent("gen_ai.conversation.id").to_string());
    APSARA_TEST_EQUAL("tool-call-1", log.GetContent("gen_ai.tool.call.id").to_string());
    APSARA_TEST_EQUAL("2147483648", log.GetContent("process.pid").to_string());
    APSARA_TEST_EQUAL("101", log.GetContent("process.start_time").to_string());
    APSARA_TEST_EQUAL("4294967295", log.GetContent("process.parent.pid").to_string());
    APSARA_TEST_EQUAL("9001", log.GetContent("container.cgroup.id").to_string());
    APSARA_TEST_EQUAL("10000000-0000-0000-0000-000000000001", log.GetContent("agentsight.binding.id").to_string());
    APSARA_TEST_EQUAL("agent-1", log.GetContent("agentsight.identity.agent_id").to_string());
    APSARA_TEST_EQUAL("credential-exfiltration", log.GetContent("security.policy_id").to_string());
    APSARA_TEST_EQUAL("3", log.GetContent("security.policy_revision").to_string());
    APSARA_TEST_EQUAL("audit", log.GetContent("security.mode").to_string());
    APSARA_TEST_EQUAL("85", log.GetContent("security.risk_score").to_string());
    APSARA_TEST_EQUAL("9223372036854775808", log.GetContent("security.large_counter").to_string());
    APSARA_TEST_EQUAL("true", log.GetContent("security.blocked").to_string());
    APSARA_TEST_EQUAL("0.750000", log.GetContent("security.confidence").to_string());
    APSARA_TEST_EQUAL("unit-test", log.GetContent("security.details.source").to_string());
    APSARA_TEST_EQUAL(kPayload, log.GetContent("event.original").to_string());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestLlmResponseProducesCrossLayerCorrelationIdentity() {
    static const std::string kConfigName = "llm-correlation-pipeline";
    static const char kResponse[]
        = R"([{"role":"assistant","parts":[{"type":"tool_call","id":"call-hermes-1","name":"shell"}]}])";
    static AgentsightLLMData data{};
    std::memset(&data, 0, sizeof(data));
    data.pid = 4242;
    data.session_id = "session-1";
    data.conversation_id = "conversation-1";
    data.response_id = "response-1";
    data.agent_name = "hermes";
    data.response_messages = kResponse;
    data.response_messages_len = sizeof(kResponse) - 1U;
    data.timestamp_ns = 1U;

    auto mgr = makeManager();
    agentsightOptions().mAgentsightEventStreamFormat = true;
    registerConfigWithQueue(*mgr, kConfigName.c_str());
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(std::make_shared<AgentsightLlmRecord>(kConfigName, data)));

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL(2U, item->mEventGroup.GetEvents().size());
    const auto& request = item->mEventGroup.GetEvents().at(0).Cast<LogEvent>();
    const auto& response = item->mEventGroup.GetEvents().at(1).Cast<LogEvent>();
    APSARA_TEST_TRUE(response.GetContent("agent.id").empty());
    APSARA_TEST_EQUAL("hermes", response.GetContent("gen_ai.agent.type").to_string());
    APSARA_TEST_EQUAL("4242", response.GetContent("process.pid").to_string());
    APSARA_TEST_EQUAL("call-hermes-1", response.GetContent("gen_ai.tool.call.id").to_string());
    APSARA_TEST_TRUE(request.GetContent("gen_ai.tool.call.id").empty());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestSecurityEventOmitsInvalidCorrelationIdentity() {
    static const std::string kConfigName = "security-invalid-identity-pipeline";
    static const std::string kPayload
        = R"({"identity":{"binding_id":1,"conversation_id":null,"tool_call_id":{},"pid":-1,"process_start_time":-1,"ppid":4294967296,"cgroup_id":-1},"event_type":"policy_decision","event":{}})";

    auto mgr = makeManager();
    registerConfigWithQueue(*mgr, kConfigName.c_str());

    auto record = std::make_shared<AgentsightSecurityRecord>(kConfigName, 11U, 1U, kPayload);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(record));

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    const auto& log = item->mEventGroup.GetEvents().at(0).Cast<LogEvent>();
    APSARA_TEST_TRUE(log.GetContent("agentsight.binding.id").empty());
    APSARA_TEST_TRUE(log.GetContent("gen_ai.conversation.id").empty());
    APSARA_TEST_TRUE(log.GetContent("gen_ai.tool.call.id").empty());
    APSARA_TEST_TRUE(log.GetContent("process.pid").empty());
    APSARA_TEST_TRUE(log.GetContent("process.start_time").empty());
    APSARA_TEST_TRUE(log.GetContent("process.parent.pid").empty());
    APSARA_TEST_TRUE(log.GetContent("container.cgroup.id").empty());
    APSARA_TEST_EQUAL(kPayload, log.GetContent("event.original").to_string());

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestMalformedSecurityEventPreservesEnvelope() {
    static const std::string kConfigName = "security-malformed-pipeline";
    static const std::string kPayload = "{not-json";

    auto mgr = makeManager();
    registerConfigWithQueue(*mgr, kConfigName.c_str());

    auto record = std::make_shared<AgentsightSecurityRecord>(kConfigName, 9U, 2U, kPayload);
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(record));

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_EQUAL(1U, item->mEventGroup.GetEvents().size());
    const auto& log = item->mEventGroup.GetEvents().at(0).Cast<LogEvent>();
    APSARA_TEST_EQUAL("9", log.GetContent("time_unix_nano").to_string());
    APSARA_TEST_EQUAL("agentsight.security", log.GetContent("event.name").to_string());
    APSARA_TEST_EQUAL("event", log.GetContent("event.kind").to_string());
    APSARA_TEST_EQUAL("security", log.GetContent("event.category").to_string());
    APSARA_TEST_EQUAL("2", log.GetContent("agentsight.schema_version").to_string());
    APSARA_TEST_EQUAL(kPayload, log.GetContent("event.original").to_string());
    APSARA_TEST_TRUE(log.GetContent("event.id").empty());

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
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecurityAuditUsesReadV2AndEnqueuesSecurityRecord);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecurityAuditFallsBackWhenV2SymbolsAreMissing);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecurityEventProducesSearchableLog);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestLlmResponseProducesCrossLayerCorrelationIdentity);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecurityEventOmitsInvalidCorrelationIdentity);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMalformedSecurityEventPreservesEnvelope);

UNIT_TEST_MAIN
