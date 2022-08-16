// Copyright 2022 iLogtail Authors
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

#include <gtest/gtest.h>

#include "unittest/Unittest.h"
#include "metas/ConnectionMetaManager.h"
#include "DynamicLibHelper.h"

namespace logtail {

// ConnectionMetaUnitTest depends on some network namespace and connection env.
// Currently, only use log to valid it.
// TODO: Polish unittest.
class ConnectionMetaUnitTest : public ::testing::Test {
public:
    void TestReadInode() {
        int8_t errorCode = 0;
        std::string str = "net:[4026531993]";
        uint32_t number = ReadNetworkNsInodeNum(str, errorCode);
        ASSERT_TRUE(number == 4026531993);

        errorCode = 0;
        str = "socket:[4026531993]";
        number = ReadSocketInodeNum(str, errorCode);
        ASSERT_TRUE(number == 4026531993);
    }

    /**
     *   [2022-04-27 15:04:30.859418]    [debug] open self net ns:success        path:/dev/proc/self/ns/net
     *   [2022-04-27 15:04:30.859437]    [debug] open pid net ns:success path:/dev/proc/27534/ns/net
     *   [2022-04-27 15:04:30.859444]    [debug] set ns status:success
     *   [2022-04-27 15:04:30.859448]    [debug] recover netlink net ns:success
     *   [2022-04-27 15:04:30.859454]    [debug] close pid net ns fd:success
     *   [2022-04-27 15:04:30.859457]    [debug] close self net ns fd:success
     */
    void TestBindNetNamespace() { NetLinkBinder binder(27534, "/dev/proc"); }

    /**
     * inode221157938
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 30.30.73.112 localPort: 2222 remotePort: 61552 stat: 1
     * inode44347245
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 11.239.153.17 localPort: 20459 remotePort: 19888 stat: 1
     * inode220153303
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 30.25.233.92 localPort: 2222 remotePort: 52762 stat: 1
     * inode221725976
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 30.30.73.112 localPort: 2222 remotePort: 50089 stat: 1
     * inode221434065
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 30.30.73.112 localPort: 22 remotePort: 63831 stat: 1
     * inode220153335
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 30.25.233.92 localPort: 2222 remotePort: 52767 stat: 1
     * inode195705799
     * family: 2 localAddr: 11.122.76.82 remoteAddr: 100.82.131.45 localPort: 60430 remotePort: 8000 stat: 1
     */
    void TestFetchInetConnections() {
        NetLinkProber prober(594114, 1, "/dev/proc/");
        ASSERT_TRUE(prober.Status() == 0);
        std::unordered_map<uint32_t, ConnectionInfoPtr> infos;
        prober.FetchInetConnections(infos);
        for (const auto& item : infos) {
            std::cout << "inode:" << item.first << std::endl;
            item.second->Print();
        }
    }

    void TestFetchUnixConnections() {
        NetLinkProber prober(594114, 1, "/dev/proc/");
        ASSERT_TRUE(prober.Status() == 0);
        std::unordered_map<uint32_t, ConnectionInfoPtr> infos;
        prober.FetchUnixConnections(infos);
        for (const auto& item : infos) {
            std::cout << "inode:" << item.first << std::endl;
            item.second->Print();
        }
    }

    void TestReadFdLink() {
        std::cout << sizeof(sockaddr) << std::endl;
        std::cout << sizeof(sockaddr_in) << std::endl;
        std::cout << sizeof(sockaddr_in6) << std::endl;
        std::cout << "==============" << std::endl;
        APSARA_TEST_TRUE(logtail::glibc::LoadGlibcFunc());
        auto instance = ConnectionMetaManager::GetInstance();
        APSARA_TEST_TRUE(instance->Init("/proc/"));
        auto info = instance->GetConnectionInfo(48064, 6);
        instance->Print();
        std::cout << "==============" << std::endl;
        if (info != nullptr) {
            info->Print();
        }
    }
};


// TODO add auto check
//    APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestBindNetNamespace, 0);
//    APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestReadInode, 0);
//    APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestFetchInetConnections, 0);
//    APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestFetchUnixConnections, 0);
APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestReadFdLink, 0);
//    APSARA_UNIT_TEST_CASE(ConnectionMetaUnitTest, TestIPV6, 0);

} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}