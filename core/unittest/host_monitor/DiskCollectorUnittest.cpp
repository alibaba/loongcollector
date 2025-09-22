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

#include <chrono>

#include "MetricEvent.h"
#include "common/FileSystemUtil.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/DiskCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class DiskCollectorUnittest : public testing::Test {
public:
    void TestGetFileSystemInfos() const;
    void TestGetSystemUptimeInformation() const;
    void TestGetDiskSerialIdInformation() const;
    void GetDiskStateInformation() const;
    void TestCollect() const;

protected:
    void SetUp() override {
        ofstream ofs("./mtab", std::ios::trunc);
        ofs << "none /sys/kernel/tracing tracefs rw,relatime 0 0\n";
        ofs << "/dev/vda1test / ext4 rw,relatime 0 0n";
        ofs << "debugfs /sys/kernel/debug debugfs rw,relatime 0 0\n";
        ofs << "/dev/vdb /home/shiyan/workspace ext4 rw,relatime 0 0\n";
        ofs << "sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0\n";
        ofs << "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n";
        ofs.close();
        ETC_DIR = ".";
        system("sudo mknod /dev/vda1test b 253 1");
    }
};

void DiskCollectorUnittest::TestGetFileSystemInfos() const {
    auto collector = DiskCollector();
    std::vector<FileSystemInfo> fileSystemInfos;
    bool hasVda1 = false;
    bool hasVdb = false;

    APSARA_TEST_EQUAL_FATAL(
        0, collector.GetFileSystemInfos(CollectTime{std::chrono::steady_clock::now(), time(nullptr)}, fileSystemInfos));
    for (auto& fileSystem : fileSystemInfos) {
        if (fileSystem.type != "ext4") {
            continue;
        }

        if (fileSystem.devName == "/dev/vda1test") {
            hasVda1 = true;
            APSARA_TEST_EQUAL_FATAL("ext4", fileSystem.type);
        } else if (fileSystem.devName == "/dev/vdb") {
            hasVdb = true;
            APSARA_TEST_EQUAL_FATAL("ext4", fileSystem.type);
        }
    }
    APSARA_TEST_EQUAL_FATAL(true, hasVda1);
    APSARA_TEST_EQUAL_FATAL(true, hasVdb);
}

void DiskCollectorUnittest::TestGetSystemUptimeInformation() const {
    SystemUptimeInformation systemUptimeInfo;

    ofstream ofs1("./uptime", std::ios::trunc);
    ofs1 << "63267496.31 497986327.21\n";
    ofs1.close();
    PROCESS_DIR = ".";

    APSARA_TEST_EQUAL_FATAL(
        true, SystemInterface::GetInstance()->GetSystemUptimeInformation(time(nullptr), systemUptimeInfo));
    APSARA_TEST_EQUAL_FATAL(63267496.31, systemUptimeInfo.uptime);
}

void DiskCollectorUnittest::TestGetDiskSerialIdInformation() const {
    bfs::create_directories("./vdb");
    std::string diskName = "vdb";
    SerialIdInformation serialIdInfo;

    ofstream ofs2("./vdb/serial", std::ios::trunc);
    ofs2 << "12345abcde\n";
    ofs2.close();
    SYSTEM_BLOCK_DIR = ".";

    APSARA_TEST_EQUAL_FATAL(
        true, SystemInterface::GetInstance()->GetDiskSerialIdInformation(time(nullptr), diskName, serialIdInfo));
    APSARA_TEST_EQUAL_FATAL("12345abcde", serialIdInfo.serialId);
}

