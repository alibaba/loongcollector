/*
 * Copyright 2024 iLogtail Authors
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

#include <string>

#include "prometheus/labels/TextParser.h"
#include "prometheus/labels/TextParserSIMD.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class TextParserBenchmark : public testing::Test {
public:
    void TestParse100M() const;
    void TestParse1000M() const;

protected:
    void SetUp() override {
        m100MData.reserve(100 * 1024 * 1024);
        // 100MB
        int repeatCnt = 100 * 1024 * 1024 / mRawData.size();
        while (repeatCnt > 0) {
            m100MData += mRawData;
            repeatCnt -= 1;
        }

        m1000MData.reserve(1000 * 1024 * 1024);
        repeatCnt = 1000 * 1024 * 1024 / mRawData.size();
        while (repeatCnt > 0) {
            m1000MData += mRawData;
            repeatCnt -= 1;
        }
    }

private:
    std::string mRawData = R"""(
test_metric1{k111111111111="v11111111111", k222222="v2"} 2.0 1234567890
test_metric2{k111111111111="v11111111111",k222222="v2"} 9.9410452992e+10
test_metric3{k111111111111="v11111111111",k222222="v2"} 9.9410452992e+10 1715829785083
test_metric4{k111111111111="v11111111111", k222222="v2" } 9.9410452992e+10 1715829785083
test_metric5{k111111111111="v11111111111",k222222="v2",} 9.9410452992e+10 1715829785083
test_metric6{k111111111111="v11111111111",k222222="v2", } 9.9410452992e+10 1715829785083
test_metric7{k111111111111="v11111111111", k222222="v2", } 9.9410452992e+10 1715829785083
test_metric8{k111111111111="v11111111111", k222222="v2", } 9.9410452992e+10 1715829785083
)""";
    std::string m100MData;
    std::string m1000MData;
};

void TextParserBenchmark::TestParse100M() const {
    auto start = std::chrono::high_resolution_clock::now();

    TextParser parser;
    auto res = parser.Parse(m100MData, 0, 0);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    cout << "elapsed: " << elapsed.count() << " seconds" << endl;
    // elapsed: 1.53s in release mode
    // elapsed: 551MB in release mode
    // 2.51s -> 1.26s when rawLine is longer if we use SIMD
}

void TextParserBenchmark::TestParse1000M() const {
    auto start = std::chrono::high_resolution_clock::now();

    TextParser parser;
    auto res = parser.Parse(m1000MData, 0, 0);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    cout << "elapsed: " << elapsed.count() << " seconds" << endl;
    // elapsed: 15.4s in release mode
    // elapsed: 4960MB in release mode
}

UNIT_TEST_CASE(TextParserBenchmark, TestParse100M)
UNIT_TEST_CASE(TextParserBenchmark, TestParse1000M)

} // namespace logtail

UNIT_TEST_MAIN
