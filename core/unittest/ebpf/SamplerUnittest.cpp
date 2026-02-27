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

#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "ebpf/util/TraceId.h"
#include "ebpf/util/sampler/Sampler.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"


DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class SamplerUnittest : public testing::Test {
public:
    SamplerUnittest() {}

    void TestRandFromSpanID();
    void TestSampleAll();

    void TestRandFromSpanID64();
    void TestSampleAll64();


protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    std::unique_ptr<HashRatioSampler> mSampler;
};

void SamplerUnittest::TestRandFromSpanID() {
    mSampler = std::make_unique<HashRatioSampler>(0.01);
    int totalCount = 0;
    int sampledCount = 0;
    for (int i = 0; i < 1000000; i++) {
        auto id = GenerateSpanID();
        bool result = mSampler->ShouldSample(id);
        totalCount++;
        if (result) {
            sampledCount++;
        }
    }
    double realPortion = double(sampledCount) / double(totalCount);
    ASSERT_GE(realPortion, 0.009);
    ASSERT_LE(realPortion, 0.011);
    APSARA_TEST_GE(realPortion, 0.009);
    APSARA_TEST_LE(realPortion, 0.011);
}
void SamplerUnittest::TestSampleAll() {
    mSampler = std::make_unique<HashRatioSampler>(1);
    for (int i = 0; i < 1000000; i++) {
        auto id = GenerateSpanID();
        APSARA_TEST_TRUE(mSampler->ShouldSample(id));
    }
}

// void SamplerUnittest::TestRandFromSpanID64() {
//     mSampler = std::make_unique<HashRatioSampler>(0.01);
//     int totalCount = 0;
//     int sampledCount = 0;
//     for (int i = 0; i < 1000000; i++) {
//         auto id = GenerateSpanID64();
//         bool result = mSampler->ShouldSample64(id);
//         totalCount++;
//         if (result) {
//             sampledCount++;
//         }
//     }
//     double realPortion = double(sampledCount) / double(totalCount);
//     ASSERT_GE(realPortion, 0.009);
//     ASSERT_LE(realPortion, 0.011);
//     APSARA_TEST_GE(realPortion, 0.009);
//     APSARA_TEST_LE(realPortion, 0.011);
// }
// void SamplerUnittest::TestSampleAll64() {
//     mSampler = std::make_unique<HashRatioSampler>(1);
//     for (int i = 0; i < 1000000; i++) {
//         auto id = GenerateSpanID64();
//         APSARA_TEST_TRUE(mSampler->ShouldSample64(id));
//     }
// }

UNIT_TEST_CASE(SamplerUnittest, TestRandFromSpanID);
UNIT_TEST_CASE(SamplerUnittest, TestSampleAll);
// UNIT_TEST_CASE(SamplerUnittest, TestRandFromSpanID64);
// UNIT_TEST_CASE(SamplerUnittest, TestSampleAll64);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