void DiskCollectorUnittest::GetDiskStateInformation() const {
    DiskStateInformation diskStateInfo;
    bool hasVda = false;
    bool hasVda1 = false;
    bool hasVdb = false;

    ofstream ofs3("./diskstats", std::ios::trunc);
    ofs3 << " 253       0 vda 7658551 323100 586387169 319931079 1424590181 625148386 29125354328 1204074948 0 "
            "334309088 1529448238 0 0 0 0 260594053 5442210 316424708 1146612591 0\n";
    ofs3 << " 253       1 vda1test 7657100 323100 586381881 319930901 1406241480 625148386 29125354320 1203581072 0 "
            "333741551 1523511973 0 0 0 0 0 0 316424556 1146612588 0\n";
    ofs3 << " 253      16 vdb 3305819 107375 388166122 5034099 2648810 539427 440272264 6734790 0 1424765 11773835 0 0 "
            "0 0 117915 4945 4832520 6527109 0\n";
    ofs3.close();
    PROCESS_DIR = ".";

    APSARA_TEST_EQUAL_FATAL(true,
                            SystemInterface::GetInstance()->GetDiskStateInformation(time(nullptr), diskStateInfo));
    for (auto const& diskState : diskStateInfo.diskStats) {
        if (diskState.major == 253 && diskState.minor == 0) {
            hasVda = true;
            APSARA_TEST_EQUAL_FATAL((int64_t)7658551, (int64_t)diskState.reads);
            APSARA_TEST_EQUAL_FATAL((int64_t)29125354328 * 512, (int64_t)diskState.writeBytes);
            APSARA_TEST_EQUAL_FATAL((int64_t)1529448238, (int64_t)diskState.qTime);
        }
        if (diskState.major == 253 && diskState.minor == 1) {
            hasVda1 = true;
            APSARA_TEST_EQUAL_FATAL((int64_t)7657100, (int64_t)diskState.reads);
            APSARA_TEST_EQUAL_FATAL((int64_t)29125354320 * 512, (int64_t)diskState.writeBytes);
            APSARA_TEST_EQUAL_FATAL((int64_t)1523511973, (int64_t)diskState.qTime);
        }
        if (diskState.major == 253 && diskState.minor == 16) {
            hasVdb = true;
            APSARA_TEST_EQUAL_FATAL((int64_t)3305819, (int64_t)diskState.reads);
            APSARA_TEST_EQUAL_FATAL((int64_t)440272264 * 512, (int64_t)diskState.writeBytes);
            APSARA_TEST_EQUAL_FATAL((int64_t)11773835, (int64_t)diskState.qTime);
        }
    }
    APSARA_TEST_EQUAL_FATAL(true, hasVda1);
    APSARA_TEST_EQUAL_FATAL(true, hasVdb);
    APSARA_TEST_EQUAL_FATAL(true, hasVda);
}

