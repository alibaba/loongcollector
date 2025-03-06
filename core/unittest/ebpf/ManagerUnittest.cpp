// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <gtest/gtest.h>

#include <memory>
#include <thread>

#include "common/TimeUtil.h"
#include "common/timer/Timer.h"
#include "ebpf/SourceManager.h"
#include "ebpf/plugin/file_security/FileSecurityManager.h"
#include "ebpf/plugin/network_security/NetworkSecurityManager.h"
#include "ebpf/plugin/process_security/ProcessSecurityManager.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/NetworkEvent.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "ebpf/type/ProcessEvent.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class ManagerUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        mSourceManager = std::make_shared<SourceManager>();
        mProcessCacheManager = std::make_shared<ProcessCacheManager>(mSourceManager, "test_host", "/", mEventQueue);
        mTimer = std::make_shared<Timer>();
    }

    void TearDown() override { mTimer->Stop(); }

    // 测试ProcessSecurityManager的基本功能
    void TestProcessSecurityManagerBasic();
    // 测试ProcessSecurityManager的事件处理
    void TestProcessSecurityManagerEventHandling();
    // 测试ProcessSecurityManager的聚合功能
    void TestProcessSecurityManagerAggregation();

    // 测试FileSecurityManager的基本功能
    void TestFileSecurityManagerBasic();
    // 测试FileSecurityManager的事件处理
    void TestFileSecurityManagerEventHandling();
    // 测试FileSecurityManager的聚合功能
    void TestFileSecurityManagerAggregation();

    // 测试NetworkSecurityManager的基本功能
    void TestNetworkSecurityManagerBasic();
    // 测试NetworkSecurityManager的事件处理
    void TestNetworkSecurityManagerEventHandling();
    // 测试NetworkSecurityManager的聚合功能
    void TestNetworkSecurityManagerAggregation();

    // 测试Manager的并发处理
    void TestManagerConcurrency();
    // 测试Manager的资源管理
    void TestManagerResourceManagement();
    // 测试Manager的错误处理
    void TestManagerErrorHandling();

protected:
    std::shared_ptr<SourceManager> mSourceManager;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
    std::shared_ptr<Timer> mTimer;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
};

