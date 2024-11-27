// Copyright 2023 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <fstream>
#include <json/json.h>
#include <list>
#include <atomic>
#include <thread>
#include "MetricManager.h"
#include "MetricConstants.h"

namespace logtail {


static std::atomic_bool running(true);


class MetricManagerUnittest : public ::testing::Test {
public:
    void SetUp() {}

    void TearDown() {
        ReadMetrics::GetInstance()->Clear();
        WriteMetrics::GetInstance()->Clear();
    }

    void TestCreateMetricAutoDelete();
    void TestCreateMetricAutoDeleteMultiThread();
    void TestCreateAndDeleteMetric();
};

APSARA_UNIT_TEST_CASE(MetricManagerUnittest, TestCreateMetricAutoDelete, 0);
APSARA_UNIT_TEST_CASE(MetricManagerUnittest, TestCreateMetricAutoDeleteMultiThread, 1);
APSARA_UNIT_TEST_CASE(MetricManagerUnittest, TestCreateAndDeleteMetric, 2);


void MetricManagerUnittest::TestCreateMetricAutoDelete() {
    std::vector<std::pair<std::string, std::string>> labels;
    labels.emplace_back(std::make_pair<std::string, std::string>("project", "project1"));
    labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "logstore1"));
    labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-hangzhou"));

    MetricsRecordRef fileMetric;
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(fileMetric, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
    APSARA_TEST_EQUAL(fileMetric->GetLabels()->size(), 3);


    CounterPtr fileCounter = fileMetric.CreateCounter("filed1");
    fileCounter->Add(111UL);
    fileCounter->Add(111UL);
    APSARA_TEST_EQUAL(fileCounter->GetValue(), 222);

    ReadMetrics::GetInstance()->UpdateMetrics();

    // assert WriteMetrics count
    MetricsRecord* tmp = WriteMetrics::GetInstance()->GetHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);


    // assert ReadMetrics count
    tmp = ReadMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);

    // mock create in other class, should be delete after
    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "project1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "logstore1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-hangzhou"));

        MetricsRecordRef fileMetric2;
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(fileMetric2, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter2 = fileMetric2.CreateCounter("filed2");
        fileCounter2->Add(222UL);
    }

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "project1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "logstore1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-hangzhou"));
        MetricsRecordRef fileMetric3;
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(fileMetric3, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter3 = fileMetric3.CreateCounter("filed3");
        fileCounter3->Add(333UL);
    }

    ReadMetrics::GetInstance()->UpdateMetrics();

    // assert WriteMetrics count
    tmp = WriteMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);


    // assert ReadMetrics count
    tmp = ReadMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);
}

void createMetrics(int count) {
    for (int i = 0; i < count; i++) {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("num", std::to_string(i)));
        labels.emplace_back(std::make_pair<std::string, std::string>("count", std::to_string(count)));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        MetricsRecordRef fileMetric;
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(fileMetric, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter = fileMetric.CreateCounter("filed1");
        fileCounter->Add(111UL);
    }
}

void MetricManagerUnittest::TestCreateMetricAutoDeleteMultiThread() {
    std::thread t1(createMetrics, 1);
    std::thread t2(createMetrics, 2);
    std::thread t3(createMetrics, 3);
    std::thread t4(createMetrics, 4);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // assert WriteMetrics count
    MetricsRecord* tmp = WriteMetrics::GetInstance()->GetHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    // 1 + 2 + 3 + 4 = 10
    APSARA_TEST_EQUAL(count, 10);

    for (int i = 0; i < 10; i++) {
        ReadMetrics::GetInstance()->UpdateMetrics();
    }

    // assert WriteMetrics count
    tmp = WriteMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            LOG_INFO(sLogger, ("key", pair.first)("value", pair.second));
        }
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 0);

    // assert ReadMetrics count
    tmp = ReadMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 0);
}


void MetricManagerUnittest::TestCreateAndDeleteMetric() {
    std::thread t1(createMetrics, 1);
    std::thread t2(createMetrics, 2);

    MetricsRecordRef* fileMetric1 = new MetricsRecordRef();
    MetricsRecordRef* fileMetric2 = new MetricsRecordRef();
    MetricsRecordRef* fileMetric3 = new MetricsRecordRef();


    std::vector<std::pair<std::string, std::string>> labels;
    labels.emplace_back(std::make_pair<std::string, std::string>("project", "test1"));
    labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "test1"));
    labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(*fileMetric1, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
    CounterPtr fileCounter = fileMetric1->CreateCounter("filed1");
    fileCounter->Add(111UL);

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "test2"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "test2"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(*fileMetric2, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter = fileMetric2->CreateCounter("filed1");
        fileCounter->Add(111UL);
    }

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "test3"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "test3"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(*fileMetric3, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter = fileMetric3->CreateCounter("filed1");
        fileCounter->Add(111UL);
    }
    std::thread t3(createMetrics, 3);
    std::thread t4(createMetrics, 4);


    t1.join();
    t2.join();
    t3.join();
    t4.join();
    // assert WriteMetrics count
    MetricsRecord* tmp = WriteMetrics::GetInstance()->GetHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    // 10 + 3
    APSARA_TEST_EQUAL(count, 13);

    delete fileMetric2;
    delete fileMetric3;

    ReadMetrics::GetInstance()->UpdateMetrics();

    // assert WriteMetrics count
    tmp = WriteMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        for (auto item = tmp->GetLabels()->begin(); item != tmp->GetLabels()->end(); ++item) {
            std::pair<std::string, std::string> pair = *item;
            LOG_INFO(sLogger, ("key", pair.first)("value", pair.second));
        }
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);
    // assert writeMetric value
    if (count == 1) {
        tmp = WriteMetrics::GetInstance()->GetHead();
        std::vector<CounterPtr> values = tmp->GetCounters();
        APSARA_TEST_EQUAL(values.size(), 1);
        if (values.size() == 1) {
            APSARA_TEST_EQUAL(values.at(0)->GetValue(), 0);
        }
    }

    // assert ReadMetrics count
    tmp = ReadMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);

    // assert readMetric value
    if (count == 1) {
        tmp = ReadMetrics::GetInstance()->GetHead();
        std::vector<CounterPtr> values = tmp->GetCounters();
        APSARA_TEST_EQUAL(values.size(), 1);
        if (values.size() == 1) {
            APSARA_TEST_EQUAL(values.at(0)->GetValue(), 111);
        }
    }

    // after dosnapshot, add value again
    fileCounter->Add(111UL);
    fileCounter->Add(111UL);
    fileCounter->Add(111UL);

    APSARA_TEST_EQUAL(fileCounter->GetValue(), 333);

    ReadMetrics::GetInstance()->UpdateMetrics();
    // assert ReadMetrics count
    tmp = ReadMetrics::GetInstance()->GetHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);

    // assert readMetric value
    if (count == 1) {
        tmp = ReadMetrics::GetInstance()->GetHead();
        std::vector<CounterPtr> values = tmp->GetCounters();
        APSARA_TEST_EQUAL(values.size(), 1);
        if (values.size() == 1) {
            APSARA_TEST_EQUAL(values.at(0)->GetValue(), 333);
        }
    }
    delete fileMetric1;
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}