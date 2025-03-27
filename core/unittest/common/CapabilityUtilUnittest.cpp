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
#include "memory/SourceBuffer.h"
#include "unittest/Unittest.h"

namespace logtail {

class CapabilityUtilUnittest : public ::testing::Test {
public:
    void TestGetCapabilities();
};

void CapabilityUtilUnittest::TestGetCapabilities() {
    // Test single capability
    uint64_t singleCap = 1ULL << 0; // CAP_CHOWN
    SourceBuffer sb;
    APSARA_TEST_STREQ_FATAL(GetCapabilities(singleCap, sb).data(), "CAP_CHOWN");

    // Test multiple capabilities
    uint64_t multipleCaps = (1ULL << 0) | (1ULL << 1) | (1ULL << 7); // CAP_CHOWN, DAC_OVERRIDE, CAP_SETUID
    auto result = GetCapabilities(multipleCaps, sb);
    APSARA_TEST_STREQ_FATAL(result.data(), "CAP_CHOWN DAC_OVERRIDE CAP_SETUID");

    // Test no capabilities
    APSARA_TEST_STREQ_DESC(GetCapabilities(0, sb).data(), "", "No capabilities should return empty string");

    // Test all capabilities
    uint64_t allCaps = ~0ULL;
    result = GetCapabilities(allCaps, sb);
    APSARA_TEST_TRUE(result.find("CAP_CHOWN") != std::string::npos);
    APSARA_TEST_TRUE(result.find("CAP_SYS_ADMIN") != std::string::npos);
    APSARA_TEST_TRUE(result.find("CAP_CHECKPOINT_RESTORE") != std::string::npos); // the last capability
}

UNIT_TEST_CASE(CapabilityUtilUnittest, TestGetCapabilities);

} // namespace logtail

UNIT_TEST_MAIN
