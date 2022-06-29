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
#include "common/SlidingWindowCounter.h"

namespace logtail {

class SlidingWindowCounterUnittest : public ::testing::Test {
public:
    void TestLoadCounter();

private:
    std::string toLoadStats(size_t load1, size_t load5, size_t load15) {
        std::ostringstream oss;
        oss << load1 << "," << load5 << "," << load15;
        return oss.str();
    }
};

UNIT_TEST_CASE(SlidingWindowCounterUnittest, TestLoadCounter);

void SlidingWindowCounterUnittest::TestLoadCounter() {
    auto counter = CreateLoadCounter();

    APSARA_TEST_EQUAL(counter.Add(0), toLoadStats(0, 0, 0));
    for (size_t idx = 1; idx <= 5; ++idx) {
        SCOPED_TRACE(std::string("1~5:") + std::to_string(idx));
        APSARA_TEST_EQUAL(counter.Add(1), toLoadStats(1, idx, idx));
    }
    for (size_t idx = 6; idx <= 15; ++idx) {
        SCOPED_TRACE(std::string("6~15:") + std::to_string(idx));
        APSARA_TEST_EQUAL(counter.Add(1), toLoadStats(1, 5, idx));
    }

    APSARA_TEST_EQUAL(counter.Add(15), toLoadStats(15, 19, 29));
    for (size_t idx = 1; idx <= 4; ++idx) {
        SCOPED_TRACE(std::string("19~15:") + std::to_string(idx));
        APSARA_TEST_EQUAL(counter.Add(0), toLoadStats(0, 19 - idx, 29 - idx));
    }
    APSARA_TEST_EQUAL(counter.Add(0), toLoadStats(0, 0, 24));
    for (size_t idx = 1; idx <= 9; ++idx) {
        SCOPED_TRACE(std::string("24~15:") + std::to_string(idx));
        APSARA_TEST_EQUAL(counter.Add(0), toLoadStats(0, 0, 24 - idx));
    }
    APSARA_TEST_EQUAL(counter.Add(0), toLoadStats(0, 0, 0));
}

} // namespace logtail

UNIT_TEST_MAIN