void DiskCollectorUnittest::TestCollect() const {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    bfs::create_directories("./vdb");
    bfs::create_directories("./vda");
    PROCESS_DIR = ".";
    auto collector = DiskCollector();

    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto diskCollector = std::make_unique<DiskCollector>();
    HostMonitorContext collectContext("test",
                                      DiskCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(1),
                                      CollectorInstance(std::move(diskCollector)));
    collectContext.mCountPerReport = 4;
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));

    ofstream ofs10("./vdb/serial", std::ios::trunc);
    ofs10 << "vdb12345abcde\n";
    ofs10.close();
    SYSTEM_BLOCK_DIR = ".";

    ofstream ofs11("./vda/serial", std::ios::trunc);
    ofs11 << "vda12345abcde\n";
    ofs11.close();
    SYSTEM_BLOCK_DIR = ".";

    ofstream ofs2("./diskstats", std::ios::trunc);
    ofs2 << " 253       0 vda 7658551 323100 586387169 319931079 1424590181 625148386 29125354328 1204074948 0 "
            "334309088 1529448238 0 0 0 0 260594053 5442210 316424708 1146612591 0\n";
    ofs2 << " 253       1 vda1test 7657103 323103 586381884 319930904 1406241483 625148389 29125354323 12035810735 3 "
            "333741554 1523511976 0 0 0 0 0 0 316424556 1146612588 0\n";
    ofs2 << " 253      16 vdb 3305822 107378 388166125 5034102 2648813 539430 440272267 6734793 3 1424768 11773838 3 3 "
            "0 0 117915 4945 4832520 6527109 0\n";
    ofs2.close();
    ofstream ofs3("./uptime", std::ios::trunc);
    ofs3 << "63267511.31 497986342.21\n";
    ofs3.close();
    PROCESS_DIR = ".";

    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    ofstream ofs4("./diskstats", std::ios::trunc);
    ofs4 << " 253       0 vda 7658551 323100 586387169 319931079 1424590181 625148386 29125354328 1204074948 0 "
            "334309088 1529448238 0 0 0 0 260594053 5442210 316424708 1146612591 0\n";
    ofs4 << " 253       1 vda1test 7657101 323101 586381882 319930902 1406241481 625148387 29125354321 1203581073 1 "
            "333741552 1523511974 0 0 0 0 0 0 316424556 1146612588 0\n";
    ofs4 << " 253      16 vdb 3305820 107376 388166123 5034100 2648811 539428 440272265 6734791 1 1424766 11773836 1 1 "
            "0 0 117915 4945 4832520 6527109 0\n";
    ofs4.close();

    ofstream ofs6("./uptime", std::ios::trunc);
    ofs6 << "63267501.31 497986332.21\n";
    ofs6.close();

    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait system interface cache stale
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    ofstream ofs5("./diskstats", std::ios::trunc);
    ofs5 << " 253       0 vda 7658551 323100 586387169 319931079 1424590181 625148386 29125354328 1204074948 0 "
            "334309088 1529448238 0 0 0 0 260594053 5442210 316424708 1146612591 0\n";
    ofs5 << " 253       1 vda1test 7657102 323102 586381883 319930903 1406241482 625148388 29125354322 12035810734 2 "
            "333741553 1523511975 0 0 0 0 0 0 316424556 1146612588 0\n";
    ofs5 << " 253      16 vdb 3305821 107377 388166124 5034101 2648812 539429 440272266 6734792 2 1424767 11773837 2 2 "
            "0 0 117915 4945 4832520 6527109 0\n";
    ofs5.close();

    ofstream ofs7("./uptime", std::ios::trunc);
    ofs7 << "63267506.31 497986337.21\n";
    ofs7.close();

    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait system interface cache stale
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    ofstream ofs8("./diskstats", std::ios::trunc);
    ofs8 << " 253       0 vda 7658551 323100 586387169 319931079 1424590181 625148386 29125354328 1204074948 0 "
            "334309088 1529448238 0 0 0 0 260594053 5442210 316424708 1146612591 0\n";
    ofs8 << " 253       1 vda1test 7657103 323103 586381884 319930904 1406241483 625148389 29125354323 12035810735 3 "
            "333741554 1523511976 0 0 0 0 0 0 316424556 1146612588 0\n";
    ofs8 << " 253      16 vdb 3305822 107378 388166125 5034102 2648813 539430 440272267 6734793 3 1424768 11773838 3 3 "
            "0 0 117915 4945 4832520 6527109 0\n";
    ofs8.close();

    ofstream ofs9("./uptime", std::ios::trunc);
    ofs9 << "63267511.31 497986342.21\n";
    ofs9.close();

    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait system interface cache stale
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

    APSARA_TEST_EQUAL_FATAL(1UL, group.GetEvents().size());

    vector<string> expectedNames = {
        "disk_readiops_avg",
        "disk_readiops_min",
        "disk_readiops_max",
        "disk_writeiops_avg",
        "disk_writeiops_min",
        "disk_writeiops_max",
        "disk_writebytes_avg",
        "disk_writebytes_min",
        "disk_writebytes_max",
        "disk_readbytes_avg",
        "disk_readbytes_min",
        "disk_readbytes_max",
        "DiskIOQueueSize_avg",
        "DiskIOQueueSize_min",
        "DiskIOQueueSize_max",
    };
    vector<double> expected_values = {0.67, 0, 1, 0.67, 0, 1, 341, 0, 512, 341, 0, 512, 0.000133333, 0, 0.0002};
    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;

    for (size_t i = 0; i < expectedNames.size(); i++) {
        APSARA_TEST_TRUE(maps.find(expectedNames[i]) != maps.end());
        double val = maps[expectedNames[i]].Value;
        EXPECT_NEAR(expected_values[static_cast<size_t>(i)], val, 100);
    }
}

UNIT_TEST_CASE(DiskCollectorUnittest, TestGetFileSystemInfos);
UNIT_TEST_CASE(DiskCollectorUnittest, TestGetSystemUptimeInformation);
UNIT_TEST_CASE(DiskCollectorUnittest, TestGetDiskSerialIdInformation);
UNIT_TEST_CASE(DiskCollectorUnittest, GetDiskStateInformation);
UNIT_TEST_CASE(DiskCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN
