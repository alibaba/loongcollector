// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/compression/LZ4Compressor.h"
#include "monitor/MetricConstants.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class CompressorMock : public Compressor {
public:
    CompressorMock(CompressType type) : Compressor(type) {};

    bool UnCompress(const std::string& input, std::string& output, std::string& errorMsg) override { return true; }

private:
    bool Compress(const std::string& input, std::string& output, std::string& errorMsg) override {
        if (input == "failed") {
            return false;
        }
        output = input.substr(0, input.size() / 2);
        return true;
    }
};

class CompressorUnittest : public ::testing::Test {
public:
    void TestMetric();
};

void CompressorUnittest::TestMetric() {
    {
        CompressorMock compressor(CompressType::MOCK);
        compressor.SetMetricRecordRef({});
        string input = "hello world";
        string output;
        string errorMsg;
        compressor.DoCompress(input, output, errorMsg);
        APSARA_TEST_EQUAL(1U, compressor.mInItemsCnt->GetValue());
        APSARA_TEST_EQUAL(input.size(), compressor.mInItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(1U, compressor.mOutItemsCnt->GetValue());
        APSARA_TEST_EQUAL(output.size(), compressor.mOutItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(0U, compressor.mDiscardedItemsCnt->GetValue());
        APSARA_TEST_EQUAL(0U, compressor.mDiscardedItemSizeBytes->GetValue());
    }
    {
        CompressorMock compressor(CompressType::MOCK);
        compressor.SetMetricRecordRef({});
        string input = "failed";
        string output;
        string errorMsg;
        compressor.DoCompress(input, output, errorMsg);
        APSARA_TEST_EQUAL(1U, compressor.mInItemsCnt->GetValue());
        APSARA_TEST_EQUAL(input.size(), compressor.mInItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(0U, compressor.mOutItemsCnt->GetValue());
        APSARA_TEST_EQUAL(0U, compressor.mOutItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(1U, compressor.mDiscardedItemsCnt->GetValue());
        APSARA_TEST_EQUAL(input.size(), compressor.mDiscardedItemSizeBytes->GetValue());
    }
}

UNIT_TEST_CASE(CompressorUnittest, TestMetric)

} // namespace logtail

UNIT_TEST_MAIN
