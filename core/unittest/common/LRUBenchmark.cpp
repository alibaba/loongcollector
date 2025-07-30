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

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/LRUCache.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class LRUBenchmark : public testing::Test {
public:
    void TestReadWrite_1_1();
    void TestReadWrite_10_1();

protected:
    void SetUp() override {
        mt19937 generator(mRd());
        uniform_int_distribution<int> distribution(0, 10000);
        for (int i = 0; i < 100000; ++i) {
            int k = distribution(generator);
            int v = distribution(generator);
            mKVs.emplace_back(to_string(k), to_string(v));
        }
    }

private:
    void TestReadWrite(int readIterations);
    vector<pair<string, string>> mKVs;
    random_device mRd;
};

void LRUBenchmark::TestReadWrite(int readIterations) {
    {
        lru11::Cache<string, string> cache(100000);
        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& kv : mKVs) {
            cache.insert(kv.first, kv.second);
        }
        for (int i = 0; i < readIterations; ++i) {
            for (const auto& kv : mKVs) {
                cache.getRef(kv.first);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "LRU elapsed: " << elapsed.count() << " seconds" << endl;
    }
    {
        unordered_map<string, string> cache(100000);
        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& kv : mKVs) {
            cache[kv.first] = kv.second;
        }
        for (int i = 0; i < readIterations; ++i) {
            for (const auto& kv : mKVs) {
                cache.find(kv.first);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        cout << "unordered_map elapsed: " << elapsed.count() << " seconds" << endl;
    }
}

void LRUBenchmark::TestReadWrite_1_1() {
    TestReadWrite(1);
    // elapsed: 1.53s in release mode
    // elapsed: 551MB in release mode
}

void LRUBenchmark::TestReadWrite_10_1() {
    TestReadWrite(10);
    // elapsed: 15.4s in release mode
    // elapsed: 4960MB in release mode
}

UNIT_TEST_CASE(LRUBenchmark, TestReadWrite_1_1)
UNIT_TEST_CASE(LRUBenchmark, TestReadWrite_10_1)

} // namespace logtail

UNIT_TEST_MAIN
