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

#include "ebpf/plugin/ProcessCacheManager.h"
#include "ebpf/plugin/network_security/NetworkSecurityManager.h"
#include "ebpf/type/NetworkEvent.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/ManagerUnittestBase.h"

using namespace logtail;
using namespace logtail::ebpf;

class NetworkSecurityManagerUnittest : public SecurityManagerUnittestBase {
public:
    void TestNetworkSecurityManagerEventHandling();
    void TestNetworkSecurityManagerAggregation();
    void TestNetworkSecurityManagerErrorHandling();

protected:
    std::shared_ptr<AbstractManager> createManagerInstance() override {
        return std::make_shared<NetworkSecurityManager>(
            mProcessCacheManager, mMockEBPFAdapter, *mEventQueue, mEventPool.get());
    }
};

void NetworkSecurityManagerUnittest::TestNetworkSecurityManagerEventHandling() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 测试TCP连接事件
    auto connectEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_CONNECT_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         6, // protocol (TCP)
                                         2, // family (AF_INET)
                                         0x0100007F, // saddr (127.0.0.1)
                                         0x0101A8C0, // daddr (192.168.1.1)
                                         12345, // sport
                                         80, // dport
                                         4026531992 // net_ns
        );
    APSARA_TEST_EQUAL(manager->HandleEvent(connectEvent), 0);

    // 测试TCP发送数据事件
    auto sendEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_SENDMSG_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         6, // protocol (TCP)
                                         2, // family (AF_INET)
                                         0x0100007F, // saddr (127.0.0.1)
                                         0x0101A8C0, // daddr (192.168.1.1)
                                         12345, // sport
                                         80, // dport
                                         4026531992 // net_ns
        );
    APSARA_TEST_EQUAL(manager->HandleEvent(sendEvent), 0);

    // 测试TCP关闭事件
    auto closeEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_CLOSE_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         6, // protocol (TCP)
                                         2, // family (AF_INET)
                                         0x0100007F, // saddr (127.0.0.1)
                                         0x0101A8C0, // daddr (192.168.1.1)
                                         12345, // sport
                                         80, // dport
                                         4026531992 // net_ns
        );
    APSARA_TEST_EQUAL(manager->HandleEvent(closeEvent), 0);

    manager->Destroy();
}

void NetworkSecurityManagerUnittest::TestNetworkSecurityManagerErrorHandling() {
    auto manager = createAndInitManagerInstance();

    // 测试 null 事件处理
    APSARA_TEST_NOT_EQUAL(manager->HandleEvent(nullptr), 0);

    auto validEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_CONNECT_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         6, // protocol (TCP)
                                         2, // family (AF_INET)
                                         0x0100007F, // saddr (127.0.0.1)
                                         0x0101A8C0, // daddr (192.168.1.1)
                                         12345, // sport
                                         80, // dport
                                         4026531992 // net_ns
        );

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    APSARA_TEST_EQUAL(manager->HandleEvent(validEvent), 0);

    // 测试无效的网络地址
    auto invalidAddrEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_CONNECT_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         6, // protocol (TCP)
                                         0, // family (invalid)
                                         0, // saddr
                                         0, // daddr
                                         0, // sport
                                         0, // dport
                                         0 // net_ns
        );
    APSARA_TEST_EQUAL(manager->HandleEvent(invalidAddrEvent), 0);

    // 测试不同协议类型
    auto udpEvent
        = std::make_shared<NetworkEvent>(1234, // pid
                                         5678, // ktime
                                         KernelEventType::TCP_SENDMSG_EVENT, // type
                                         std::chrono::system_clock::now().time_since_epoch().count(), // timestamp
                                         17, // protocol (UDP)
                                         2, // family (AF_INET)
                                         0x0100007F, // saddr (127.0.0.1)
                                         0x0101A8C0, // daddr (192.168.1.1)
                                         12345, // sport
                                         80, // dport
                                         4026531992 // net_ns
        );
    APSARA_TEST_EQUAL(manager->HandleEvent(udpEvent), 0);

    manager->Destroy();
}

void NetworkSecurityManagerUnittest::TestNetworkSecurityManagerAggregation() {
    auto manager = createAndInitManagerInstance();

    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    SecurityOptions options;
    APSARA_TEST_EQUAL(
        manager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)),
        0);

    // 创建多个相关的网络事件
    std::vector<std::shared_ptr<NetworkEvent>> events;

    // 同一连接的多个事件
    for (int i = 0; i < 3; ++i) {
        events.push_back(
            std::make_shared<NetworkEvent>(1234, // pid
                                           5678, // ktime
                                           KernelEventType::TCP_SENDMSG_EVENT, // type
                                           std::chrono::system_clock::now().time_since_epoch().count() + i, // timestamp
                                           6, // protocol (TCP)
                                           2, // family (AF_INET)
                                           0x0100007F, // saddr (127.0.0.1)
                                           0x0101A8C0, // daddr (192.168.1.1)
                                           12345, // sport
                                           80, // dport
                                           4026531992 // net_ns
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

UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestNetworkSecurityManagerEventHandling);
UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestNetworkSecurityManagerAggregation);
UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestNetworkSecurityManagerErrorHandling);
UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestDifferentConfigNamesReplacement);
UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestSameConfigNameUpdate);
UNIT_TEST_CASE(NetworkSecurityManagerUnittest, TestBasicConfigUpdate);

UNIT_TEST_MAIN
