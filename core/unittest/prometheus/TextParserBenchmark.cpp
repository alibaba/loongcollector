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
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class TextParserBenchmark : public testing::Test {
public:
    void TestParse100M() const;

protected:
    void SetUp() override {
        // 100MB
        int totalSize = 100 * 1024 * 1024;
        while (totalSize > 0) {
            m100MData += mRawData;
            totalSize -= mRawData.size();
        }
    }

private:
    std::string mRawData = R"""(
test_metric1{k1="v1", k2="v2"} 2.0 1234567890
test_metric2{k1="v1",k2="v2"} 9.9410452992e+10
test_metric3{k1="v1",k2="v2"} 9.9410452992e+10 1715829785083
test_metric4{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083
test_metric5{k1="v1",k2="v2",} 9.9410452992e+10 1715829785083
test_metric6{k1="v1",k2="v2", } 9.9410452992e+10 1715829785083
test_metric7{k1="v1", k2="v2", } 9.9410452992e+10 1715829785083
test_metric8{k1="v1", k2="v2", } 9.9410452992e+10 1715829785083
        )""";
    std::string m100MData;
};

void TextParserBenchmark::TestParse100M() const {
    auto start = std::chrono::high_resolution_clock::now();

    TextParser parser;
    parser.Parse(m100MData);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    cout << "elapsed: " << elapsed.count() << " seconds" << endl;
    // elapsed: 10.1014 seconds in release mode
}

UNIT_TEST_CASE(TextParserBenchmark, TestParse100M)

} // namespace logtail

UNIT_TEST_MAIN
