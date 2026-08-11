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

#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ProcessQueueItem.h"
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
    /// 0: always 0; 1: return 1 once then 0; 2: call LLM callback once then 0; 3: call HTTPS callback once then 0
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
std::vector<int> g_ut_raw_https_values;

void fake_config_set_enable_raw_https(AgentsightConfigHandle* cfg, int enabled) {
    (void)cfg;
    g_ut_raw_https_values.push_back(enabled);
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
static AgentsightHttpsData sUtHttpsData{};

int fake_handle_read(AgentsightHandle* h,
                     agentsight_https_callback_fn https,
                     void* https_user_data,
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
    if (gRead.read_mode == 3) {
        static const char method[] = "POST";
        static const char path[] = "/v1/custom";
        static const char reqHeaders[] = "{\"host\":\"my-svc.example.com:8443\"}";
        static const char reqBody[] = "{\"q\":1}";
        static const char respHeaders[] = "{\"content-type\":\"application/octet-stream\"}";
        static const char respBody[] = "{\"a\":1}";
        std::memset(&sUtHttpsData, 0, sizeof(sUtHttpsData));
        sUtHttpsData.pid = 4321;
        std::memcpy(sUtHttpsData.process_name, "utp", 3U);
        sUtHttpsData.timestamp_ns = 1700000000000000000ULL;
        sUtHttpsData.duration_ns = 1500000000ULL;
        sUtHttpsData.method = method;
        sUtHttpsData.path = path;
        sUtHttpsData.status_code = 502;
        sUtHttpsData.is_sse = 1;
        sUtHttpsData.request_headers = reqHeaders;
        sUtHttpsData.request_headers_len = static_cast<uint32_t>(sizeof(reqHeaders) - 1U);
        sUtHttpsData.request_body = reqBody;
        sUtHttpsData.request_body_len = static_cast<uint32_t>(sizeof(reqBody) - 1U);
        sUtHttpsData.response_headers = respHeaders;
        sUtHttpsData.response_headers_len = static_cast<uint32_t>(sizeof(respHeaders) - 1U);
        sUtHttpsData.response_body = respBody;
        sUtHttpsData.response_body_len = static_cast<uint32_t>(sizeof(respBody) - 1U);
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
    t->config_set_enable_raw_https = fake_config_set_enable_raw_https;
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

std::shared_ptr<AgentsightHttpsRecord> makeHttpsRecord(
    const char* configName,
    const char* requestHeaders = "{\"host\":\"my-svc.example.com:8443\",\"content-type\":\"application/json\"}",
    uint16_t statusCode = 502) {
    static const char method[] = "POST";
    static const char path[] = "/v1/custom";
    static const char reqBody[] = "{\"unparsable\":true}";
    static const char respHeaders[] = "{\"content-type\":\"application/octet-stream\"}";
    static const char respBody[] = "{\"raw\":\"bytes\"}";
    static AgentsightHttpsData d{};
    std::memset(&d, 0, sizeof(d));
    d.pid = 4321;
    std::memcpy(d.process_name, "ut-agent", 8U);
    d.timestamp_ns = 1700000000000000000ULL;
    d.duration_ns = statusCode == 0 ? 0 : 1500000000ULL;
    d.method = method;
    d.path = path;
    d.status_code = statusCode;
    d.is_sse = statusCode == 0 ? 0 : 1;
    d.request_headers = requestHeaders;
    d.request_headers_len = static_cast<uint32_t>(std::strlen(requestHeaders));
    d.request_body = reqBody;
    d.request_body_len = static_cast<uint32_t>(sizeof(reqBody) - 1U);
    d.response_headers = statusCode == 0 ? nullptr : respHeaders;
    d.response_headers_len = statusCode == 0 ? 0 : static_cast<uint32_t>(sizeof(respHeaders) - 1U);
    d.response_body = statusCode == 0 ? nullptr : respBody;
    d.response_body_len = statusCode == 0 ? 0 : static_cast<uint32_t>(sizeof(respBody) - 1U);
    return std::make_shared<AgentsightHttpsRecord>(std::string(configName), d);
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
        g_ut_raw_https_values.clear();
        auto& o = agentsightOptions();
        o.mAgentsightCmdlineWhitelist.clear();
        o.mAgentsightCmdlineBlacklist.clear();
        o.mAgentsightHttps.clear();
        o.mAgentsightHttp.clear();
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
        o.mAgentsightEnableRawHttps = false;
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

    void TestAddOrUpdateValidation();
    void TestAddOrUpdateNoSymbols();
    void TestMissingRawHttpsSetter();
    void TestRestartStartFailure();
    void TestConfigNewNull();
    void TestAddRemoveDestroy();
    void TestSecondAddOrUpdate();
    void TestOnEpollDrain();
    void TestOnEpollDrainHttps();
    void TestOnEpollDrainHttpsGatedOff();
    void TestOnEpollNoHandle();
    void TestAddOrUpdateInvalidEventFd();
    void TestHandleEventBranches();
    void TestHandleHttpsEvent();
    void TestHandleHttpsRequestOnlyAndIpv6();
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

void AgentsightManagerUnittest::TestMissingRawHttpsSetter() {
    auto symbols = makeFullSymbolTable();
    symbols->config_set_enable_raw_https = nullptr;
    mAgentSightAdapter->setAgentSightSymbols(std::move(symbols));

    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    APSARA_TEST_NOT_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));
    APSARA_TEST_EQUAL(0, mgr->RegisteredConfigCount());
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
    auto& opts = agentsightOptions();
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&opts)));
    opts.mAgentsightEnableRawHttps = true;
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 1, nullptr, PluginOptions(&opts)));
    opts.mAgentsightEnableRawHttps = false;
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 1, nullptr, PluginOptions(&opts)));
    APSARA_TEST_EQUAL(1, mgr->RegisteredConfigCount());
    APSARA_TEST_EQUAL(3UL, g_ut_raw_https_values.size());
    APSARA_TEST_EQUAL(0, g_ut_raw_https_values[0]);
    APSARA_TEST_EQUAL(1, g_ut_raw_https_values[1]);
    APSARA_TEST_EQUAL(0, g_ut_raw_https_values[2]);
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

void AgentsightManagerUnittest::TestOnEpollDrainHttps() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    auto& opts = agentsightOptions();
    opts.mAgentsightEnableRawHttps = true; // gate on: HTTPS callback registered
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&opts)));

    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};

    // The HTTPS callback must have enqueued exactly one deep-copied record.
    std::shared_ptr<CommonEvent> evt;
    APSARA_TEST_TRUE(mEventQueue->try_dequeue(evt));
    auto* rec = static_cast<AgentsightHttpsRecord*>(evt.get());
    APSARA_TEST_EQUAL(KernelEventType::AGENTSIGHT_HTTPS_RECORD, evt->GetKernelEventType());
    APSARA_TEST_EQUAL("p1", rec->GetPipelineConfigName());
    APSARA_TEST_EQUAL(4321, rec->mPid);
    APSARA_TEST_EQUAL("utp", rec->mProcessName);
    APSARA_TEST_EQUAL(1700000000000000000ULL, rec->mTimestampNs);
    APSARA_TEST_EQUAL(1500000000ULL, rec->mDurationNs);
    APSARA_TEST_EQUAL("POST", rec->mMethod);
    APSARA_TEST_EQUAL("/v1/custom", rec->mPath);
    APSARA_TEST_EQUAL(502, rec->mStatusCode);
    APSARA_TEST_EQUAL(1, rec->mIsSse);
    APSARA_TEST_EQUAL("{\"host\":\"my-svc.example.com:8443\"}", rec->mRequestHeaders);
    APSARA_TEST_EQUAL("{\"q\":1}", rec->mRequestBody);
    APSARA_TEST_EQUAL("{\"content-type\":\"application/octet-stream\"}", rec->mResponseHeaders);
    APSARA_TEST_EQUAL("{\"a\":1}", rec->mResponseBody);

    mgr->Destroy();
}

