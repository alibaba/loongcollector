/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common/TimeKeeper.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace logtail;

class TimeKeeperBenchmark : public testing::Test {
public:
    void TestGetTimeNs();
    void TestGetTimeMs();
};

/*
[ RUN      ] TimeKeeperBenchmark.TestGetTimeNs
TimeKeeper.NowNs elapsed: 0.00223174 seconds
GetCurrentTimeInNanoSeconds elapsed: 0.300854 seconds
*/
void TimeKeeperBenchmark::TestGetTimeNs() {
    int iterations = 1000000;
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto ns = TimeKeeper::GetInstance()->NowNs();
            (void)ns;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "TimeKeeper.NowNs elapsed: " << elapsed.count() << " seconds" << endl;
    }
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto ns = GetCurrentTimeInNanoSeconds();
            (void)ns;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "GetCurrentTimeInNanoSeconds elapsed: " << elapsed.count() << " seconds" << endl;
    }
}

/*
TimeKeeper.NowNs elapsed: 0.0026014 seconds
GetCurrentTimeInNanoSeconds elapsed: 0.301546 seconds
*/
void TimeKeeperBenchmark::TestGetTimeMs() {
    int iterations = 1000000;
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto ns = TimeKeeper::GetInstance()->NowMs();
            (void)ns;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "TimeKeeper.NowNs elapsed: " << elapsed.count() << " seconds" << endl;
    }
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto ns = GetCurrentTimeInMilliSeconds();
            (void)ns;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "GetCurrentTimeInNanoSeconds elapsed: " << elapsed.count() << " seconds" << endl;
    }
}

UNIT_TEST_CASE(TimeKeeperBenchmark, TestGetTimeNs)
UNIT_TEST_CASE(TimeKeeperBenchmark, TestGetTimeMs)

UNIT_TEST_MAIN
