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
// Authors: Wardenjohn <zhangwarden@gmail.com>

#include "MetricEvent.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorTimerEvent.h"
#include "host_monitor/collector/MemCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class MemCollectorUnittest : public testing::Test {
public:
    void TestGetHostSystemMeminfoStat() const;
    void TestCollect() const;

protected:
    void SetUp() override {
        ofstream ofs("./meminfo", std::ios::trunc);
        ofs << "MemTotal:       31534908 kB\n";
        ofs << "MemFree:        13226912 kB\n";
        ofs << "MemAvailable:   28771376 kB\n";
        ofs << "Buffers:          280412 kB\n";
        ofs << "Cached:         14919736 kB\n";
        ofs << "SwapCached:            0 kB\n";
        ofs << "Active:          3868640 kB\n";
        ofs << "Inactive:       13282300 kB\n";
        ofs << "Active(anon):        772 kB\n";
        ofs << "Inactive(anon):  1952588 kB\n";
        ofs << "Active(file):    3867868 kB\n";
        ofs << "Inactive(file): 11329712 kB\n";
        ofs << "Unevictable:           0 kB\n";
        ofs << "Mlocked:               0 kB\n";
        ofs << "SwapTotal:             0 kB\n";
        ofs << "SwapFree:              0 kB\n";
        ofs << "Dirty:                 0 kB\n";
        ofs << "Writeback:             0 kB\n";
        ofs << "AnonPages:       1784516 kB\n";
        ofs << "Mapped:           394160 kB\n";
        ofs << "Shmem:              2568 kB\n";
        ofs << "KReclaimable:     803156 kB\n";
        ofs << "Slab:             899560 kB\n";
        ofs << "SReclaimable:     803156 kB\n";
        ofs << "SUnreclaim:        96404 kB\n";
        ofs << "KernelStack:        9904 kB\n";
        ofs << "PageTables:        29252 kB\n";
        ofs << "NFS_Unstable:          0 kB\n";
        ofs << "Bounce:                0 kB\n";
        ofs << "WritebackTmp:          0 kB\n";
        ofs << "CommitLimit:    15767452 kB\n";
        ofs << "Committed_AS:    4265992 kB\n";
        ofs << "VmallocTotal:   34359738367 kB\n";
        ofs << "VmallocUsed:       18148 kB\n";
        ofs << "VmallocChunk:          0 kB\n";
        ofs << "Percpu:            15872 kB\n";
        ofs << "HardwareCorrupted:     0 kB\n";
        ofs << "AnonHugePages:    851968 kB\n";
        ofs << "ShmemHugePages:        0 kB\n";
        ofs << "ShmemPmdMapped:        0 kB\n";
        ofs << "FileHugePages:    364544 kB\n";
        ofs << "FilePmdMapped:    202752 kB\n";
        ofs << "CmaTotal:              0 kB\n";
        ofs << "CmaFree:               0 kB\n";
        ofs << "DupText:               0 kB\n";
        ofs << "MemZeroed:             0 kB\n";
        ofs << "Unaccepted:            0 kB\n";
        ofs << "HugePages_Total:       0\n";
        ofs << "HugePages_Free:        0\n";
        ofs << "HugePages_Rsvd:        0\n";
        ofs << "HugePages_Surp:        0\n";
        ofs << "Hugepagesize:       2048 kB\n";
        ofs << "Hugetlb:               0 kB\n";
        ofs << "DirectMap4k:      357644 kB\n";
        ofs << "DirectMap2M:    11141120 kB\n";
        ofs << "DirectMap1G:    23068672 kB\n";
        ofs.close();
        PROCESS_DIR = ".";
    }
};

void MemCollectorUnittest::TestGetHostSystemMeminfoStat() const {
    auto collector = MemCollector();
    MemoryInformation memInfoStat;
    SwapInformation swapStat;
    APSARA_TEST_TRUE(collector.GetHostMeminfoStat(memInfoStat, swapStat));
    APSARA_TEST_EQUAL_FATAL((int64_t)31534908*1024, (int64_t)memInfoStat.total);
    APSARA_TEST_EQUAL_FATAL((int64_t)13226912*1024, (int64_t)memInfoStat.free);
    APSARA_TEST_EQUAL_FATAL((int64_t)28771376*1024, (int64_t)memInfoStat.available);
    APSARA_TEST_EQUAL_FATAL((int64_t)28771376*1024, (int64_t)memInfoStat.actualFree);
    APSARA_TEST_EQUAL_FATAL((int64_t)(31534908-28771376)*1024, (int64_t)memInfoStat.actualUsed);
    APSARA_TEST_EQUAL_FATAL((static_cast<double>(31534908-28771376)/static_cast<double>(31534908))*100.0, static_cast<double>(memInfoStat.usedPercent));
    double total=31534908.0;
    double actual_free = 28771376.0;
    double free_percent = actual_free*100.0/total;
    APSARA_TEST_EQUAL_FATAL(free_percent, static_cast<double>(memInfoStat.freePercent));
}

void MemCollectorUnittest::TestCollect() const {
    auto collector = MemCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectConfig(MemCollector::sName, 0, 0, std::chrono::seconds(1));

    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));
    double total=31534908.0;
    double actual_free = 28771376.0;
    double free_percent = actual_free*100.0/total;
    vector<double> expectedValues = {(31534908-28771376)*1024.0,
                                (31534908-28771376)*1024.0,
                                (static_cast<double>(31534908-28771376)/static_cast<double>(31534908))*100.0,
                                free_percent};

    vector<string> expectedNames = {
        "memory_usedspace",
        "memory_actualusedspace", 
        "memory_usedutilization", 
        "memory_freeutilization",
    };

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
    for (size_t i = 0; i < 16; ++i) {
        APSARA_TEST_TRUE(maps.find(expectedNames[i]) != maps.end());
        double val = maps[expectedNames[i]].Value;
        EXPECT_NEAR(expectedValues[static_cast<size_t>(i / 3)], val, 1e-6);
    }

}

UNIT_TEST_CASE(MemCollectorUnittest, TestGetHostSystemMeminfoStat);
UNIT_TEST_CASE(MemCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN