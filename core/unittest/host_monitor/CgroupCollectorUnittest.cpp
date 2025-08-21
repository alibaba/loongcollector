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
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/HostMonitorTypes.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/CgroupCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

#include <filesystem>

using namespace std;

namespace logtail {

class CgroupCollectorUnittest : public testing::Test {
public:
  void TestCollectNormal() const;
  void TestCollectNullGroup() const;

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

  void TearDown() override {
    bfs::remove("./cgroups");
    PROCESS_DIR = "/proc";
  }
};

void CgroupCollectorUnittest::TestCollectNormal() const {
  auto cgroupCollector = std::make_unique<CgroupCollector>();
  HostMonitorContext collectContext(
      "test", CgroupCollector::sName, QueueKey{}, 0, std::chrono::seconds(15),
      CollectorInstance(std::move(cgroupCollector)));
  collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
  CgroupCollector collector;
  APSARA_TEST_TRUE(collector.Init(collectContext));
  collectContext.mCountPerReport = 3;

  collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
  // First two calls with nullptr (intermediate collections in kMultiValue mode)
  APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

  collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
  APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

  // Third call with a valid group (ShouldGenerateMetric returns true)
  collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
  PipelineEventGroup group(make_shared<SourceBuffer>());
  APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

  auto event = group.GetEvents()[0].Cast<MetricEvent>();
  auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;

  // Verify all expected metric names exist
  vector<string> expected_names = {
      "cgroup_cpuset",   "cgroup_cpu",     "cgroup_cpuacct",
      "cgroup_blkio",    "cgroup_memory",  "cgroup_devices",
      "cgroup_freezer",  "cgroup_net_cls", "cgroup_perf_event",
      "cgroup_net_prio", "cgroup_hugetlb", "cgroup_pids",
      "cgroup_ioasids",  "cgroup_rdma"};
  for (const auto &name : expected_names) {
    APSARA_TEST_TRUE(maps.find(name) != maps.end());
  }

  // Verify metric values
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_cpuset"].Value);
  APSARA_TEST_EQUAL_FATAL(89.0, maps["cgroup_cpu"].Value);
  APSARA_TEST_EQUAL_FATAL(89.0, maps["cgroup_cpuacct"].Value);
  APSARA_TEST_EQUAL_FATAL(84.0, maps["cgroup_blkio"].Value);
  APSARA_TEST_EQUAL_FATAL(126.0, maps["cgroup_memory"].Value);
  APSARA_TEST_EQUAL_FATAL(84.0, maps["cgroup_devices"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_freezer"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_net_cls"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_perf_event"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_net_prio"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_hugetlb"].Value);
  APSARA_TEST_EQUAL_FATAL(92.0, maps["cgroup_pids"].Value);
  APSARA_TEST_EQUAL_FATAL(1.0, maps["cgroup_ioasids"].Value);
  APSARA_TEST_EQUAL_FATAL(5.0, maps["cgroup_rdma"].Value);

  // Verify tag
  APSARA_TEST_TRUE(event.HasTag("m"));
  APSARA_TEST_EQUAL(string("system.cgroup"), event.GetTag("m"));
}

void CgroupCollectorUnittest::TestCollectNullGroup() const {
  CgroupCollector collector;
  HostMonitorContext collectContext(
      "test", CgroupCollector::sName, QueueKey{}, 0, std::chrono::seconds(15),
      CollectorInstance(std::make_unique<CgroupCollector>()));
  collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
  APSARA_TEST_TRUE(collector.Init(collectContext));
  collectContext.mCountPerReport = 3;

  collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
  // Passing nullptr should return true (intermediate collection, no output
  // needed)
  APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));
}

UNIT_TEST_CASE(CgroupCollectorUnittest, TestCollectNormal);
UNIT_TEST_CASE(CgroupCollectorUnittest, TestCollectNullGroup);

} // namespace logtail

UNIT_TEST_MAIN