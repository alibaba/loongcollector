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
#include "host_monitor/collector/ProcessCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ProcessCollectorUnittest : public testing::Test {
public:
    void TestGetHostPidStat() const;
    void TestCollect() const;

protected:
    void SetUp() override {
        // /proc/pid/status
        ofstream ofs("./12345/status", std::ios::trunc);
        ofs << "Name:   ilogtail\n";
        ofs << "Umask:  0022\n";
        ofs << "State:  S (sleeping)\n";
        ofs << "Tgid:   1813\n";
        ofs << "Ngid:   0\n";
        ofs << "Pid:    1813\n";
        ofs << "PPid:   1811\n";
        ofs << "TracerPid:      0\n";
        ofs << "Uid:    0       0       0       0\n";
        ofs << "Gid:    0       0       0       0\n";
        ofs << "FDSize: 64\n";
        ofs << "Groups: \n";
        ofs << "NStgid: 1813\n";
        ofs << "NSpid:  1813\n";
        ofs << "NSpgid: 1811\n";
        ofs << "NSsid:  1811\n";
        ofs << "VmPeak:  1667952 kB\n";
        ofs << "VmSize:  1667952 kB\n";
        ofs << "VmLck:         0 kB\n";
        ofs << "VmPin:         0 kB\n";
        ofs << "VmHWM:    195944 kB\n";
        ofs << "VmRSS:    186632 kB\n";
        ofs << "RssAnon:          132640 kB\n";
        ofs << "RssFile:           53992 kB\n";
        ofs << "RssShmem:              0 kB\n";
        ofs << "VmData:   640112 kB\n";
        ofs << "VmStk:       132 kB\n";
        ofs << "VmExe:     48244 kB\n";
        ofs << "VmLib:     45532 kB\n";
        ofs << "VmPTE:       776 kB\n";
        ofs << "VmSwap:        0 kB\n";
        ofs << "HugetlbPages:          0 kB\n";
        ofs << "CoreDumping:    0\n";
        ofs << "THP_enabled:    1\n";
        ofs << "Threads:        55\n";
        ofs << "SigQ:   84/123074\n";
        ofs << "SigPnd: 0000000000000000\n";
        ofs << "ShdPnd: 0000000000000000\n";
        ofs << "SigBlk: 0000000000010000\n";
        ofs << "SigIgn: 0000000000000000\n";
        ofs << "SigCgt: 00000001804154e2\n";
        ofs << "CapInh: 0000000000000000\n";
        ofs << "CapPrm: 000001ffffffffff\n";
        ofs << "CapEff: 000001ffffffffff\n";
        ofs << "CapBnd: 000001ffffffffff\n";
        ofs << "CapAmb: 0000000000000000\n";
        ofs << "NoNewPrivs:     0\n";
        ofs << "Seccomp:        0\n";
        ofs << "Seccomp_filters:        0\n";
        ofs << "Speculation_Store_Bypass:       vulnerable\n";
        ofs << "Cpus_allowed:   ffffffff\n";
        ofs << "Cpus_allowed_list:      0-31\n";
        ofs << "Mems_allowed:   00000000,00000001\n";
        ofs << "Mems_allowed_list:      0\n";
        ofs << "voluntary_ctxt_switches:        1129507\n";
        ofs << "nonvoluntary_ctxt_switches:     3\n";
        ofs.close();
        // /proc/pid/statm
        ofstream ofs("./12345/statm", std::ios::trunc);
        ofs << "416988 46661 13498 12061 0 160061 0\n";
        ofs.close();
        // /proc/pid/stat
        ofstream ofs("./12345/stat", std::ios::trunc);
        ofs << "1813 (ilogtail) S 1811 1811 1811 0 -1 1077936192 1378102 0 848 0 643169 334268 0 0 20 0 55 0 1304 1707982848 46314 18446744073709551615 4227072 53627809 140730946407792 0 0 0 65536 0 4281570 0 0 0 17 26 0 0 24 0 0 66246848 67456896 101158912 140730946416312 140730946416341 140730946416341 140730946416603 0";
        ofs.close();
        PROCESS_DIR = ".";
    }
};

void ProcessCollectorUnittest::TestGetHostPidStat() const {
    auto collector = ProcessCollector();
    ProcessAllStat stat;
    APSARA_TEST_TRUE(collector.GetProcessAllStat(pid, stat));
    
}

void ProcessCollectorUnittest::TestCollect() const {
    auto collector = ProcessCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectConfig(ProcessCollector::sName, 0, 0, std::chrono::seconds(1));

    std::cout << group.GetEvents().size() << std::endl;
    APSARA_TEST_TRUE(collector.Collect(collectConfig, &group));
    APSARA_TEST_EQUAL_FATAL(3 * 10, group.GetEvents().size());
    
}

UNIT_TEST_CASE(ProcessCollectorUnittest, TestGetHostSystemCPUStat);
UNIT_TEST_CASE(ProcessCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN
