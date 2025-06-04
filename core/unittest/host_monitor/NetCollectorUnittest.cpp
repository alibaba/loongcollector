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
#include "host_monitor/collector/NetCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class NetCollectorUnittest : public ::testing::Test {
public:
    void TestGetNetRateInfo() const;
    void TestReadSocketStat() const;
    void TestReadNetLink() const;
    void TestGetNetStateByNetLink() const;
    void TestGetNetTCPInfo() const;
    void TestCollect() const;

protected:
    void SetUp() override{

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
        //  face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
        //     lo: 1538516774 9633892    0    0    0     0          0         0 1538516774 9633892    0    0    0     0       0          0
        //   eth0: 9338508096 24973536    0    0    0     0          0         0 42362852159 11767669    0    0    0     0       0          0
        // docker0: 96663341  195219    0    0    0     0          0         0 155828048  161266    0    0    0     0       0          0
        // veth6c3a07a:  188547     695    0    0    0     0          0         0   274800    1314    0    0    0     0       0          0
        // vethc4371db: 99107500  194212    0    0    0     0          0         0 155543068  161069    0    0    0     0       0          0
        ofstream ofs3("./net/dev", std::ios::trunc);
        ofs3 << "Inter-|   Receive                                                |  Transmit\n";
        ofs3 << " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n";
        ofs3 << "     lo: 1538516774 9633892    0    0    0     0          0         0 1538516774 9633892    0    0    0     0       0          0\n";
        ofs3 << "   eth0: 9338508096 24973536    0    0    0     0          0         0 42362852159 11767669    0    0    0     0       0          0\n";
        ofs3 << " docker0: 96663341  195219    0    0    0     0          0         0 155828048  161266    0    0    0     0       0          0\n";
        ofs3 << " veth6c3a07a:  188547     695    0    0    0     0          0         0   274800    1314    0    0    0     0       0          0\n";
        ofs3 << " vethc4371db: 99107500  194212    0    0    0     0          0         0 155543068  161069    0    0    0     0       0          0\n";
        ofs3.close();
        
        PROCESS_DIR = ".";
    }

};

void NetCollectorUnittest::TestGetNetRateInfo() const {
    NetCollector collector = NetCollector();
    vector<NetInterfaceMetric> metrics;
    APSARA_TEST_TRUE(collector.GetNetRateInfo(metrics));
    APSARA_TEST_EQUAL_FATAL(5, metrics.size());

    APSARA_TEST_EQUAL_FATAL("lo", metrics[0].name);
    APSARA_TEST_EQUAL_FATAL(1538516774, metrics[0].rxBytes);
    APSARA_TEST_EQUAL_FATAL(9633892, metrics[0].rxPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].rxErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].rxDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].rxOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].rxFrame);
    APSARA_TEST_EQUAL_FATAL(1538516774, metrics[0].txBytes);
    APSARA_TEST_EQUAL_FATAL(9633892, metrics[0].txPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].txErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].txDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].txOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].txCollisions);
    APSARA_TEST_EQUAL_FATAL(0, metrics[0].txCarrier);

    APSARA_TEST_EQUAL_FATAL("eth0", metrics[1].name);
    APSARA_TEST_EQUAL_FATAL(9338508096, metrics[1].rxBytes);
    APSARA_TEST_EQUAL_FATAL(24973536, metrics[1].rxPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].rxErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].rxDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].rxOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].rxFrame);
    APSARA_TEST_EQUAL_FATAL(42362852159, metrics[1].txBytes);
    APSARA_TEST_EQUAL_FATAL(11767669, metrics[1].txPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].txErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].txDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].txOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].txCollisions);
    APSARA_TEST_EQUAL_FATAL(0, metrics[1].txCarrier);

    APSARA_TEST_EQUAL_FATAL("docker0", metrics[2].name);
    APSARA_TEST_EQUAL_FATAL(96663341, metrics[2].rxBytes);
    APSARA_TEST_EQUAL_FATAL(195219, metrics[2].rxPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].rxErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].rxDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].rxOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].rxFrame);
    APSARA_TEST_EQUAL_FATAL(155828048, metrics[2].txBytes);
    APSARA_TEST_EQUAL_FATAL(161266, metrics[2].txPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].txErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].txDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].txOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].txCollisions);
    APSARA_TEST_EQUAL_FATAL(0, metrics[2].txCarrier);

    APSARA_TEST_EQUAL_FATAL("veth6c3a07a", metrics[3].name);
    APSARA_TEST_EQUAL_FATAL(188547, metrics[3].rxBytes);
    APSARA_TEST_EQUAL_FATAL(695, metrics[3].rxPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].rxErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].rxDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].rxOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].rxFrame);
    APSARA_TEST_EQUAL_FATAL(274800, metrics[3].txBytes);
    APSARA_TEST_EQUAL_FATAL(1314, metrics[3].txPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].txErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].txDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].txOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].txCollisions);
    APSARA_TEST_EQUAL_FATAL(0, metrics[3].txCarrier);

    APSARA_TEST_EQUAL_FATAL("vethc4371db", metrics[4].name);
    APSARA_TEST_EQUAL_FATAL(99107500, metrics[4].rxBytes);
    APSARA_TEST_EQUAL_FATAL(194212, metrics[4].rxPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].rxErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].rxDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].rxOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].rxFrame);
    APSARA_TEST_EQUAL_FATAL(155543068, metrics[4].txBytes);
    APSARA_TEST_EQUAL_FATAL(161069, metrics[4].txPackets);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].txErrors);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].txDropped);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].txOverruns);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].txCollisions);
    APSARA_TEST_EQUAL_FATAL(0, metrics[4].txCarrier);
}

