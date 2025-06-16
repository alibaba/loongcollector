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

#include "CPUCollector.h"
#include "MetricCalculate.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
struct MockData {
    double data1;
    double data2;
    double data3;
    // Define the field descriptors
    static inline const FieldName<MockData> mockDataFields[] = {
        FIELD_ENTRY(MockData, data1),
        FIELD_ENTRY(MockData, data2),
        FIELD_ENTRY(MockData, data3),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<MockData, double>&)>& callback) {
        for (const auto& field : mockDataFields) {
            callback(field);
        }
    }
};


class MetricCalculateUnittest : public testing::Test {
public:
    void TestReset();
    void TestAddValue();
    void TestGetMaxValue();
    void TestGetMinValue();
    void TestGetAvgValue();
    void TestStat();
    void TestGetLastValue();
    void TestCount();
    void TestCalAfterReset();
};

void MetricCalculateUnittest::TestReset() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData = {1, 2, 3};
    metricCalculate.AddValue(mockData);
    metricCalculate.Reset();
    int cnt = metricCalculate.Count();
    APSARA_TEST_EQUAL(cnt, 0);
}

void MetricCalculateUnittest::TestAddValue() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData = {1, 2, 3};
    metricCalculate.AddValue(mockData);
    int cnt = metricCalculate.Count();
    APSARA_TEST_EQUAL(cnt, 1);
}

void MetricCalculateUnittest::TestGetMaxValue() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    MockData maxData;
    metricCalculate.GetMaxValue(maxData);
    APSARA_TEST_EQUAL(maxData.data1, 3);
    APSARA_TEST_EQUAL(maxData.data2, 6);
    APSARA_TEST_EQUAL(maxData.data3, 9);
}

void MetricCalculateUnittest::TestGetMinValue() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    MockData minData;
    metricCalculate.GetMinValue(minData);
    APSARA_TEST_EQUAL(minData.data1, 1);
    APSARA_TEST_EQUAL(minData.data2, 4);
    APSARA_TEST_EQUAL(minData.data3, 7);
}

void MetricCalculateUnittest::TestGetAvgValue() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    MockData avgData;
    metricCalculate.GetAvgValue(avgData);
    APSARA_TEST_EQUAL(avgData.data1, 2);
    APSARA_TEST_EQUAL(avgData.data2, 5);
    APSARA_TEST_EQUAL(avgData.data3, 8);
}

void MetricCalculateUnittest::TestStat() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    MockData maxData, minData, avgData;
    metricCalculate.Stat(maxData, minData, avgData);
    APSARA_TEST_EQUAL(maxData.data1, 3);
    APSARA_TEST_EQUAL(maxData.data2, 6);
    APSARA_TEST_EQUAL(maxData.data3, 9);
    APSARA_TEST_EQUAL(minData.data1, 1);
    APSARA_TEST_EQUAL(minData.data2, 4);
    APSARA_TEST_EQUAL(minData.data3, 7);
    APSARA_TEST_EQUAL(avgData.data1, 2);
    APSARA_TEST_EQUAL(avgData.data2, 5);
    APSARA_TEST_EQUAL(avgData.data3, 8);
}

void MetricCalculateUnittest::TestGetLastValue() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData lastData;
    metricCalculate.GetLastValue(lastData);
    APSARA_TEST_EQUAL(lastData.data1, 2);
    APSARA_TEST_EQUAL(lastData.data2, 4);
    APSARA_TEST_EQUAL(lastData.data3, 9);
}

void MetricCalculateUnittest::TestCount() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    int cnt = metricCalculate.Count();
    APSARA_TEST_EQUAL(cnt, 3);
}

void MetricCalculateUnittest::TestCalAfterReset() {
    MetricCalculate<MockData> metricCalculate;
    MockData mockData0 = {1, 1, 1};
    metricCalculate.AddValue(mockData0);
    int cnt = metricCalculate.Count();
    APSARA_TEST_EQUAL(cnt, 1);
    metricCalculate.Reset();
    MockData mockData1 = {1, 6, 8};
    metricCalculate.AddValue(mockData1);
    MockData mockData2 = {2, 4, 9};
    metricCalculate.AddValue(mockData2);
    MockData mockData3 = {3, 5, 7};
    metricCalculate.AddValue(mockData3);
    cnt = metricCalculate.Count();
    APSARA_TEST_EQUAL(cnt, 3);
    MockData maxData, minData, avgData;
    metricCalculate.Stat(maxData, minData, avgData);
    APSARA_TEST_EQUAL(maxData.data1, 3);
    APSARA_TEST_EQUAL(minData.data2, 4);
    APSARA_TEST_EQUAL(avgData.data3, 8);
}

UNIT_TEST_CASE(MetricCalculateUnittest, TestReset);
UNIT_TEST_CASE(MetricCalculateUnittest, TestAddValue);
UNIT_TEST_CASE(MetricCalculateUnittest, TestGetMaxValue);
UNIT_TEST_CASE(MetricCalculateUnittest, TestGetMinValue);
UNIT_TEST_CASE(MetricCalculateUnittest, TestGetAvgValue);
UNIT_TEST_CASE(MetricCalculateUnittest, TestStat);
UNIT_TEST_CASE(MetricCalculateUnittest, TestGetLastValue);
UNIT_TEST_CASE(MetricCalculateUnittest, TestCount);
UNIT_TEST_CASE(MetricCalculateUnittest, TestCalAfterReset);


} // namespace logtail

UNIT_TEST_MAIN
