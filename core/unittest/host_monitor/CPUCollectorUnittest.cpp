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

#include <chrono>

#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CPUCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class CPUCollectorUnittest : public testing::Test {
public:
    void TestCollectNormal() const;
    void TestCpuValue0() const;
    void TestjiffiesDeltaNegative() const;
    void TestCpuCount0() const;
    void TestGetCPUInformation() const;
    void TestGetCPUInformationInterface() const;
    void TestGroupNull() const;

protected:
    void SetUp() override {
        ofstream ofs("./stat", std::ios::trunc);
        ofs << "cpu  1 1 1 1 1 1 1 1 1 1\n";
        ofs << "cpu0 1 1 1 1 1 1 1 1 1 1\n";
        ofs << "cpu1 1 1 1 1 1 1 1 1 1 1\n";
        ofs << "cpu2 1 1 1 1 1 1 1 1 1 1";
        ofs.close();
        PROCESS_DIR = ".";
    }
};

void CPUCollectorUnittest::TestCollectNormal() const {
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));
    collectContext.Reset();
    collectContext.mCountPerReport = 3;

    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    collectContext.mCollectTime.mMetricTime += 1;
    collectContext.mCollectTime.mScheduleTime += std::chrono::seconds(1);
    ofstream ofs("./stat", std::ios::trunc);
    ofs << "cpu  2 2 2 2 2 2 2 2 2 2 \n";
    ofs << "cpu0 1 1 1 1 1 1 1 1 1 1\n";
    ofs << "cpu1 1 1 1 1 1 1 1 1 1 1\n";
    ofs << "cpu2 1 1 1 1 1 1 1 1 1 1";
    ofs.close();
    PROCESS_DIR = ".";
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    collectContext.mCollectTime.mMetricTime += 1;
    collectContext.mCollectTime.mScheduleTime += std::chrono::seconds(1);
    ofstream ofs1("./stat", std::ios::trunc);
    ofs1 << "cpu  3 3 3 3 3 3 3 3 3 3 \n";
    ofs1 << "cpu0 1 1 1 1 1 1 1 1 1 1\n";
    ofs1 << "cpu1 1 1 1 1 1 1 1 1 1 1\n";
    ofs1 << "cpu2 1 1 1 1 1 1 1 1 1 1";
    ofs1.close();
    PROCESS_DIR = ".";
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    collectContext.mCollectTime.mMetricTime += 1;
    collectContext.mCollectTime.mScheduleTime += std::chrono::seconds(1);
    ofstream ofs2("./stat", std::ios::trunc);
    ofs2 << "cpu  4 4 4 4 4 4 4 4 4 4 \n";
    ofs2 << "cpu0 1 1 1 1 1 1 1 1 1 1\n";
    ofs2 << "cpu1 1 1 1 1 1 1 1 1 1 1\n";
    ofs2 << "cpu2 1 1 1 1 1 1 1 1 1 1";
    ofs2.close();
    PROCESS_DIR = ".";
    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

    APSARA_TEST_EQUAL_FATAL(1UL, group.GetEvents().size());

    vector<string> expected_names = {"cpu_system_avg",
                                     "cpu_system_max",
                                     "cpu_system_min",
                                     "cpu_idle_avg",
                                     "cpu_idle_max",
                                     "cpu_idle_min",
                                     "cpu_user_avg",
                                     "cpu_user_max",
                                     "cpu_user_min",
                                     "cpu_wait_avg",
                                     "cpu_wait_max",
                                     "cpu_wait_min",
                                     "cpu_other_avg",
                                     "cpu_other_max",
                                     "cpu_other_min",
                                     "cpu_total_avg",
                                     "cpu_total_max",
                                     "cpu_total_min",
                                     "cpu_cores_value"};
    vector<double> expected_values
        = {12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 12.5, 50, 50, 50, 87.5, 87.5, 87.5, 3};

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
    for (size_t i = 0; i < 19; ++i) {
        APSARA_TEST_TRUE(maps.find(expected_names[i]) != maps.end());
        double val = maps[expected_names[i]].Value;
        EXPECT_NEAR(expected_values[static_cast<size_t>(i)], val, 1e-6);
    }
}

void CPUCollectorUnittest::TestCpuValue0() const {
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));

    ofstream ofs("./stat", std::ios::trunc);
    ofs << "cpu  0 0 0 0 0 0 0 0 0 0 \n";
    ofs << "cpu0 0 0 0 0 0 0 0 0 0 0 \n";
    ofs.close();

    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));
    APSARA_TEST_FALSE_FATAL(collector.Collect(collectContext, &group));
}

void CPUCollectorUnittest::TestjiffiesDeltaNegative() const {
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));

    ofstream ofs("./stat", std::ios::trunc);
    ofs << "cpu  2 2 2 2 2 2 2 2 2 2 \n";
    ofs << "cpu0 2 2 2 2 2 2 2 2 2 2 \n";
    ofs.close();
    PROCESS_DIR = ".";
    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

    ofstream ofs1("./stat", std::ios::trunc);
    ofs1 << "cpu 1 1 1 1 1 1 1 1 1 1\n";
    ofs1 << "cpu0 1 1 1 1 1 1 1 1 1 1\n";

    ofs1.close();
    PROCESS_DIR = ".";

    APSARA_TEST_FALSE_FATAL(collector.Collect(collectContext, &group));
}

void CPUCollectorUnittest::TestCpuCount0() const {
    CPUInformation cpuInfo;
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));
    collectContext.Reset();

    ofstream ofs2("./stat", std::ios::trunc);
    ofs2 << "cpu  0 0 0 0 0 0 0 0 0 0\n";
    ofs2.close();
    PROCESS_DIR = ".";

    APSARA_TEST_TRUE(SystemInterface::GetInstance()->GetCPUInformation(time(nullptr), cpuInfo));

    APSARA_TEST_FALSE_FATAL(collector.Collect(collectContext, &group));
}

void CPUCollectorUnittest::TestGetCPUInformation() const {
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));
    collectContext.Reset();

    ofstream ofs3("./stat", std::ios::trunc);
    ofs3 << "cpua a b c \n";
    ofs3.close();
    PROCESS_DIR = ".";

    APSARA_TEST_FALSE_FATAL(collector.Collect(collectContext, &group));
}

void CPUCollectorUnittest::TestGetCPUInformationInterface() const {
    CPUInformation cpuInfo;
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    collectContext.mCollectTime.mMetricTime += 1;
    collectContext.mCollectTime.mScheduleTime += std::chrono::seconds(1);
    ofstream ofs4("./stat", std::ios::trunc);
    ofs4 << "";
    ofs4.close();
    PROCESS_DIR = ".";

    APSARA_TEST_FALSE_FATAL(SystemInterface::GetInstance()->GetCPUInformation(time(nullptr), cpuInfo));
}

void CPUCollectorUnittest::TestGroupNull() const {
    auto collector = CPUCollector();

    auto cpuCollector = std::make_unique<CPUCollector>();
    HostMonitorContext collectContext("test",
                                      CPUCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(cpuCollector)));

    APSARA_TEST_FALSE_FATAL(collector.Collect(collectContext, nullptr));
}

UNIT_TEST_CASE(CPUCollectorUnittest, TestCollectNormal);
UNIT_TEST_CASE(CPUCollectorUnittest, TestCpuValue0);
UNIT_TEST_CASE(CPUCollectorUnittest, TestjiffiesDeltaNegative);
UNIT_TEST_CASE(CPUCollectorUnittest, TestCpuCount0);
UNIT_TEST_CASE(CPUCollectorUnittest, TestGetCPUInformation);
UNIT_TEST_CASE(CPUCollectorUnittest, TestGetCPUInformationInterface);
UNIT_TEST_CASE(CPUCollectorUnittest, TestGroupNull);

} // namespace logtail

UNIT_TEST_MAIN