void AgentsightManagerUnittest::TestOnEpollDrainHttpsGatedOff() {
    auto mgr = makeManager();
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p1");
    ctx.SetProcessQueueKey(1);
    // Default gate (EnableRawHttps=false): HTTPS callback passed as nullptr, so the fallback
    // event is consumed by coolbpf and dropped -- nothing must be enqueued.
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    gRead.read_mode = 3;
    APSARA_TEST_NOT_EQUAL(0, mgr->OnEpollReadable());
    gRead = decltype(gRead){};

    std::shared_ptr<CommonEvent> evt;
    APSARA_TEST_FALSE(mEventQueue->try_dequeue(evt));

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

void AgentsightManagerUnittest::TestHandleHttpsEvent() {
    const QueueKey queueKey = QueueKeyManager::GetInstance()->GetKey("p_https");
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_https");
    ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(queueKey, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("p_https");

    auto mgr = makeManager();
    // Before AddOrUpdateConfig, mPipelineCtx is null: event is dropped silently.
    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeHttpsRecord("orphan")));

    ctx.SetProcessQueueKey(queueKey);
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeHttpsRecord("p_https")));

    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    if (!item) {
        return;
    }
    APSARA_TEST_EQUAL("p_https", configName);
    const auto& events = item->mEventGroup.GetEvents();
    APSARA_TEST_EQUAL(2UL, events.size());

    const auto* req = events[0].Get<LogEvent>();
    const auto* resp = events[1].Get<LogEvent>();

    // Shared correlation id and request-side fields.
    APSARA_TEST_TRUE(req->HasContent("event.id"));
    APSARA_TEST_EQUAL(req->GetContent("event.id").to_string(), resp->GetContent("event.id").to_string());
    APSARA_TEST_EQUAL("http.request", req->GetContent("event.name"));
    APSARA_TEST_EQUAL("http.response", resp->GetContent("event.name"));
    APSARA_TEST_EQUAL("4321", req->GetContent("pid"));
    APSARA_TEST_EQUAL("ut-agent", req->GetContent("comm"));
    APSARA_TEST_EQUAL("https", req->GetContent("url.scheme"));
    APSARA_TEST_EQUAL("POST", req->GetContent("http.request.method"));
    APSARA_TEST_EQUAL("/v1/custom", req->GetContent("url.path"));
    APSARA_TEST_EQUAL("my-svc.example.com", req->GetContent("server.address"));
    APSARA_TEST_EQUAL("8443", req->GetContent("server.port"));
    APSARA_TEST_TRUE(req->HasContent("http.request.header"));
    APSARA_TEST_EQUAL("{\"unparsable\":true}", req->GetContent("http.request.body.content"));
    APSARA_TEST_EQUAL(std::to_string(std::string("{\"unparsable\":true}").size()),
                      req->GetContent("http.request.body.size"));

    // Response-side fields.
    APSARA_TEST_EQUAL("https", resp->GetContent("url.scheme"));
    APSARA_TEST_EQUAL("502", resp->GetContent("http.response.status_code"));
    APSARA_TEST_EQUAL("1", resp->GetContent("is_sse"));
    APSARA_TEST_TRUE(resp->HasContent("http.response.header"));
    APSARA_TEST_EQUAL("{\"raw\":\"bytes\"}", resp->GetContent("http.response.body.content"));
    APSARA_TEST_EQUAL(std::to_string(std::string("{\"raw\":\"bytes\"}").size()),
                      resp->GetContent("http.response.body.size"));

    // Timestamps: request at timestamp_ns, response at timestamp_ns + duration_ns.
    APSARA_TEST_EQUAL(1700000000, req->GetTimestamp());
    APSARA_TEST_EQUAL(1700000000 + 1, resp->GetTimestamp());
    APSARA_TEST_TRUE(resp->GetTimestampNanosecond().has_value());
    APSARA_TEST_EQUAL(500000000U, resp->GetTimestampNanosecond().value());

    mgr->Destroy();
    ProcessQueueManager::GetInstance()->DeleteQueue(queueKey);
}

void AgentsightManagerUnittest::TestHandleHttpsRequestOnlyAndIpv6() {
    const QueueKey queueKey = QueueKeyManager::GetInstance()->GetKey("p_https_edges");
    CollectionPipelineContext ctx;
    ctx.SetConfigName("p_https_edges");
    ctx.SetProcessQueueKey(queueKey);
    ProcessQueueManager::GetInstance()->CreateOrUpdateCountBoundedQueue(queueKey, 0, ctx);
    ProcessQueueManager::GetInstance()->EnablePop("p_https_edges");

    auto mgr = makeManager();
    APSARA_TEST_EQUAL(0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, asVariant()));

    APSARA_TEST_EQUAL(
        0,
        mgr->HandleEvent(makeHttpsRecord("p_https_edges", "{\":authority\":\"[2001:db8::1]:9443\"}", 200)));
    std::unique_ptr<ProcessQueueItem> item;
    std::string configName;
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_TRUE(item != nullptr);
    if (item) {
        const auto& events = item->mEventGroup.GetEvents();
        APSARA_TEST_EQUAL(2UL, events.size());
        const auto* request = events[0].Get<LogEvent>();
        APSARA_TEST_EQUAL("2001:db8::1", request->GetContent("server.address"));
        APSARA_TEST_EQUAL("9443", request->GetContent("server.port"));
    }

    APSARA_TEST_EQUAL(0, mgr->HandleEvent(makeHttpsRecord("p_https_edges", "{\"host\":\"request-only\"}", 0)));
    item.reset();
    APSARA_TEST_TRUE(ProcessQueueManager::GetInstance()->PopItem(0, item, configName));
    APSARA_TEST_TRUE(item != nullptr);
    if (item) {
        const auto& events = item->mEventGroup.GetEvents();
        APSARA_TEST_EQUAL(1UL, events.size());
        const auto* request = events[0].Get<LogEvent>();
        APSARA_TEST_EQUAL("http.request", request->GetContent("event.name"));
        APSARA_TEST_EQUAL("request-only", request->GetContent("server.address"));
    }

    mgr->Destroy();
    ProcessQueueManager::GetInstance()->DeleteQueue(queueKey);
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

UNIT_TEST_CASE(AgentsightManagerUnittest, TestGetPluginType);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateValidation);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateNoSymbols);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestMissingRawHttpsSetter);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestRestartStartFailure);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestConfigNewNull);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddRemoveDestroy);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestSecondAddOrUpdate);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollNoHandle);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollDrain);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollDrainHttps);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestOnEpollDrainHttpsGatedOff);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestAddOrUpdateInvalidEventFd);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHandleEventBranches);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHandleHttpsEvent);
UNIT_TEST_CASE(AgentsightManagerUnittest, TestHandleHttpsRequestOnlyAndIpv6);
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

UNIT_TEST_MAIN
