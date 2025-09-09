/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <typeinfo>

#include "MetricEvent.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/collector/NetCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class NetCollectorUnittest : public ::testing::Test {
public:
    void TestCollect() const;
    void TestIpv6FileNoExist() const;
    void TestDevNoExist() const;

protected:
    void SetUp() override {
        // sockets: used 316
        // TCP: inuse 25 orphan 0 tw 2 alloc 28 mem 4
        // UDP: inuse 3 mem 0
        // UDPLITE: inuse 0
        // RAW: inuse 0
        // FRAG: inuse 0 memory 0
        std::filesystem::create_directories("./net");
        ofstream ofs1("./net/sockstat", std::ios::trunc);
        ofs1 << "sockets: used 316\n";
        ofs1 << "TCP: inuse 25 orphan 0 tw 2 alloc 28 mem 4\n";
        ofs1 << "UDP: inuse 3 mem 0\n";
        ofs1 << "UDPLITE: inuse 0\n";
        ofs1 << "RAW: inuse 0\n";
        ofs1 << "FRAG: inuse 0 memory 0\n";
        ofs1.close();

        // TCP6: inuse 2
        // UDP6: inuse 2
        // UDPLITE6: inuse 0
        // RAW6: inuse 0
        // FRAG6: inuse 0 memory 0
        ofstream ofs2("./net/sockstat6", std::ios::trunc);
        ofs2 << "TCP6: inuse 2\n";
        ofs2 << "UDP6: inuse 2\n";
        ofs2 << "UDPLITE6: inuse 0\n";
        ofs2 << "RAW6: inuse 0\n";
        ofs2 << "FRAG6: inuse 0 memory 0\n";
        ofs2.close();

        // Inter-|   Receive                                                |  Transmit
        //  face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls
        //  carrier compressed
        //     lo: 1538516774 9633892    0    0    0     0          0         0 1538516774 9633892    0    0    0     0
        //     0          0
        //   eth0: 9338508096 24973536    0    0    0     0          0         0 42362852159 11767669    0    0    0 0
        //   0          0
        // docker0: 96663341  195219    0    0    0     0          0         0 155828048  161266    0    0    0     0 0
        // 0 veth6c3a07a:  188547     695    0    0    0     0          0         0   274800    1314    0    0    0 0 0
        // 0 vethc4371db: 99107500  194212    0    0    0     0          0         0 155543068  161069    0    0    0 0
        // 0          0
        ofstream ofs3("./net/dev", std::ios::trunc);
        ofs3 << "Inter-|   Receive                                                |  Transmit\n";
        ofs3 << " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo "
                "colls carrier compressed\n";
        ofs3 << "     lo: 1538516774 9633892    0    0    0     0          0         0 1538516774 9633892    0    0    "
                "0     0       0          0\n";
        // ofs3 << "   eth0: 9338508096 24973536    0    0    0     0          0         0 42362852159 11767669    0 0 "
        //         "   0     0       0          0\n";

        ofs3.close();

        // fe80000000000000004279fffe4bbfe3 03 40 20 80  docker0
        // fe8000000000000002163efffe250097 02 40 20 80     eth0
        // 00000000000000000000000000000001 01 80 10 80       lo
        // fe800000000000009c156afffe89f929 05 40 20 80 veth1305bc9
        ofstream ofs4("./net/if_inet6");
        // ofs4 << "fe8000000000000002163efffe250097 02 40 20 80     eth0";
        ofs4 << "00000000000000000000000000000001 01 80 10 80       lo";
        PROCESS_DIR = ".";
    }
};

void NetCollectorUnittest::TestCollect() const {
    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;
    NetCollector collector = NetCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto netCollector = std::make_unique<NetCollector>();
    HostMonitorContext collectconfig("test",
                                     NetCollector::sName,
                                     QueueKey{},
                                     0,
                                     std::chrono::seconds(1),
                                     CollectorInstance(std::move(netCollector)));
    collectconfig.mCountPerReport = 3;

    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));

    APSARA_TEST_EQUAL_FATAL(5UL, group.GetEvents().size());

    vector<string> device_names = {
        // "eth0",
        "lo",
    };

    vector<string> rate_names = {
        "networkin_droppackages_avg",   "networkin_droppackages_max",   "networkin_droppackages_min",
        "networkin_errorpackages_avg",  "networkin_errorpackages_max",  "networkin_errorpackages_min",
        "networkin_rate_avg",           "networkin_rate_max",           "networkin_rate_min",
        "networkin_packages_avg",       "networkin_packages_max",       "networkin_packages_min",
        "networkout_droppackages_avg",  "networkout_droppackages_max",  "networkout_droppackages_min",
        "networkout_errorpackages_avg", "networkout_errorpackages_max", "networkout_errorpackages_min",
        "networkout_packages_avg",      "networkout_packages_max",      "networkout_packages_min",
        "networkout_rate_avg",          "networkout_rate_max",          "networkout_rate_min",
    };

    for (size_t j = 0; j < device_names.size(); j++) {
        auto event = group.GetEvents()[j].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(device_names[j], event.GetTag("device"));
        APSARA_TEST_EQUAL_FATAL(hostname, event.GetTag("hostname"));
        APSARA_TEST_EQUAL_FATAL(std::string("system.net_original"), event.GetTag("m"));
        for (size_t i = 0; i < rate_names.size(); ++i) {
            APSARA_TEST_TRUE(maps.find(rate_names[i]) != maps.end());
            EXPECT_NEAR(0.0, maps[rate_names[i]].Value, 1e-6);
        }
    }

    vector<string> tcp_names = {
        "LISTEN",
        "ESTABLISHED",
        "NON_ESTABLISHED",
        "TCP_TOTAL",
    };
    vector<string> tcp_cnt_names = {
        "net_tcpconnection_avg",
        "net_tcpconnection_max",
        "net_tcpconnection_min",
    };
    for (size_t j = 0; j < tcp_names.size(); j++) {
        auto event = group.GetEvents()[j + device_names.size()].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(tcp_names[j], event.GetTag("state"));
        APSARA_TEST_EQUAL_FATAL(std::string("system.tcp"), event.GetTag("m"));
        for (size_t i = 0; i < tcp_cnt_names.size(); ++i) {
            APSARA_TEST_TRUE(maps.find(tcp_cnt_names[i]) != maps.end());
        }
    }
}

