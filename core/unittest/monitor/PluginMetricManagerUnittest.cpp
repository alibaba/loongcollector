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

#include "monitor/metric_constants/MetricConstants.h"
#include "monitor/PluginMetricManager.h"
#include "unittest/Unittest.h"

namespace logtail {

class PluginMetricManagerUnittest : public ::testing::Test {
public:
    void SetUp() {
        MetricLabelsPtr defaultLabels = std::make_shared<MetricLabels>();
        defaultLabels->emplace_back(METRIC_LABEL_KEY_PROJECT, "default_project");
        defaultLabels->emplace_back(METRIC_LABEL_KEY_LOGSTORE, "default_logstore");
        defaultLabels->emplace_back(METRIC_LABEL_KEY_REGION, "default_region");
        defaultLabels->emplace_back(METRIC_LABEL_KEY_PIPELINE_NAME, "default_config");
        defaultLabels->emplace_back(METRIC_LABEL_KEY_PLUGIN_TYPE, "default_plugin");
        defaultLabels->emplace_back(METRIC_LABEL_KEY_PLUGIN_ID, "default_id");
        WriteMetrics::GetInstance()->PrepareMetricsRecordRef(mMetricsRecordRef, std::move(*defaultLabels));
        std::unordered_map<std::string, MetricType> metricKeys;
        metricKeys.emplace("default_counter", MetricType::METRIC_TYPE_COUNTER);
        metricKeys.emplace("default_gauge", MetricType::METRIC_TYPE_INT_GAUGE);
        pluginMetricManager = std::make_shared<PluginMetricManager>(mMetricsRecordRef->GetLabels(), metricKeys);
    }

    void TearDown() {}

    void TestInitPluginMetricManager();
    void TestGetOrCreateMetricsRecordRefPtr();
    void TestReleaseMetricsRecordRefPtr();
    void TestRegisterSizeGauge();
    void TestReentrantMetricsRecord();

private:
    MetricsRecordRef mMetricsRecordRef;
    PluginMetricManagerPtr pluginMetricManager;
};

APSARA_UNIT_TEST_CASE(PluginMetricManagerUnittest, TestInitPluginMetricManager, 0);
APSARA_UNIT_TEST_CASE(PluginMetricManagerUnittest, TestGetOrCreateMetricsRecordRefPtr, 1);
APSARA_UNIT_TEST_CASE(PluginMetricManagerUnittest, TestReleaseMetricsRecordRefPtr, 2);
APSARA_UNIT_TEST_CASE(PluginMetricManagerUnittest, TestRegisterSizeGauge, 3);
APSARA_UNIT_TEST_CASE(PluginMetricManagerUnittest, TestReentrantMetricsRecord, 4);

void PluginMetricManagerUnittest::TestInitPluginMetricManager() {
    APSARA_TEST_EQUAL(pluginMetricManager->mDefaultLabels.size(), 6);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 0);
}

void PluginMetricManagerUnittest::TestGetOrCreateMetricsRecordRefPtr() {
    MetricLabels labels;
    labels.emplace_back("test_label", "test_value");

    auto ptr_1 = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_NOT_EQUAL(ptr_1, nullptr);
    APSARA_TEST_EQUAL(ptr_1->GetLabels()->size(), 7);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 1);
    APSARA_TEST_EQUAL(ptr_1.use_count(), 2);

    auto ptr_2 = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(ptr_2->GetLabels()->size(), 7);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 1);
    APSARA_TEST_EQUAL(ptr_2.use_count(), 3);
    APSARA_TEST_EQUAL(ptr_1.use_count(), ptr_2.use_count());
}

void PluginMetricManagerUnittest::TestReleaseMetricsRecordRefPtr() {
    MetricLabels labels;
    labels.emplace_back("test_label", "test_value");

    auto ptr = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_NOT_EQUAL(ptr, nullptr);
    APSARA_TEST_EQUAL(ptr->GetLabels()->size(), 7);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 1);

    pluginMetricManager->ReleaseReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 0);

    auto ptr2 = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_NOT_EQUAL(ptr2, nullptr);
    APSARA_TEST_NOT_EQUAL(ptr, ptr2); // Should not be the same if the first one was released
}

void PluginMetricManagerUnittest::TestRegisterSizeGauge() {
    IntGaugePtr sizeGauge = mMetricsRecordRef.CreateIntGauge("test_gauge");
    pluginMetricManager->RegisterSizeGauge(sizeGauge);

    MetricLabels labels;
    labels.emplace_back("test_label", "test_value");

    auto ptr = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(sizeGauge->GetValue(), 1); // One entry should be in the map

    pluginMetricManager->ReleaseReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(sizeGauge->GetValue(), 0); // The entry should have been removed
}

void PluginMetricManagerUnittest::TestReentrantMetricsRecord() {
    MetricLabels labels;
    labels.emplace_back("test_label", "test_value");

    auto ptr = pluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(), 1); // One entry should be in the map
    APSARA_TEST_EQUAL(ptr->GetLabels()->size(), 7);

    auto counter_valid = ptr->GetCounter("default_counter");
    APSARA_TEST_NOT_EQUAL(counter_valid, nullptr);

    auto counter_invalid = ptr->GetCounter("invalid_counter");
    APSARA_TEST_EQUAL(counter_invalid, nullptr);

    pluginMetricManager->ReleaseReentrantMetricsRecordRef(labels);
    APSARA_TEST_EQUAL(pluginMetricManager->mReentrantMetricsRecordRefsMap.size(),
                      0); // The entry should have been removed
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}