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
#pragma once

#include <vector>
#include <filesystem>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h" 
#include "plugin/input/InputHostMonitor.h"
#include "host_monitor/Constants.h"
#include "monitor/Monitor.h"

namespace logtail {

extern const uint32_t kMinInterval;
extern const uint32_t kDefaultInterval;

// extern std::filesystem::path PROCESS_NET_SOCKSTAT;
// extern std::filesystem::path PROCESS_NET_SOCKSTAT6;
// extern std::filesystem::path PROCESS_NET_DEV;

enum EnumTcpState : int8_t {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
    TCP_IDLE,
    TCP_BOUND,
    TCP_UNKNOWN,
    TCP_TOTAL,
    TCP_NON_ESTABLISHED,

    TCP_STATE_END, // 仅用于状态计数
};
// std::string GetTcpStateName(EnumTcpState n);

struct NetState {
    int tcpStates[TCP_STATE_END] = {0};
    unsigned int tcpInboundTotal = 0;
    unsigned int tcpOutboundTotal = 0;
    unsigned int allInboundTotal = 0;
    unsigned int allOutboundTotal = 0;

    void calcTcpTotalAndNonEstablished();
    std::string toString(const char *lf = "\n", const char *tab = "    ") const;
    bool operator==(const NetState &) const;

    inline bool operator!=(const NetState &r) const {
        return !(*this == r);
    }
};

struct NetLinkRequest {
    struct nlmsghdr nlh;
    struct inet_diag_req r;
};

// /proc/net/snmp  tcp:
enum class EnumNetSnmpTCPKey : int {
    RtoAlgorithm = 1,
    RtoMin,
    RtoMax,
    MaxConn,
    ActiveOpens,
    PassiveOpens,
    AttemptFails,
    EstabResets,
    CurrEstab,
    InSegs,
    OutSegs,
    RetransSegs,
    InErrs,
    OutRsts,
    InCsumErrors,
};

static constexpr const bool simpleTcpState[] = {
    false,
    true, // SIC_TCP_ESTABLISHED
    false, false, false, false, false, false, false, false,
    true, // SIC_TCP_LISTEN
    false, false, false, false,
    true, // SIC_TCP_TOTAL
    true, // SIC_TCP_NON_ESTABLISHED
};

struct NetInterfaceMetric {
    // received
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    uint64_t rxErrors = 0;
    uint64_t rxDropped = 0;
    uint64_t rxOverruns = 0;
    uint64_t rxFrame = 0;
    // transmitted
    uint64_t txPackets = 0;
    uint64_t txBytes = 0;
    uint64_t txErrors = 0;
    uint64_t txDropped = 0;
    uint64_t txOverruns = 0;
    uint64_t txCollisions = 0;
    uint64_t txCarrier = 0;

    uint64_t speed = 0;
    std::string name;
};

struct InterfaceConfig {
    std::string name;
    std::string ipv4;
    std::string ipv6;
};

// TCP各种状态下的连接数
struct ResTCPStat {
    
    uint64_t tcpEstablished;
    uint64_t tcpListen;
    uint64_t tcpTotal;
    uint64_t tcpNonEstablished;

    static inline const FieldName<ResTCPStat, uint64_t> resTCPStatFields[] = {
        FIELD_ENTRY(ResTCPStat, tcpEstablished),
        FIELD_ENTRY(ResTCPStat, tcpListen),
        FIELD_ENTRY(ResTCPStat, tcpTotal),
        FIELD_ENTRY(ResTCPStat, tcpNonEstablished),
    };

    static void enumerate(const std::function<void(const FieldName<ResTCPStat, uint64_t>&)>& callback) {
        for (auto& field : resTCPStatFields) {
            callback(field);
        }
    };
};

// 入方向丢包率，出方向丢包率，
struct ResNetPackRate{
    double rxDropRate;
    double txDropRate;
    // double rxErrorRate;
    // double txErrorRate;

    static inline const FieldName<ResNetPackRate, double> resPackRateFields[] = {
        FIELD_ENTRY(ResNetPackRate, rxDropRate),
        FIELD_ENTRY(ResNetPackRate, txDropRate),
        // FIELD_ENTRY(ResNetPackRate, rxErrorRate),
        // FIELD_ENTRY(ResNetPackRate, txErrorRate),
    };
    static void enumerate(const std::function<void(const FieldName<ResNetPackRate, double>&)>& callback) {
        for (auto& field : resPackRateFields) {
            callback(field);
        }
    };
};

//每秒发包数，上行带宽，下行带宽.每秒发送错误包数量
struct ResNetRatePerSec{
    double rxPackRate;
    double txPackRate;
    double rxByteRate;
    double txByteRate;
    double txErrorRate;
    double rxErrorRate;

    static inline const FieldName<ResNetRatePerSec, double> resRatePerSecFields[] = {
        FIELD_ENTRY(ResNetRatePerSec, rxPackRate),
        FIELD_ENTRY(ResNetRatePerSec, txPackRate),
        FIELD_ENTRY(ResNetRatePerSec, rxByteRate),
        FIELD_ENTRY(ResNetRatePerSec, txByteRate),
        FIELD_ENTRY(ResNetRatePerSec, txErrorRate),
        FIELD_ENTRY(ResNetRatePerSec, rxErrorRate),
    };
    static void enumerate(const std::function<void(const FieldName<ResNetRatePerSec, double>&)>& callback) {
        for (auto& field : resRatePerSecFields) {
            callback(field);
        }
    }
};


class NetCollector : public BaseCollector {
public:
    NetCollector();

    // int Init(int totalCount = kDefaultInterval / kMinInterval);
    int Init(int totalCount = 15);

    ~NetCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;

    const std::string& Name() const override { return sName; }

private:
    // bool GetNetInfo(std::vector<NetInterfaceMetric>& netInterfaceMetrics, ResTCPStat& resTCPStat); //获取全部网络指标
    
    bool GetNetTCPInfo(ResTCPStat& resTCPStat); //获取各种tcp状态
    bool GetNetStateByNetLink(NetState& netState); //通过网络连接获取tcp指标
    bool ReadNetLink(std::vector<uint64_t>& tcpStateCount);
    bool ReadSocketStat(const std::filesystem::path& path, int& tcp);

    bool GetNetRateInfo(std::vector<NetInterfaceMetric>& netInterfaceMetrics); //获取各种rate指标
    bool GetInterfaceConfigs(std::vector<NetInterfaceMetric>& netInterfaceMetrics);

    // bool GetNetStateBySS();
    // bool GetNetStateByReadFile();

private:
    std::map<std::string, NetInterfaceMetric> mLastInterfaceStatMap;
    std::map<std::string, InterfaceConfig> mInterfaceConfigMap;
    // std::chrono::steady_clock::time_point mInterfaceConfigExpireTime;
    std::chrono::steady_clock::time_point mLastTime;
    std::map<std::string,NetInterfaceMetric> mLastInterfaceMetrics;
    int mTotalCount = 0;
    int mCount = 0;
    MetricCalculate<ResTCPStat, uint64_t> mTCPCal;
    std::map<std::string, MetricCalculate<ResNetPackRate>> mPackRateCalMap;
    std::map<std::string, MetricCalculate<ResNetRatePerSec>> mRatePerSecCalMap;
};


} // namespace logtail
