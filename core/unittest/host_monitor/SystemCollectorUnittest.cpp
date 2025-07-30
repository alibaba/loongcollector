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
        PROCESS_DIR = ".";
    }
};

void SystemCollectorUnittest::TestCollect() const {
    double cores = static_cast<double>(std::thread::hardware_concurrency());
    auto collector = SystemCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectconfig(SystemCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_EQUAL_FATAL(1UL, group.GetEvents().size());

    vector<string> expected_names = {"load_1m_min",
                                     "load_1m_max",
                                     "load_1m_avg",
                                     "load_5m_min",
                                     "load_5m_max",
                                     "load_5m_avg",
                                     "load_15m_min",
                                     "load_15m_max",
                                     "load_15m_avg",
                                     "load_per_core_1m_min",
                                     "load_per_core_1m_max",
                                     "load_per_core_1m_avg",
                                     "load_per_core_5m_min",
                                     "load_per_core_5m_max",
                                     "load_per_core_5m_avg",
                                     "load_per_core_15m_min",
                                     "load_per_core_15m_max",
                                     "load_per_core_15m_avg"};
    vector<double> expected_values = {0.10, 0.07, 0.03, 0.10 / cores, 0.07 / cores, 0.03 / cores};

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
    for (size_t i = 0; i < 18; ++i) {
        APSARA_TEST_TRUE(maps.find(expected_names[i]) != maps.end());
        double val = maps[expected_names[i]].Value;
        EXPECT_NEAR(expected_values[static_cast<size_t>(i / 3)], val, 1e-6);
    }
}

UNIT_TEST_CASE(SystemCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN
