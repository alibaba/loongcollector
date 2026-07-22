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

#include "common/MachineInfoUtil.h"
#include "unittest/Unittest.h"

#if defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <string>

#include "json/value.h"

#include "app_config/AppConfig.h"
#include "common/LogtailCommonFlags.h"
#endif

namespace logtail {
class HostnameValidationUnittest : public ::testing::Test {
public:
    void DecHostnameValidationTest() {
        std::string hostname;
        hostname = "4294967296"; // 2^32
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "4294967295"; // 2^32-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0.";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "256.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.16777216"; // 2^24
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.16777215"; // 2^24-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.0";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.0.";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "256.123.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.256.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.0.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.65536"; // 2^16
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.65535"; // 2^16-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.0";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.0.";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "256.123.123.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.123.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0.123.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.256.123.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.0.123.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.256.123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.255.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.0.123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.255.256";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.255.255";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.255.0";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "255.255.255.0.";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "255.255.a255";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "a255d";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "abcd";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
    }

    void OctHostnameValidationTest() {
        std::string hostname;
        hostname = "040000000000"; // 2^32
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "037777777777"; // 2^32-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "0400.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0100000000"; // 2^24
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.077777777"; // 2^24-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "0400.0123.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0123.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0400.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0200000"; // 2^16
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0177777"; // 2^16-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "0400.0123.0123.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0123.0123.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0400.0123.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0123.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0400.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0377.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0377.0400";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0377.0377.0377";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
    }
};

UNIT_TEST_CASE(HostnameValidationUnittest, DecHostnameValidationTest);
UNIT_TEST_CASE(HostnameValidationUnittest, OctHostnameValidationTest);

#if defined(__linux__)
class MachineInfoUtilLinuxUnittest : public ::testing::Test {
public:
    void TestNicIpv4AndHostIdentityHelpers() {
        Json::Value conf;
        conf[kIgnoredInterfacesKey] = "kube-ipvs0,nodelocaldns,docker0";
        AppConfig::GetInstance()->ParseJsonToFlags(conf);

        EXPECT_TRUE(IsIgnoredInterfaceForHostIdentity("kube-ipvs0"));
        (void)GetAnyAvailableIP();
    }

    // Find an interface whose IPv4 address is neither empty nor loopback, so the working_interface
    // branch of GetHostIp can be exercised deterministically. Returns "" if none is available.
    static std::string FindInterfaceWithValidIp() {
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) != 0) {
            return "";
        }
        std::string result;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            std::string name = ifa->ifa_name;
            std::string ip = GetHostIpByInterface(name);
            if (!ip.empty() && ip.find("127.") != 0) {
                result = name;
                break;
            }
        }
        freeifaddrs(ifaddr);
        return result;
    }

    void TestGetHostIpWithWorkingInterface() {
        std::string savedFlag = STRING_FLAG(working_interface);

        // Case 1: working_interface points to a non-existent NIC. GetHostIpByInterface yields "",
        // so GetHostIp must fall through to the original hostname + interface fallback chain and
        // return exactly what it returns when working_interface is empty.
        STRING_FLAG(working_interface) = "";
        std::string baseline = GetHostIp();
        STRING_FLAG(working_interface) = "loongcollector_nonexistent_iface";
        EXPECT_EQ(GetHostIp(), baseline);

        // Case 2: working_interface points to a NIC with a valid (non-loopback) IP. GetHostIp must
        // prioritize that interface and return its IP directly, regardless of hostname resolution.
        std::string goodIntf = FindInterfaceWithValidIp();
        if (!goodIntf.empty()) {
            std::string expectedIp = GetHostIpByInterface(goodIntf);
            STRING_FLAG(working_interface) = goodIntf;
            EXPECT_EQ(GetHostIp(), expectedIp);
            // The passed-in intf argument must not override the working_interface priority.
            EXPECT_EQ(GetHostIp("loongcollector_nonexistent_iface"), expectedIp);
        }
        // else: no non-loopback IPv4 interface in this environment; the valid-IP case is skipped.

        STRING_FLAG(working_interface) = savedFlag;
    }
};

UNIT_TEST_CASE(MachineInfoUtilLinuxUnittest, TestNicIpv4AndHostIdentityHelpers);
UNIT_TEST_CASE(MachineInfoUtilLinuxUnittest, TestGetHostIpWithWorkingInterface);
#endif

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
