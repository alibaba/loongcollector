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

#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/CPUCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class CPUCollectorUnittest : public testing::Test {
public:
    void TestCollect() const;

protected:
    void SetUp() override {
        ofstream ofs("./stat", std::ios::trunc);
        ofs << "cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0\n";
        ofs << "cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 1 2\n";
        ofs << "cpua a b c d e f 424744 0 1 2\n";
        ofs << "cpu1 14708487 14216 4613031 2108180843\n"; // test old linux kernel
        ofs << "cpu3 14708487 14216 4613031 2108180843"; // test old linux kernel
        ofs.close();
        PROCESS_DIR = ".";
    }
};

void CPUCollectorUnittest::TestCollect() const {
    auto collector = CPUCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectConfig(CPUCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));
    APSARA_TEST_EQUAL_FATAL(3 * 10, group.GetEvents().size());
    vector<double> expected1 = {14708487.0 / SYSTEM_HERTZ,
                                14216.0 / SYSTEM_HERTZ,
                                4613031.0 / SYSTEM_HERTZ,
                                2108180843.0 / SYSTEM_HERTZ,
                                57199.0 / SYSTEM_HERTZ,
                                0.0,
                                424744.0 / SYSTEM_HERTZ,
                                0.0,
                                1.0 / SYSTEM_HERTZ,
                                2.0 / SYSTEM_HERTZ};
    vector<double> expected2 = {14708487.0 / SYSTEM_HERTZ,
                                14216.0 / SYSTEM_HERTZ,
                                4613031.0 / SYSTEM_HERTZ,
                                2108180843.0 / SYSTEM_HERTZ,
                                0.0,
                                0.0,
                                0.0,
                                0.0,
                                0.0,
                                0.0};
    vector<double> expected3 = {14708487.0 / SYSTEM_HERTZ,
                                14216.0 / SYSTEM_HERTZ,
                                4613031.0 / SYSTEM_HERTZ,
                                2108180843.0 / SYSTEM_HERTZ,
                                0.0,
                                0.0,
                                0.0,
                                0.0,
                                0.0,
                                0.0};
    vector<string> expectedMode = {"user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"};
    for (size_t i = 0; i < 8; ++i) {
        auto event = group.GetEvents()[i].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_seconds_total", event.GetName());
        APSARA_TEST_EQUAL_FATAL(expected1[i], event.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("0", event.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i], event.GetTag("mode"));
        auto event2 = group.GetEvents()[i + 10].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_seconds_total", event2.GetName());
        APSARA_TEST_EQUAL_FATAL(expected2[i], event2.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("1", event2.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i], event2.GetTag("mode"));
        auto event3 = group.GetEvents()[i + 20].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_seconds_total", event3.GetName());
        APSARA_TEST_EQUAL_FATAL(expected3[i], event3.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("3", event3.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i], event3.GetTag("mode"));
    }
    for (size_t i = 8; i < 10; ++i) {
        auto event = group.GetEvents()[i].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_guest_seconds_total", event.GetName());
        APSARA_TEST_EQUAL_FATAL(expected1[i], event.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("0", event.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i - 8], event.GetTag("mode"));
        auto event2 = group.GetEvents()[i + 10].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_guest_seconds_total", event2.GetName());
        APSARA_TEST_EQUAL_FATAL(expected2[i], event2.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("1", event2.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i - 8], event2.GetTag("mode"));
        auto event3 = group.GetEvents()[i + 20].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL("node_cpu_guest_seconds_total", event3.GetName());
        APSARA_TEST_EQUAL_FATAL(expected3[i], event3.GetValue<UntypedSingleValue>()->mValue);
        APSARA_TEST_EQUAL_FATAL("3", event3.GetTag("cpu"));
        APSARA_TEST_EQUAL_FATAL(expectedMode[i - 8], event3.GetTag("mode"));
    }
}

UNIT_TEST_CASE(CPUCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN
