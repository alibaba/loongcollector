// Copyright 2023 iLogtail Authors
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

#include "common/NetworkUtil.h"
#include "unittest/Unittest.h"

#if defined(__linux__)
#include <arpa/inet.h>
#endif

namespace logtail {

class NetworkUtilUnittest : public ::testing::Test {
public:
    void TestGetAddrString();
    void TestGetFamilyString();
    void TestGetProtocolString();
    void TestGetStateString();

    void TestCIDROptions();
};

void NetworkUtilUnittest::TestGetAddrString() {
#if defined(__linux__)
    // Test IP address conversion
    // 127.0.0.1 in network byte order
    uint32_t localhost = 0x0100007F;
    APSARA_TEST_STREQ_DESC(GetAddrString(localhost).c_str(), "127.0.0.1", "Localhost IP should match");

    // 192.168.1.1 in network byte order
    uint32_t localIP = 0x0101A8C0;
    APSARA_TEST_STREQ_DESC(GetAddrString(localIP).c_str(), "192.168.1.1", "Local IP should match");

    // 8.8.8.8 in network byte order
    uint32_t googleDNS = 0x08080808;
    APSARA_TEST_STREQ_DESC(GetAddrString(googleDNS).c_str(), "8.8.8.8", "Google DNS IP should match");
#endif
}

void NetworkUtilUnittest::TestGetFamilyString() {
#if defined(__linux__)
    // Test common address families
    APSARA_TEST_STREQ_DESC(GetFamilyString(AF_INET).c_str(), "AF_INET", "IPv4 family should match");
    APSARA_TEST_STREQ_DESC(GetFamilyString(AF_INET6).c_str(), "AF_INET6", "IPv6 family should match");
    APSARA_TEST_STREQ_DESC(GetFamilyString(AF_UNIX).c_str(), "AF_UNIX", "Unix domain socket family should match");
#endif
}

void NetworkUtilUnittest::TestGetProtocolString() {
    // Test common protocols
    APSARA_TEST_STREQ_DESC(GetProtocolString(1).c_str(), "ICMP", "ICMP protocol should match");
    APSARA_TEST_STREQ_DESC(GetProtocolString(6).c_str(), "TCP", "TCP protocol should match");
    APSARA_TEST_STREQ_DESC(GetProtocolString(17).c_str(), "UDP", "UDP protocol should match");
    APSARA_TEST_STREQ_DESC(GetProtocolString(89).c_str(), "OSPF", "OSPF protocol should match");

    // Test unknown protocol
    APSARA_TEST_STREQ_DESC(GetProtocolString(999).c_str(), "Unknown", "Unknown protocol should return 'Unknown'");
}

void NetworkUtilUnittest::TestGetStateString() {
    // Test TCP states
    APSARA_TEST_STREQ_DESC(GetStateString(1).c_str(), "TCP_ESTABLISHED", "TCP ESTABLISHED state should match");
    APSARA_TEST_STREQ_DESC(GetStateString(2).c_str(), "TCP_SYN_SENT", "TCP SYN_SENT state should match");
    APSARA_TEST_STREQ_DESC(GetStateString(10).c_str(), "TCP_LISTEN", "TCP LISTEN state should match");

    // Test invalid state
    APSARA_TEST_STREQ_DESC(GetStateString(999).c_str(), "INVALID_STATE", "Invalid state should return 'INVALID_STATE'");
}

void NetworkUtilUnittest::TestCIDROptions() {
    CIDR cidr;
    bool status = ParseCIDR("192.168.0.1/27", &cidr);
    LOG_DEBUG(sLogger, ("cidr addr", std::get<uint32_t>(cidr.mAddr.mIp)));
    APSARA_TEST_TRUE(status);
    // 3232235521 ===> 192.168.0.1
    // 3232235786 ===> 192.168.1.10
    bool contains = CIDRContainsForIPV4(16820416, 24, 167880896);
    APSARA_TEST_FALSE(contains);
    // test 192.168.1.10
    contains = CIDRContainsForIPV4(16820416, 16, 167880896);
    APSARA_TEST_TRUE(contains);
}

UNIT_TEST_CASE(NetworkUtilUnittest, TestGetAddrString);
UNIT_TEST_CASE(NetworkUtilUnittest, TestGetFamilyString);
UNIT_TEST_CASE(NetworkUtilUnittest, TestGetProtocolString);
UNIT_TEST_CASE(NetworkUtilUnittest, TestGetStateString);
UNIT_TEST_CASE(NetworkUtilUnittest, TestCIDROptions);


} // namespace logtail

UNIT_TEST_MAIN
