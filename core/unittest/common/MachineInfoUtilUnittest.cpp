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

#include "unittest/Unittest.h"
#include "common/MachineInfoUtil.h"

namespace logtail {
class HostnameValidationUnittest : public ::testing::Test {
public:
    void DecHostnameValidationTest() {
        std::string hostname;
        hostname = "4294967296";  // 2^32
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "4294967295";  // 2^32-1
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
        hostname = "255.16777216";  // 2^24
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
        hostname = "040000000000";  // 2^32
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "037777777777";  // 2^32-1
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));

        hostname = "0400.0123";
        EXPECT_TRUE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0123";
        EXPECT_FALSE(IsDigitsDotsHostname(hostname.c_str()));
        hostname = "0377.0100000000";  // 2^24
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

} // end of namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