void ManagerUnittest::TestProcessSecurityManagerBasic() {
    auto manager = std::make_shared<ProcessSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);

    // 测试初始化
    SecurityOptions options;
    APSARA_TEST_EQUAL(manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试暂停
    APSARA_TEST_EQUAL(manager->Suspend(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());

    // 测试恢复
    APSARA_TEST_EQUAL(manager->Resume(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试销毁
    APSARA_TEST_EQUAL(manager->Destroy(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());
}

void ManagerUnittest::TestProcessSecurityManagerEventHandling() {
    auto manager = std::make_shared<ProcessSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 创建并处理Execve事件
    auto execveEvent = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_EXECVE_EVENT, 799);
    APSARA_TEST_EQUAL(manager->HandleEvent(execveEvent), 0);

    // 创建并处理Exit事件
    auto exitEvent = std::make_shared<ProcessExitEvent>(1234, 5678, KernelEventType::PROCESS_EXIT_EVENT, 789, 0, 1234);
    APSARA_TEST_EQUAL(manager->HandleEvent(exitEvent), 0);

    // 创建并处理Clone事件
    auto cloneEvent = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_CLONE_EVENT, 789);
    APSARA_TEST_EQUAL(manager->HandleEvent(cloneEvent), 0);

    manager->Destroy();
}

void ManagerUnittest::TestFileSecurityManagerBasic() {
    auto manager = std::make_shared<FileSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);

    // 测试初始化
    SecurityOptions options;
    APSARA_TEST_EQUAL(manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试暂停
    APSARA_TEST_EQUAL(manager->Suspend(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());

    // 测试恢复
    APSARA_TEST_EQUAL(manager->Resume(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试销毁
    APSARA_TEST_EQUAL(manager->Destroy(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());
}

void ManagerUnittest::TestFileSecurityManagerEventHandling() {
    auto manager = std::make_shared<FileSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 测试文件权限事件
    auto permissionEvent = std::make_shared<FileEvent>(1234,
                                                       5678,
                                                       KernelEventType::FILE_PERMISSION_EVENT,
                                                       std::chrono::system_clock::now().time_since_epoch().count(),
                                                       "/test/file.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(permissionEvent), 0);

    // 测试文件mmap事件
    auto mmapEvent = std::make_shared<FileEvent>(1234,
                                                 5678,
                                                 KernelEventType::FILE_MMAP,
                                                 std::chrono::system_clock::now().time_since_epoch().count(),
                                                 "/test/mmap.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(mmapEvent), 0);

    // 测试文件truncate事件
    auto truncateEvent = std::make_shared<FileEvent>(1234,
                                                     5678,
                                                     KernelEventType::FILE_PATH_TRUNCATE,
                                                     std::chrono::system_clock::now().time_since_epoch().count(),
                                                     "/test/truncate.txt");
    APSARA_TEST_EQUAL(manager->HandleEvent(truncateEvent), 0);

    manager->Destroy();
}

void ManagerUnittest::TestManagerConcurrency() {
    auto processManager
        = std::make_shared<ProcessSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    auto fileManager = std::make_shared<FileSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);

    SecurityOptions options;
    processManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    fileManager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 创建多个线程同时处理事件
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&processManager, i]() {
            auto event = std::make_shared<ProcessEvent>(1000 + i, 5000 + i, KernelEventType::PROCESS_EXECVE_EVENT, i);
            processManager->HandleEvent(event);
        });

        threads.emplace_back([&fileManager, i]() {
            auto event = std::make_shared<FileEvent>(2000 + i,
                                                     6000 + i,
                                                     KernelEventType::FILE_PERMISSION_EVENT,
                                                     std::chrono::system_clock::now().time_since_epoch().count(),
                                                     "/test/file" + std::to_string(i) + ".txt");
            fileManager->HandleEvent(event);
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    processManager->Destroy();
    fileManager->Destroy();
}

void ManagerUnittest::TestManagerErrorHandling() {
    auto manager = std::make_shared<ProcessSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);

    auto event = std::make_shared<ProcessEvent>(1234, 5678, KernelEventType::PROCESS_EXECVE_EVENT, 0);
    APSARA_TEST_EQUAL(manager->HandleEvent(event), 0);

    APSARA_TEST_NOT_EQUAL(manager->HandleEvent(nullptr), 0);

    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));
    manager->Suspend();
    APSARA_TEST_FALSE(manager->IsRunning());
    APSARA_TEST_EQUAL(manager->HandleEvent(event), 0);

    manager->Destroy();
}

void ManagerUnittest::TestNetworkSecurityManagerBasic() {
    auto manager = std::make_shared<NetworkSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);

    // 测试初始化
    SecurityOptions options;
    APSARA_TEST_EQUAL(manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试暂停
    APSARA_TEST_EQUAL(manager->Suspend(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());

    // 测试恢复
    APSARA_TEST_EQUAL(manager->Resume(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options)), 0);
    APSARA_TEST_TRUE(manager->IsRunning());

    // 测试销毁
    APSARA_TEST_EQUAL(manager->Destroy(), 0);
    APSARA_TEST_FALSE(manager->IsRunning());
}

void ManagerUnittest::TestNetworkSecurityManagerEventHandling() {
    auto manager = std::make_shared<NetworkSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

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

void ManagerUnittest::TestNetworkSecurityManagerAggregation() {
    auto manager = std::make_shared<NetworkSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

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
    auto execveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id key{1234, 5678};
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->process.filename = "test_filename";
    execveEvent->process.args = "test_arg";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->msg.cleanup_key.ktime = 0;
    execveEvent->msg.parent.pid = 2345;
    execveEvent->msg.parent.ktime = 6789;

    // 测试缓存更新
    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(execveEvent));

    auto pExecveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id pkey{2345, 6789};
    pExecveEvent->process.pid = 2345;
    pExecveEvent->process.ktime = 6789;
    pExecveEvent->process.uid = 1000;
    pExecveEvent->process.binary = "test_binary";
    pExecveEvent->process.cmdline = "test_cmdline";
    pExecveEvent->process.filename = "test_filename";
    pExecveEvent->process.args = "test_arg";
    pExecveEvent->process.cmdline = "test_cmdline";

    mProcessCacheManager->updateCache(pkey, std::shared_ptr<MsgExecveEventUnix>(pExecveEvent));

    // 触发聚合
    auto execTime = std::chrono::steady_clock::now();
    APSARA_TEST_TRUE(manager->ConsumeAggregateTree(execTime));

    manager->Destroy();
}

void ManagerUnittest::TestProcessSecurityManagerAggregation() {
    auto manager = std::make_shared<ProcessSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

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
    auto execveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id key{1234, 5678};
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->process.filename = "test_filename";
    execveEvent->process.args = "test_arg";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->msg.cleanup_key.ktime = 0;
    execveEvent->msg.parent.pid = 2345;
    execveEvent->msg.parent.ktime = 6789;

    // 测试缓存更新
    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(execveEvent));

    auto pExecveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id pkey{2345, 6789};
    pExecveEvent->process.pid = 2345;
    pExecveEvent->process.ktime = 6789;
    pExecveEvent->process.uid = 1000;
    pExecveEvent->process.binary = "test_binary";
    pExecveEvent->process.cmdline = "test_cmdline";
    pExecveEvent->process.filename = "test_filename";
    pExecveEvent->process.args = "test_arg";
    pExecveEvent->process.cmdline = "test_cmdline";

    mProcessCacheManager->updateCache(pkey, std::shared_ptr<MsgExecveEventUnix>(pExecveEvent));

    // 触发聚合
    auto execTime = std::chrono::steady_clock::now();
    APSARA_TEST_TRUE(manager->ConsumeAggregateTree(execTime));

    manager->Destroy();
}

void ManagerUnittest::TestFileSecurityManagerAggregation() {
    auto manager = std::make_shared<FileSecurityManager>(mProcessCacheManager, mSourceManager, mEventQueue, mTimer);
    SecurityOptions options;
    manager->Init(std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

    // 创建多个相关的文件事件
    std::vector<std::shared_ptr<FileEvent>> events;

    // 同一连接的多个事件
    for (int i = 0; i < 3; ++i) {
        events.push_back(
            std::make_shared<FileEvent>(1234, // pid
                                        5678, // ktime
                                        KernelEventType::FILE_PATH_TRUNCATE, // type
                                        std::chrono::system_clock::now().time_since_epoch().count() + i, // timestamp
                                        "/test/" + std::to_string(i) // path
                                        ));
    }

    // 处理所有事件
    for (const auto& event : events) {
        APSARA_TEST_EQUAL(manager->HandleEvent(event), 0);
    }

    // add cache
    auto execveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id key{1234, 5678};
    execveEvent->process.pid = 1234;
    execveEvent->process.ktime = 5678;
    execveEvent->process.uid = 1000;
    execveEvent->process.binary = "test_binary";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->process.filename = "test_filename";
    execveEvent->process.args = "test_arg";
    execveEvent->process.cmdline = "test_cmdline";
    execveEvent->msg.cleanup_key.ktime = 0;
    execveEvent->msg.parent.pid = 2345;
    execveEvent->msg.parent.ktime = 6789;

    // 测试缓存更新
    mProcessCacheManager->updateCache(key, std::shared_ptr<MsgExecveEventUnix>(execveEvent));

    auto pExecveEvent = std::make_shared<MsgExecveEventUnix>();
    data_event_id pkey{2345, 6789};
    pExecveEvent->process.pid = 2345;
    pExecveEvent->process.ktime = 6789;
    pExecveEvent->process.uid = 1000;
    pExecveEvent->process.binary = "test_binary";
    pExecveEvent->process.cmdline = "test_cmdline";
    pExecveEvent->process.filename = "test_filename";
    pExecveEvent->process.args = "test_arg";
    pExecveEvent->process.cmdline = "test_cmdline";

    mProcessCacheManager->updateCache(pkey, std::shared_ptr<MsgExecveEventUnix>(pExecveEvent));

    // 触发聚合
    auto execTime = std::chrono::steady_clock::now();
    APSARA_TEST_TRUE(manager->ConsumeAggregateTree(execTime));

    manager->Destroy();
}

// UNIT_TEST_CASE(ManagerUnittest, TestProcessSecurityManagerBasic);
UNIT_TEST_CASE(ManagerUnittest, TestProcessSecurityManagerEventHandling);
// UNIT_TEST_CASE(ManagerUnittest, TestFileSecurityManagerBasic);
UNIT_TEST_CASE(ManagerUnittest, TestFileSecurityManagerEventHandling);
UNIT_TEST_CASE(ManagerUnittest, TestManagerConcurrency);
UNIT_TEST_CASE(ManagerUnittest, TestManagerErrorHandling);
// UNIT_TEST_CASE(ManagerUnittest, TestNetworkSecurityManagerBasic);
UNIT_TEST_CASE(ManagerUnittest, TestNetworkSecurityManagerEventHandling);
UNIT_TEST_CASE(ManagerUnittest, TestNetworkSecurityManagerAggregation);
UNIT_TEST_CASE(ManagerUnittest, TestProcessSecurityManagerAggregation);
UNIT_TEST_CASE(ManagerUnittest, TestFileSecurityManagerAggregation);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
