// Copyright 2025 iLogtail Authors
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

#include <typeinfo>

#include "MetricEvent.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/SystemCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SystemCollectorUnittest : public ::testing::Test {
public:
    void TestGetHostSystemLoadStat() const;
    void TestCollect() const;

protected:
    void SetUp() override {
        ofstream ofs("./loadavg", std::ios::trunc);
        // 0.10 0.07 0.03 1/561 78450
        ofs << "0.10 0.07 0.03 1/561 78450";
        ofs.close();
        PROC_LOADAVG = "./loadavg";
    }
};

void SystemCollectorUnittest::TestGetHostSystemLoadStat() const {
    // hardware_concurrency_ptr = mock_hardware_concurrency;
    auto collector = SystemCollector();
    double cores = static_cast<double>(std::thread::hardware_concurrency());
    SystemStat stat;
    APSARA_TEST_TRUE(collector.GetHostSystemLoadStat(stat));
    APSARA_TEST_EQUAL_FATAL(0.10, stat.load1);
    APSARA_TEST_EQUAL_FATAL(0.07, stat.load5);
    APSARA_TEST_EQUAL_FATAL(0.03, stat.load15);
    APSARA_TEST_EQUAL_FATAL(0.10 / cores, stat.load1_per_core);
    APSARA_TEST_EQUAL_FATAL(0.07 / cores, stat.load5_per_core);
    APSARA_TEST_EQUAL_FATAL(0.03 / cores, stat.load15_per_core);
}

void SystemCollectorUnittest::TestCollect() const {
    double cores = static_cast<double>(std::thread::hardware_concurrency());
    auto collector = SystemCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectconfig(SystemCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_EQUAL_FATAL(18, group.GetEvents().size());

    vector<string> expected_names
        = {"load_1m", "load_5m", "load_15m", "load_per_core_1m", "load_per_core_5m", "load_per_core_15m"};
    vector<double> expected_values = {0.10, 0.07, 0.03, 0.10 / cores, 0.07 / cores, 0.03 / cores};
    vector<string> expected_tags = {"Minimum", "Maximum", "Average"};

    for (size_t i = 0; i < 6; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            auto event = group.GetEvents()[j + i * 3].Cast<MetricEvent>();
            APSARA_TEST_EQUAL_FATAL(expected_names[i], event.GetName());
            EXPECT_NEAR(expected_values[i], event.GetValue<UntypedSingleValue>()->mValue, 1e-6);
            APSARA_TEST_EQUAL_FATAL(expected_tags[j], event.GetTag("valueTag"));
        }
    }
}

UNIT_TEST_CASE(SystemCollectorUnittest, TestGetHostSystemLoadStat);
UNIT_TEST_CASE(SystemCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN
