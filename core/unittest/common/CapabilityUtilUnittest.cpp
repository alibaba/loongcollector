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

#include "common/CapabilityUtil.h"
#include "unittest/Unittest.h"

namespace logtail {

class CapabilityUtilUnittest : public ::testing::Test {
public:
    void TestGetCapability();
    void TestGetCapabilities();
    void TestInvalidCapability();
};

void CapabilityUtilUnittest::TestGetCapability() {
    // Test some common capabilities
    APSARA_TEST_STREQ_DESC(GetCapability(0).c_str(), "CAP_CHOWN", "CAP_CHOWN should match");
    APSARA_TEST_STREQ_DESC(GetCapability(1).c_str(), "DAC_OVERRIDE", "DAC_OVERRIDE should match");
    APSARA_TEST_STREQ_DESC(GetCapability(2).c_str(), "CAP_DAC_READ_SEARCH", "CAP_DAC_READ_SEARCH should match");
    APSARA_TEST_STREQ_DESC(GetCapability(7).c_str(), "CAP_SETUID", "CAP_SETUID should match");
    APSARA_TEST_STREQ_DESC(GetCapability(21).c_str(), "CAP_SYS_ADMIN", "CAP_SYS_ADMIN should match");
}

void CapabilityUtilUnittest::TestGetCapabilities() {
    // Test single capability
    uint64_t singleCap = 1ULL << 0; // CAP_CHOWN
    APSARA_TEST_STREQ_DESC(GetCapabilities(singleCap).c_str(), "CAP_CHOWN", "Single capability should match");

    // Test multiple capabilities
    uint64_t multipleCaps = (1ULL << 0) | (1ULL << 1) | (1ULL << 7); // CAP_CHOWN, DAC_OVERRIDE, CAP_SETUID
    std::string result = GetCapabilities(multipleCaps);
    APSARA_TEST_TRUE(result.find("CAP_CHOWN") != std::string::npos);
    APSARA_TEST_TRUE(result.find("DAC_OVERRIDE") != std::string::npos);
    APSARA_TEST_TRUE(result.find("CAP_SETUID") != std::string::npos);

    // Test no capabilities
    APSARA_TEST_STREQ_DESC(GetCapabilities(0).c_str(), "", "No capabilities should return empty string");

    // Test all capabilities
    uint64_t allCaps = ~0ULL;
    result = GetCapabilities(allCaps);
    APSARA_TEST_TRUE(result.find("CAP_CHOWN") != std::string::npos);
    APSARA_TEST_TRUE(result.find("CAP_SYS_ADMIN") != std::string::npos);
    APSARA_TEST_TRUE(result.find("CAP_CHECKPOINT_RESTORE") != std::string::npos);
}

void CapabilityUtilUnittest::TestInvalidCapability() {
    // Test invalid capability value
    try {
        GetCapability(-1);
        APSARA_TEST_TRUE(false); // Should not reach here
    } catch (const std::invalid_argument& e) {
        APSARA_TEST_TRUE(true);
    }

    try {
        GetCapability(100);
        APSARA_TEST_TRUE(false); // Should not reach here
    } catch (const std::invalid_argument& e) {
        APSARA_TEST_TRUE(true);
    }
}

UNIT_TEST_CASE(CapabilityUtilUnittest, TestGetCapability);
UNIT_TEST_CASE(CapabilityUtilUnittest, TestGetCapabilities);
UNIT_TEST_CASE(CapabilityUtilUnittest, TestInvalidCapability);

} // namespace logtail

UNIT_TEST_MAIN