void NetCollectorUnittest::TestReadSocketStat() const {
    NetCollector collector = NetCollector();
    int tcp = 0;
    APSARA_TEST_TRUE(collector.ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT, tcp));
    APSARA_TEST_EQUAL_FATAL(tcp, 30);

    int tcp6 = 0;
    APSARA_TEST_TRUE(collector.ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT6, tcp6));
    APSARA_TEST_EQUAL_FATAL(tcp6, 0);
}

void NetCollectorUnittest::TestReadNetLink() const {
    NetCollector collector = NetCollector();
    vector<uint64_t> tcpStats;
    APSARA_TEST_TRUE(collector.ReadNetLink(tcpStats));
}

void NetCollectorUnittest::TestGetNetStateByNetLink() const {
    NetCollector collector = NetCollector();
    NetState netState;
    APSARA_TEST_TRUE(collector.GetNetStateByNetLink(netState));

}

void NetCollectorUnittest::TestGetNetTCPInfo() const {
    NetCollector collector = NetCollector();
    ResTCPStat resTCPStat;
    APSARA_TEST_TRUE(collector.GetNetTCPInfo(resTCPStat));
}

void NetCollectorUnittest::TestCollect() const {
    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;
    NetCollector collector = NetCollector();
    PipelineEventGroup group(make_shared<SourceBuffer>());
    HostMonitorTimerEvent::CollectConfig collectconfig(NetCollector::sName, 0, 0, std::chrono::seconds(1));
    
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));
    APSARA_TEST_TRUE(collector.Collect(collectconfig, &group));

    APSARA_TEST_EQUAL_FATAL(9UL, group.GetEvents().size());

    vector<string> device_names = {
        "lo",
        "eth0",
        "docker0",
        "veth6c3a07a",
        "vethc4371db",
    };

    vector<string> rate_names = {
        "networkin_droppackages_percent_device_avg",
        "networkin_droppackages_percent_device_max",
        "networkin_droppackages_percent_device_min",
        "networkin_rate_avg",
        "networkin_rate_max",
        "networkin_rate_min",
        "networkout_droppackages_percent_device_avg",
        "networkout_droppackages_percent_device_max",
        "networkout_droppackages_percent_device_min",
        "networkout_errorpackages_avg",
        "networkout_errorpackages_max",
        "networkout_errorpackages_min",
        "networkout_packages_avg",
        "networkout_packages_max",
        "networkout_packages_min",
        "networkout_rate_avg",
        "networkout_rate_max",
        "networkout_rate_min",
    };

    for(size_t j=0; j<device_names.size(); j++){
        auto event = group.GetEvents()[j].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(device_names[j], event.GetTag("device"));
        APSARA_TEST_EQUAL_FATAL(hostname, event.GetTag("hostname"));
        for (size_t i = 0; i < rate_names.size(); ++i){
            APSARA_TEST_TRUE(maps.find(rate_names[i]) != maps.end());
            EXPECT_NEAR(0.0, maps[rate_names[i]].Value, 1e-6);
        }
    }

    vector<string> tcp_names = {
        "listen",
        "established",
        "nonestablished",
    };
    vector<string> tcp_cnt_names = {
        "net_tcpconnection_avg",
        "net_tcpconnection_max",
        "net_tcpconnection_min",
    };
    for(size_t j=0; j<tcp_names.size(); j++){
        std::cout<<"xixix "<<tcp_names[j]<<" "<<tcp_cnt_names[j]<<std::endl;
        auto event = group.GetEvents()[j+device_names.size()].Cast<MetricEvent>();
        auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
        APSARA_TEST_EQUAL_FATAL(tcp_names[j], event.GetTag("state"));
        for (size_t i = 0; i < tcp_cnt_names.size(); ++i){
            APSARA_TEST_TRUE(maps.find(tcp_cnt_names[i]) != maps.end());
        }
    }

    vector<string> tcp_total_names = {
        "vm.TcpCount_min",
        "vm.TcpCount_max",
        "vm.TcpCount_avg",
    };
    auto event = group.GetEvents()[8].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;
    APSARA_TEST_EQUAL_FATAL(std::string("total"), event.GetTag("state"));
    for(size_t i = 0; i < tcp_total_names.size(); ++i){
        APSARA_TEST_TRUE(maps.find(tcp_total_names[i]) != maps.end());
    }

}

UNIT_TEST_CASE(NetCollectorUnittest, TestGetNetRateInfo);
UNIT_TEST_CASE(NetCollectorUnittest, TestReadSocketStat);
UNIT_TEST_CASE(NetCollectorUnittest, TestReadNetLink);
UNIT_TEST_CASE(NetCollectorUnittest, TestGetNetStateByNetLink);
UNIT_TEST_CASE(NetCollectorUnittest, TestGetNetTCPInfo);
UNIT_TEST_CASE(NetCollectorUnittest, TestCollect);

} // namespace logtail

UNIT_TEST_MAIN