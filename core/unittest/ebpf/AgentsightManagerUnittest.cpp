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
#include "ebpf/Config.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/agentsight/AgentsightEvents.h"
#include "ebpf/plugin/agentsight/AgentsightManager.h"
#include "ebpf/type/FileEvent.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ManagerUnittestBase.h"

using namespace logtail;
using namespace logtail::ebpf;

namespace {

AgentsightConfigHandle* gFakeCfg = reinterpret_cast<AgentsightConfigHandle*>(0x10U);
AgentsightHandle* gFakeHandle = reinterpret_cast<AgentsightHandle*>(0x20U);

struct FakeReadControl {
    int start_ret = 0;
    /// 0: always 0; 1: return 1 once then 0; 2: call LLM callback once then 0
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

std::unique_ptr<AgentSightSymbolTable> makeFullSymbolTable() {
    auto t = std::make_unique<AgentSightSymbolTable>();
    t->last_error = fake_last_error;
    t->config_new = fake_config_new;
    t->config_free = fake_config_free;
    t->config_set_verbose = fake_config_set_verbose;
    t->config_set_log_path = fake_config_set_log_path;
    t->handle_new = fake_handle_new;
    t->handle_free = fake_handle_free;
    t->handle_start = fake_handle_start;
    t->handle_stop = fake_handle_stop;
    t->handle_get_eventfd = fake_get_eventfd;
    t->handle_read = fake_handle_read;
    return t;
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

    std::shared_ptr<AgentsightManager> makeManager() {
        auto m = AgentsightManager::Create(mProcessCacheManager,
                                           std::static_pointer_cast<EBPFAdapter>(mAgentSightAdapter),
                                           *mEventQueue,
                                           mEventPool.get());
        APSARA_TEST_EQUAL(0, m->Init());
        return m;
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
    APSARA_TEST_NOT_EQUAL(
        0, mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&o)));

    APSARA_TEST_NOT_EQUAL(
        0,
        mgr->AddOrUpdateConfig(
            nullptr, 0, nullptr, PluginOptions(&agentsightOptions())));

    {
        static SecurityOptions wrong;
        wrong = agentsightOptions();
        wrong.mProbeType = SecurityProbeType::FILE;
        APSARA_TEST_NOT_EQUAL(
            0,
            mgr->AddOrUpdateConfig(&ctx, 0, nullptr, PluginOptions(&wrong)));
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

UNIT_TEST_MAIN