void NetCollectorUnittest::TestIpv6FileNoExist() const {
    // 删除单个文件
    std::error_code ec;
    bool success = std::filesystem::remove("./net/sockstat6", ec);
    if (!success && ec) {
        // 处理错误，比如文件不存在或权限不足
        std::cerr << "Failed to delete file: " << ec.message() << std::endl;
    }

    success = std::filesystem::remove("./net/if_inet6", ec);
    if (!success && ec) {
        // 处理错误，比如文件不存在或权限不足
        std::cerr << "Failed to delete file: " << ec.message() << std::endl;
    }

    uint64_t tcp;
    APSARA_TEST_FALSE(LinuxSystemInterface::GetInstance()->ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT6, tcp));

    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;
    NetCollector collector = NetCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    auto netCollector = std::make_unique<NetCollector>();
    HostMonitorContext collectconfig("test",
                                     NetCollector::sName,
                                     QueueKey{},
                                     0,
                                     std::chrono::seconds(1),
                                     CollectorInstance(std::move(netCollector)));
    collectconfig.mCountPerReport = 3;

    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));

    APSARA_TEST_EQUAL_FATAL(5UL, group.GetEvents().size());

    vector<string> device_names = {
        // "eth0",
        "lo",
    };

    vector<string> rate_names = {
        "networkin_droppackages_avg",   "networkin_droppackages_max",   "networkin_droppackages_min",
        "networkin_errorpackages_avg",  "networkin_errorpackages_max",  "networkin_errorpackages_min",
        "networkin_rate_avg",           "networkin_rate_max",           "networkin_rate_min",
        "networkin_packages_avg",       "networkin_packages_max",       "networkin_packages_min",
        "networkout_droppackages_avg",  "networkout_droppackages_max",  "networkout_droppackages_min",
        "networkout_errorpackages_avg", "networkout_errorpackages_max", "networkout_errorpackages_min",
        "networkout_packages_avg",      "networkout_packages_max",      "networkout_packages_min",
        "networkout_rate_avg",          "networkout_rate_max",          "networkout_rate_min",
    };

    for (size_t j = 0; j < device_names.size(); j++) {
        auto event = group.GetEvents()[j].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(device_names[j], event.GetTag("device"));
        APSARA_TEST_EQUAL_FATAL(hostname, event.GetTag("hostname"));
        APSARA_TEST_EQUAL_FATAL(std::string("system.net_original"), event.GetTag("m"));
        for (size_t i = 0; i < rate_names.size(); ++i) {
            APSARA_TEST_TRUE(maps.find(rate_names[i]) != maps.end());
            EXPECT_NEAR(0.0, maps[rate_names[i]].Value, 1e-6);
        }
    }

    vector<string> tcp_names = {
        "LISTEN",
        "ESTABLISHED",
        "NON_ESTABLISHED",
        "TCP_TOTAL",
    };
    vector<string> tcp_cnt_names = {
        "net_tcpconnection_avg",
        "net_tcpconnection_max",
        "net_tcpconnection_min",
    };
    for (size_t j = 0; j < tcp_names.size(); j++) {
        auto event = group.GetEvents()[j + device_names.size()].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(tcp_names[j], event.GetTag("state"));
        APSARA_TEST_EQUAL_FATAL(std::string("system.tcp"), event.GetTag("m"));
        for (size_t i = 0; i < tcp_cnt_names.size(); ++i) {
            APSARA_TEST_TRUE(maps.find(tcp_cnt_names[i]) != maps.end());
        }
    }
}

void NetCollectorUnittest::TestDevNoExist() const {
    InterfaceConfig ifConfig;
    std::string devName = "devNoExist";
    APSARA_TEST_FALSE(LinuxSystemInterface::GetInstance()->GetInterfaceConfig(ifConfig, devName));
}

UNIT_TEST_CASE(NetCollectorUnittest, TestCollect);
UNIT_TEST_CASE(NetCollectorUnittest, TestIpv6FileNoExist);
UNIT_TEST_CASE(NetCollectorUnittest, TestDevNoExist);

} // namespace logtail

UNIT_TEST_MAIN
