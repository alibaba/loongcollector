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
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CgroupCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class CgroupCollectorUnittest : public testing::Test {
public:
    void TestCollectNormal() const;

protected:
    void SetUp() override {
        ofstream ofs("./cgroups", std::ios::trunc);
        ofs << "#subsys_name    hierarchy       num_cgroups     enabled\n";
        ofs << "cpuset  10      5       1\n";
        ofs << "cpu     3       89      1\n";
        ofs << "cpuacct 3       89      1\n";
        ofs << "blkio   7       84      1\n";
        ofs << "memory  12      126     1\n";
        ofs << "devices 9       84      1\n";
        ofs << "freezer 11      5       1\n";
        ofs << "net_cls 13      5       1\n";
        ofs << "perf_event      4       5       1\n";
        ofs << "net_prio        13      5       1\n";
        ofs << "hugetlb 8       5       1\n";
        ofs << "pids    5       92      1\n";
        ofs << "ioasids 2       1       1\n";
        ofs << "rdma    6       5       1\n";
        ofs.close();
        PROCESS_DIR = ".";
    }
};

void CgroupCollectorUnittest::TestCollectNormal() const {
    auto collector = CgroupCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectConfig(CgroupCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));

    vector<string> expected_names = {"cgroup_cpuset",
                                     "cgroup_cpu",
                                     "cgroup_cpuacct",
                                     "cgroup_blkio",
                                     "cgroup_memory",
                                     "cgroup_devices",
                                     "cgroup_freezer",
                                     "cgroup_net_cls",
                                     "cgroup_perf_event",
                                     "cgroup_net_prio",
                                     "cgroup_hugetlb",
                                     "cgroup_pids",
                                     "cgroup_ioasids",
                                     "cgroup_rdma"};
    vector<double> expected_values = {
        5, // cgroup_cpuset
        89, // cgroup_cpu
        89, // cgroup_cpuacct
        84, // cgroup_blkio
        126, // cgroup_memory
        84, // cgroup_devices
        5, // cgroup_freezer
        5, // cgroup_net_cls
        5, // cgroup_perf_event - 修正：应该是5，不是4
        5, // cgroup_net_prio - 修正：应该是5，不是13
        5, // cgroup_hugetlb
        92, // cgroup_pids
        1, // cgroup_ioasids
        5, // cgroup_rdma
    };

    // 创建name到value的映射关系，确保一一对应
    std::unordered_map<string, double> expected_name_value_map;
    for (size_t i = 0; i < expected_names.size(); ++i) {
        expected_name_value_map[expected_names[i]] = expected_values[i];
    }

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;

    // 验证所有期望的指标都存在
    for (const auto& expected_name : expected_names) {
        APSARA_TEST_TRUE(maps.find(expected_name) != maps.end());
    }

    // 验证每个指标的值都正确
    for (const auto& [name, expected_value] : expected_name_value_map) {
        APSARA_TEST_TRUE(maps.find(name) != maps.end());
        double actual_value = maps[name].Value;
        APSARA_TEST_EQUAL_FATAL(expected_value, actual_value);
    }
}

UNIT_TEST_CASE(CgroupCollectorUnittest, TestCollectNormal);

} // namespace logtail

UNIT_TEST_MAIN
