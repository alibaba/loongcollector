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
#include "MetricExportor.h"
#include "MetricConstants.h"

namespace logtail {


static std::atomic_bool running(true);


class MetricManagerUnittest : public ::testing::Test {
public:
    void SetUp() {}

    void TearDown() {
        MetricManager::GetInstance()->ClearWriteList();
        MetricManager::GetInstance()->ClearReadList();
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
    MetricManager::GetInstance()->PrepareMetricsRecordRef(fileMetric, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
    APSARA_TEST_EQUAL(fileMetric->GetLabels()->size(), 3);


    CounterPtr fileCounter = fileMetric.CreateCounter("filed1");
    fileCounter->Add(111UL);
    fileCounter->Add(111UL);
    APSARA_TEST_EQUAL(fileCounter->GetValue(), 222);

    MetricExportor::GetInstance()->PushMetrics(true);

    // assert MetricManager count
    MetricsRecord* tmp = MetricManager::GetInstance()->GetWriteListHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);


    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetReadListHead();
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
        MetricManager::GetInstance()->PrepareMetricsRecordRef(fileMetric2, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter2 = fileMetric2.CreateCounter("filed2");
        fileCounter2->Add(222UL);
    }

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "project1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "logstore1"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-hangzhou"));
        MetricsRecordRef fileMetric3;
        MetricManager::GetInstance()->PrepareMetricsRecordRef(fileMetric3, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter3 = fileMetric3.CreateCounter("filed3");
        fileCounter3->Add(333UL);
    }

    MetricExportor::GetInstance()->PushMetrics(true);

    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetWriteListHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);


    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetReadListHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);
}

void PushMetrics() {
    for (int i = 0; i < 10; i++) {
        LOG_INFO(sLogger, ("PushMetricsCount", i));
        MetricExportor::GetInstance()->PushMetrics(true);
    }
}

void createMetrics(int count) {
    for (int i = 0; i < count; i++) {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("num", std::to_string(i)));
        labels.emplace_back(std::make_pair<std::string, std::string>("count", std::to_string(count)));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        MetricsRecordRef fileMetric;
        MetricManager::GetInstance()->PrepareMetricsRecordRef(fileMetric, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
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

    // assert MetricManager count
    MetricsRecord* tmp = MetricManager::GetInstance()->GetWriteListHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    // 1 + 2 + 3 + 4 = 10
    APSARA_TEST_EQUAL(count, 10);

    for (int i = 0; i < 10; i++) {
        MetricExportor::GetInstance()->PushMetrics(true);
    }

    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetWriteListHead();
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

    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetReadListHead();
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
    MetricManager::GetInstance()->PrepareMetricsRecordRef(*fileMetric1, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
    CounterPtr fileCounter = fileMetric1->CreateCounter("filed1");
    fileCounter->Add(111UL);

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "test2"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "test2"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        MetricManager::GetInstance()->PrepareMetricsRecordRef(*fileMetric2, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter = fileMetric2->CreateCounter("filed1");
        fileCounter->Add(111UL);
    }

    {
        std::vector<std::pair<std::string, std::string>> labels;
        labels.emplace_back(std::make_pair<std::string, std::string>("project", "test3"));
        labels.emplace_back(std::make_pair<std::string, std::string>("logstore", "test3"));
        labels.emplace_back(std::make_pair<std::string, std::string>("region", "cn-beijing"));
        MetricManager::GetInstance()->PrepareMetricsRecordRef(*fileMetric3, MetricCategory::METRIC_CATEGORY_UNKNOWN, std::move(labels));
        CounterPtr fileCounter = fileMetric3->CreateCounter("filed1");
        fileCounter->Add(111UL);
    }
    std::thread t3(createMetrics, 3);
    std::thread t4(createMetrics, 4);


    t1.join();
    t2.join();
    t3.join();
    t4.join();
    // assert MetricManager count
    MetricsRecord* tmp = MetricManager::GetInstance()->GetWriteListHead();
    int count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    // 10 + 3
    APSARA_TEST_EQUAL(count, 13);

    delete fileMetric2;
    delete fileMetric3;

    MetricExportor::GetInstance()->PushMetrics(true);

    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetWriteListHead();
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
        tmp = MetricManager::GetInstance()->GetWriteListHead();
        std::vector<CounterPtr> values = tmp->GetCounters();
        APSARA_TEST_EQUAL(values.size(), 1);
        if (values.size() == 1) {
            APSARA_TEST_EQUAL(values.at(0)->GetValue(), 0);
        }
    }

    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetReadListHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);

    // assert readMetric value
    if (count == 1) {
        tmp = MetricManager::GetInstance()->GetReadListHead();
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

    MetricExportor::GetInstance()->PushMetrics(true);
    // assert MetricManager count
    tmp = MetricManager::GetInstance()->GetReadListHead();
    count = 0;
    while (tmp) {
        tmp = tmp->GetNext();
        count++;
    }
    APSARA_TEST_EQUAL(count, 1);

    // assert readMetric value
    if (count == 1) {
        tmp = MetricManager::GetInstance()->GetReadListHead();
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