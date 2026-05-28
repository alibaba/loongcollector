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

#include <gtest/gtest.h>

#include <memory>

#include "ebpf/plugin/process_security/ProcessSecurityManager.h"
#include "ebpf/type/ProcessEvent.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ManagerUnittestBase.h"

using namespace logtail;
using namespace logtail::ebpf;

class ProcessSecurityManagerUnittest : public SecurityManagerUnittestBase {
public:
    void TestProcessSecurityManagerEventHandling();
    void TestProcessSecurityManagerAggregation();
    void TestProcessSecurityManagerErrorHandling();

protected:
    std::shared_ptr<AbstractManager> createManagerInstance() override {
        return std::make_shared<ProcessSecurityManager>(
            mProcessCacheManager, mMockEBPFAdapter, *mEventQueue, mEventPool.get());
    }
};

void ProcessSecurityManagerUnittest::TestProcessSecurityManagerEventHandling() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 测试EXECVE事件
    auto execveEvent = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_EXECVE_EVENT, 799);
    APSARA_TEST_EQUAL(manager->HandleEvent(execveEvent), 0);

    // 测试EXIT事件
    auto exitEvent = std::make_shared<ProcessExitEvent>(1234, 5678, KernelEventType::PROCESS_EXIT_EVENT, 789, 0, 1234);
    APSARA_TEST_EQUAL(manager->HandleEvent(exitEvent), 0);

    // 测试CLONE事件
    auto cloneEvent = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_CLONE_EVENT, 789);
    APSARA_TEST_EQUAL(manager->HandleEvent(cloneEvent), 0);

    manager->Destroy();
}

void ProcessSecurityManagerUnittest::TestProcessSecurityManagerErrorHandling() {
    auto manager = createAndInitManagerInstance();

    // 测试 null 事件处理
    APSARA_TEST_NOT_EQUAL(manager->HandleEvent(nullptr), 0);

    auto validEvent = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_EXECVE_EVENT, 0);
    APSARA_TEST_EQUAL(manager->HandleEvent(validEvent), 0);

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);
    APSARA_TEST_EQUAL(manager->HandleEvent(validEvent), 0);

    manager->Destroy();
}

void ProcessSecurityManagerUnittest::TestProcessSecurityManagerAggregation() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 创建多个相关的进程事件
    std::vector<std::shared_ptr<ProcessEvent>> events;

    // 同一连接的多个事件
    for (int i = 0; i < 3; ++i) {
        events.push_back(
            std::make_shared<ProcessEvent>(1234, // pid
                                           5678, // ktime
                                           KernelEventType::PROCESS_CLONE_EVENT, // type
                                           std::chrono::system_clock::now().time_since_epoch().count() + i // timestamp
                                           ));
    }

    // 处理所有事件
    for (const auto& event : events) {
        APSARA_TEST_EQUAL(manager->HandleEvent(event), 0);
    }

    // add cache
    auto execveEvent = std::make_shared<ProcessCacheValue>();
    data_event_id key{1234, 5678};
    execveEvent->mPPid = 2345;
    execveEvent->mPKtime = 6789;

    // 测试缓存更新
    mProcessCacheManager->mProcessCache.AddCache(key, execveEvent);
    auto pExecveEvent = std::make_shared<ProcessCacheValue>();
    data_event_id pkey{2345, 6789};
    mProcessCacheManager->mProcessCache.AddCache(pkey, pExecveEvent);

    // 触发聚合
    APSARA_TEST_EQUAL(0, manager->SendEvents());

    manager->Destroy();
}

UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestDifferentConfigNamesReplacement);
UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestSameConfigNameUpdate);
UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestBasicConfigUpdate);
UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestProcessSecurityManagerEventHandling);
UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestProcessSecurityManagerAggregation);
UNIT_TEST_CASE(ProcessSecurityManagerUnittest, TestProcessSecurityManagerErrorHandling);

UNIT_TEST_MAIN
