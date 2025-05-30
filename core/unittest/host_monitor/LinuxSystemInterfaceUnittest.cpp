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

#include <fstream>

#include "host_monitor/Constants.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class LinuxSystemInterfaceUnittest : public testing::Test {
public:
    void TestGetSystemInformationOnce() const;
    void TestGetCPUInformationOnce() const;
    void TestGetProcessListInformationOnce() const;
    void TestGetProcessInformationOnce() const;

protected:
    void SetUp() override {
        bfs::create_directories("./1");
        ofstream ofs1("./stat", std::ios::trunc);
        ofs1 << "btime 1731142542\n";
        ofs1 << "cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0\n";
        ofs1 << "cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 1 2\n";
        ofs1 << "cpua a b c d e f 424744 0 1 2\n";
        ofs1 << "cpu1 14708487 14216 4613031 2108180843\n"; // test old linux kernel
        ofs1 << "cpu3 14708487 14216 4613031 2108180843"; // test old linux kernel
        ofs1.close();

        PROCESS_DIR = ".";
        bfs::create_directories("./1");
        ofstream ofs2("./1/stat", std::ios::trunc);
        ofs2 << "1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 "
                "4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 "
                "140727020027760 140727020027777 140727020027777 140727020027887 0";
        ofs2.close();
    }

    void TearDown() override {
        bfs::remove_all("./1");
        bfs::remove_all("./stat");
    }
};

void LinuxSystemInterfaceUnittest::TestGetSystemInformationOnce() const {
    SystemInformation systemInfo;
    LinuxSystemInterface::GetInstance()->GetSystemInformationOnce(systemInfo);
    APSARA_TEST_TRUE_FATAL(systemInfo.collectTime.time_since_epoch().count() > 0);
    APSARA_TEST_EQUAL_FATAL(systemInfo.bootTime, 1731142542);
};

void LinuxSystemInterfaceUnittest::TestGetCPUInformationOnce() const {
    CPUInformation cpuInfo;
    LinuxSystemInterface::GetInstance()->GetCPUInformationOnce(cpuInfo);
    APSARA_TEST_TRUE_FATAL(cpuInfo.collectTime.time_since_epoch().count() > 0);
    auto cpus = cpuInfo.stats;
    APSARA_TEST_EQUAL_FATAL(4, cpus.size());
    APSARA_TEST_EQUAL_FATAL(-1, cpus[0].index);
    APSARA_TEST_EQUAL_FATAL(1195061569, cpus[0].user);
    APSARA_TEST_EQUAL_FATAL(1728645, cpus[0].nice);
    APSARA_TEST_EQUAL_FATAL(418424132, cpus[0].system);
    APSARA_TEST_EQUAL_FATAL(203670447952, cpus[0].idle);
    APSARA_TEST_EQUAL_FATAL(14723544, cpus[0].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].irq);
    APSARA_TEST_EQUAL_FATAL(773400, cpus[0].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].guestNice);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[1].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[1].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[1].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[1].idle);
    APSARA_TEST_EQUAL_FATAL(57199, cpus[1].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].irq);
    APSARA_TEST_EQUAL_FATAL(424744, cpus[1].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].steal);
    APSARA_TEST_EQUAL_FATAL(1, cpus[1].guest);
    APSARA_TEST_EQUAL_FATAL(2, cpus[1].guestNice);
    APSARA_TEST_EQUAL_FATAL(1, cpus[2].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[2].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[2].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[2].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[2].idle);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].irq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].guestNice);
    APSARA_TEST_EQUAL_FATAL(3, cpus[3].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[3].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[3].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[3].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[3].idle);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].irq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].guestNice);
};

void LinuxSystemInterfaceUnittest::TestGetProcessListInformationOnce() const {
    ProcessListInformation processListInfo;
    LinuxSystemInterface::GetInstance()->GetProcessListInformationOnce(processListInfo);
    APSARA_TEST_TRUE_FATAL(processListInfo.collectTime.time_since_epoch().count() > 0);
    APSARA_TEST_EQUAL_FATAL(1, processListInfo.pids.size());
    APSARA_TEST_EQUAL_FATAL(1, processListInfo.pids[0]);
};

void LinuxSystemInterfaceUnittest::TestGetProcessInformationOnce() const {
    ProcessInformation processInfo;
    LinuxSystemInterface::GetInstance()->GetProcessInformationOnce(1, processInfo);
    APSARA_TEST_TRUE_FATAL(processInfo.collectTime.time_since_epoch().count() > 0);
    APSARA_TEST_EQUAL_FATAL(1, processInfo.stat.pid);
    APSARA_TEST_EQUAL_FATAL("cat", processInfo.stat.name);
    APSARA_TEST_EQUAL_FATAL('R', processInfo.stat.state);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.parentPid);
    APSARA_TEST_EQUAL_FATAL(34816, processInfo.stat.tty);
    APSARA_TEST_EQUAL_FATAL(1110, processInfo.stat.minorFaults);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.majorFaults);
    APSARA_TEST_EQUAL_FATAL(20, processInfo.stat.priority);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.nice);
    APSARA_TEST_EQUAL_FATAL(1, processInfo.stat.numThreads);
    APSARA_TEST_EQUAL_FATAL(171, processInfo.stat.rss);
};

UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetSystemInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetCPUInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessListInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessInformationOnce);

} // namespace logtail

UNIT_TEST_MAIN
