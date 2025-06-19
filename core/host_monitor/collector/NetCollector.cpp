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

#include "host_monitor/collector/NetCollector.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <boost/lexical_cast.hpp>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::string NetCollector::sName = "net";
const std::string kMetricLabelStat = "state";
const std::string kMetricValueTag = "valueTag";

const static int NET_INTERFACE_LIST_MAX = 20;

NetCollector::NetCollector() {
    Init();
}

int NetCollector::Init(int totalCount) {
    mTotalCount = totalCount;
    mCount = 0;
    mLastTime = std::chrono::steady_clock::now();
    return 0;
}

bool NetCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    ResTCPStat resTCPStat;
    std::vector<NetInterfaceMetric> netInterfaceMetrics;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    if (!(GetNetTCPInfo(resTCPStat) && GetNetRateInfo(netInterfaceMetrics))) {
        mLastTime = start;
        return false;
    }

    mCount++;
    double interval = std::chrono::duration_cast<std::chrono::duration<double>>(start - mLastTime).count();

    // tcp
    mTCPCal.AddValue(resTCPStat);
    
    // rate
    for (auto& netInterfaceMetric : netInterfaceMetrics) {
        if (netInterfaceMetric.name.empty()) {
            continue;
        }

        std::string curname = netInterfaceMetric.name;
        // 入方向、出方向 的 丢包率
        ResNetPackRate resPackRate;
        resPackRate.rxDropRate = netInterfaceMetric.rxPackets == 0
            ? 0.0
            : netInterfaceMetric.rxDropped / netInterfaceMetric.rxPackets * 100.0;
        resPackRate.txDropRate = netInterfaceMetric.txPackets == 0
            ? 0.0
            : netInterfaceMetric.txDropped / netInterfaceMetric.txPackets * 100.0;
        
        // mPackRateCalMap没有这个接口的数据
        if (mPackRateCalMap.find(curname) == mPackRateCalMap.end()) {
            mPackRateCalMap[curname] = MetricCalculate<ResNetPackRate>();
        }
        mPackRateCalMap[curname].AddValue(resPackRate);

        // // 更新last内容
        // mLastInterfaceMetrics[curname] = netInterfaceMetric;

        // 每秒发、收 的 字节数,每秒收包数，每秒收包错误数
        if (mLastInterfaceMetrics.find(curname) != mLastInterfaceMetrics.end()) {
            ResNetRatePerSec resRatePerSec;
            
            resRatePerSec.rxByteRate
                = mLastInterfaceMetrics[curname].rxBytes > netInterfaceMetric.rxBytes || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxBytes - mLastInterfaceMetrics[curname].rxBytes) * 8 / interval;
            
            resRatePerSec.rxPackRate
                = mLastInterfaceMetrics[curname].rxPackets > netInterfaceMetric.rxPackets || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxPackets - mLastInterfaceMetrics[curname].rxPackets) / interval;
            
            resRatePerSec.txPackRate
                = mLastInterfaceMetrics[curname].txPackets > netInterfaceMetric.txPackets || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txPackets - mLastInterfaceMetrics[curname].txPackets) / interval;
            
            resRatePerSec.txByteRate
                = mLastInterfaceMetrics[curname].txBytes > netInterfaceMetric.txBytes || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txBytes - mLastInterfaceMetrics[curname].txBytes) * 8 / interval;
            
            resRatePerSec.txErrorRate
                = mLastInterfaceMetrics[curname].txErrors > netInterfaceMetric.txErrors || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txErrors - mLastInterfaceMetrics[curname].txErrors) / interval;
            
            // mRatePerSecCalMap没有这个接口的数据
            if (mRatePerSecCalMap.find(curname) == mRatePerSecCalMap.end()) {
                mRatePerSecCalMap[curname] = MetricCalculate<ResNetRatePerSec>();
            }
            mRatePerSecCalMap[curname].AddValue(resRatePerSec);
        }
        // 第一次统计这个接口的数据，无法计算每秒收发的数据，只更新last内容
        mLastInterfaceMetrics[curname] = netInterfaceMetric;
    }
    

    if (mCount < mTotalCount) {
        mLastTime = start;
        return true;
    }

    const time_t now = time(nullptr);
    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;


    // 入方向、出方向 的 丢包率
    // 每秒发、收 的 字节数、包数

    for (auto& packRateCal : mPackRateCalMap) {
        std::string curname = packRateCal.first;

        MetricEvent* metricEvent = group->AddMetricEvent(true);
        if (!metricEvent) {
            mLastTime = start;
            return false;
        }

        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetTag(std::string("hostname"), hostname);
        metricEvent->SetTag(std::string("device"), curname);
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();


        ResNetPackRate minPackRate, maxPackRate, avgPackRate;
        packRateCal.second.Stat(maxPackRate, minPackRate, avgPackRate);
        packRateCal.second.Reset();


        std::vector<std::string> packRateNames = {
            "networkin_droppackages_percent_min",
            "networkin_droppackages_percent_max",
            "networkin_droppackages_percent_avg",
            "networkout_droppackages_percent_min",
            "networkout_droppackages_percent_max",
            "networkout_droppackages_percent_avg",
        };
        std::vector<double> packRateValues = {
            minPackRate.rxDropRate,
            maxPackRate.rxDropRate,
            avgPackRate.rxDropRate,
            minPackRate.txDropRate,
            maxPackRate.txDropRate,
            avgPackRate.txDropRate,
        };

        if (mRatePerSecCalMap.find(curname) != mRatePerSecCalMap.end()) {
            ResNetRatePerSec minRatePerSec, maxRatePerSec, avgRatePerSec;
            mRatePerSecCalMap[curname].Stat(maxRatePerSec, minRatePerSec, avgRatePerSec);
            mRatePerSecCalMap[curname].Reset();
            packRateNames.push_back("networkout_packages_min");
            packRateValues.push_back(minRatePerSec.txPackRate);
            packRateNames.push_back("networkout_packages_max");
            packRateValues.push_back(maxRatePerSec.txPackRate);
            packRateNames.push_back("networkout_packages_avg");
            packRateValues.push_back(avgRatePerSec.txPackRate);
            packRateNames.push_back("networkin_packages_min");
            packRateValues.push_back(minRatePerSec.rxPackRate);
            packRateNames.push_back("networkin_packages_max");
            packRateValues.push_back(maxRatePerSec.rxPackRate);
            packRateNames.push_back("networkin_packages_avg");
            packRateValues.push_back(avgRatePerSec.rxPackRate);
            packRateNames.push_back("networkout_errorpackages_min");
            packRateValues.push_back(minRatePerSec.txErrorRate);
            packRateNames.push_back("networkout_errorpackages_max");
            packRateValues.push_back(maxRatePerSec.txErrorRate);
            packRateNames.push_back("networkout_errorpackages_avg");
            packRateValues.push_back(avgRatePerSec.txErrorRate);
            packRateNames.push_back("networkin_errorpackages_min");
            packRateValues.push_back(minRatePerSec.rxErrorRate);
            packRateNames.push_back("networkin_errorpackages_max");
            packRateValues.push_back(maxRatePerSec.rxErrorRate);
            packRateNames.push_back("networkin_errorpackages_avg");
            packRateValues.push_back(avgRatePerSec.rxErrorRate);
            packRateNames.push_back("networkout_rate_min");
            packRateValues.push_back(minRatePerSec.txByteRate);
            packRateNames.push_back("networkout_rate_max");
            packRateValues.push_back(maxRatePerSec.txByteRate);
            packRateNames.push_back("networkout_rate_avg");
            packRateValues.push_back(avgRatePerSec.txByteRate);
            packRateNames.push_back("networkin_rate_min");
            packRateValues.push_back(minRatePerSec.rxByteRate);
            packRateNames.push_back("networkin_rate_max");
            packRateValues.push_back(maxRatePerSec.rxByteRate);
            packRateNames.push_back("networkin_rate_avg");
            packRateValues.push_back(avgRatePerSec.rxByteRate);
        }


        for (size_t i = 0; i < packRateNames.size(); i++) {
            multiDoubleValues->SetValue(
                packRateNames[i], UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, packRateValues[i]});
        }
    }


    // TCP各种状态下的连接数
    ResTCPStat minTCP, maxTCP, avgTCP;
    mTCPCal.Stat(maxTCP, minTCP, avgTCP);
    mTCPCal.Reset();

    MetricEvent* listenEvent = group->AddMetricEvent(true);
    if (!listenEvent) {
        mLastTime = start;
        return false;
    }
    listenEvent->SetTimestamp(now, 0);
    listenEvent->SetTag(std::string("state"), std::string("LISTEN"));
    listenEvent->SetValue<UntypedMultiDoubleValues>(listenEvent);
    auto* listenMultiDoubleValues = listenEvent->MutableValue<UntypedMultiDoubleValues>();
    listenMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_min"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpListen)});
    
    listenMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_max"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpListen)});
    
    listenMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_avg"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpListen)});
    

    MetricEvent* establishedEvent = group->AddMetricEvent(true);
    if (!establishedEvent) {
        mLastTime = start;
        return false;
    }
    establishedEvent->SetTimestamp(now, 0);
    establishedEvent->SetTag(std::string("state"), std::string("ESTABLISHED"));
    establishedEvent->SetValue<UntypedMultiDoubleValues>(establishedEvent);
    auto* establishedMultiDoubleValues = establishedEvent->MutableValue<UntypedMultiDoubleValues>();
    establishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_min"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpEstablished)});
    
    establishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_max"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpEstablished)});
    
    establishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_avg"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpEstablished)});
    

    MetricEvent* nonestablishedEvent = group->AddMetricEvent(true);
    if (!nonestablishedEvent) {
        mLastTime = start;
        return false;
    }
    nonestablishedEvent->SetTimestamp(now, 0);
    nonestablishedEvent->SetTag(std::string("state"), std::string("NON_ESTABLISHED"));
    nonestablishedEvent->SetValue<UntypedMultiDoubleValues>(nonestablishedEvent);
    auto* nonestablishedMultiDoubleValues = nonestablishedEvent->MutableValue<UntypedMultiDoubleValues>();
    nonestablishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_min"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpNonEstablished)});
    nonestablishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_max"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpNonEstablished)});
    nonestablishedMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_avg"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpNonEstablished)});

    MetricEvent* totalEvent = group->AddMetricEvent(true);
    if (!totalEvent) {
        mLastTime = start;
        return false;
    }
    totalEvent->SetTimestamp(now, 0);
    totalEvent->SetTag(std::string("state"), std::string("TCP_TOTAL"));
    totalEvent->SetValue<UntypedMultiDoubleValues>(totalEvent);
    auto* totalMultiDoubleValues = totalEvent->MutableValue<UntypedMultiDoubleValues>();
    totalMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_min"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpTotal)});
    
    totalMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_max"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpTotal)});
    
    totalMultiDoubleValues->SetValue(
        std::string("net_tcpconnection_avg"),
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpTotal)});
    


    mCount = 0;
    mLastTime = start;
    return true;
}

// bool NetCollector::GetNetInfo(std::map<std::string, ResNetRate>& resNetRateMap, ResTCPStat& resTCPStat) {
//     return GetNetRateInfo(resNetRateMap) && GetNetTCPInfo(resTCPStat);
// }


// #define OPTION_NETLINK 1
// #define OPTION_SS 2
// #define OPTION_FILE 4

bool NetCollector::GetNetTCPInfo(ResTCPStat& resTCPStat) {
    
    NetState netState;
    // typedef decltype(&NetCollector::GetNetStateByNetLink) FnType;
    // std::vector<FnType> funcs = {
    //     &NetCollector::GetNetStateByNetLink,
    // };

    // const size_t funcSize = sizeof(funcs) / sizeof(funcs[0]);

    bool ret = false;
    // for (size_t i = 0; i < funcSize && ret != true; i++) {
    //     ret = (this->*funcs[i])(netState);

    // }
    ret = GetNetStateByNetLink(netState);

    if (ret) {
        resTCPStat.tcpEstablished = (netState.tcpStates[TCP_ESTABLISHED]);
        resTCPStat.tcpListen = (netState.tcpStates[TCP_LISTEN]);
        resTCPStat.tcpTotal = (netState.tcpStates[TCP_TOTAL]);
        resTCPStat.tcpNonEstablished = (netState.tcpStates[TCP_NON_ESTABLISHED]);
    }

    return ret;
}

// GetNetStateByNetLink
bool NetCollector::GetNetStateByNetLink(NetState& netState) {
    std::vector<uint64_t> tcpStateCount(TCP_CLOSING + 1, 0);
    if (ReadNetLink(tcpStateCount) == false) {
        return false;
    }
    int tcp = 0, tcpSocketStat = 0;

    if (ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT, tcp)) {
        tcpSocketStat += tcp;
    }
    if (ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT6, tcp)) {
        tcpSocketStat += tcp;
    }

    int total = 0;
    for (int i = TCP_ESTABLISHED; i <= TCP_CLOSING; i++) {
        if (i == TCP_SYN_SENT || i == TCP_SYN_RECV) {
            total += tcpStateCount[i];
        }
        netState.tcpStates[i] = tcpStateCount[i];
    }
    // 设置为-1表示没有采集
    netState.tcpStates[TCP_TOTAL] = total + tcpSocketStat;
    netState.tcpStates[TCP_NON_ESTABLISHED] = netState.tcpStates[TCP_TOTAL] - netState.tcpStates[TCP_ESTABLISHED];
    return true;
}


bool NetCollector::ReadNetLink(std::vector<uint64_t>& tcpStateCount) {
    
    static uint32_t sequence_number = 1;
    int fd;
    // struct inet_diag_msg *r;
    // 使用netlink socket与内核通信
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
    if (fd < 0) {

        LOG_WARNING(sLogger,
                    ("ReadNetLink, socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG) failed, error msg: ",
                     std::string(strerror(errno))));
        close(fd);
        return false;
    }


    // 存在多个netlink socket时，必须单独bind,并通过nl_pid来区分
    struct sockaddr_nl nladdr_bind {};
    memset(&nladdr_bind, 0, sizeof(nladdr_bind));
    nladdr_bind.nl_family = AF_NETLINK;
    nladdr_bind.nl_pad = 0;
    nladdr_bind.nl_pid = getpid();
    nladdr_bind.nl_groups = 0;
    if (bind(fd, (struct sockaddr*)&nladdr_bind, sizeof(nladdr_bind))) {
        LOG_WARNING(sLogger, ("ReadNetLink, bind netlink socket failed, error msg: ", std::string(strerror(errno))));
        close(fd);
        return false;
    }
    struct sockaddr_nl nladdr {};
    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    struct NetLinkRequest req {};
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = sizeof(req);
    req.nlh.nlmsg_type = TCPDIAG_GETSOCK;
    req.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    // sendto kernel
    req.nlh.nlmsg_pid = getpid();
    req.nlh.nlmsg_seq = ++sequence_number;
    req.r.idiag_family = AF_INET;
    req.r.idiag_states = 0xfff;
    req.r.idiag_ext = 0;
    struct iovec iov {};
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = &req;
    iov.iov_len = sizeof(req);
    struct msghdr msg {};
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&nladdr;
    msg.msg_namelen = sizeof(nladdr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(fd, &msg, 0) < 0) {
        LOG_WARNING(sLogger, ("ReadNetLink, sendmsg(2) failed, error msg: ", std::string(strerror(errno))));
        close(fd);
        return false;
    }
    char buf[8192];
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    while (true) {
        // struct nlmsghdr *h;
        memset(&msg, 0, sizeof(msg));
        msg.msg_name = (void*)&nladdr;
        msg.msg_namelen = sizeof(nladdr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        ssize_t status = recvmsg(fd, (struct msghdr*)&msg, 0);
        if (status < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            
            LOG_WARNING(sLogger, ("ReadNetLink, recvmsg(2) failed, error msg: ", std::string(strerror(errno))));
            close(fd);
            return false;
        } else if (status == 0) {
            
            LOG_WARNING(sLogger,
                        ("ReadNetLink, Unexpected zero-sized  reply from netlink socket. error msg: ",
                         std::string(strerror(errno))));
            close(fd);
            return true;
        }

        // h = (struct nlmsghdr *) buf;
        for (auto h = (struct nlmsghdr*)buf; NLMSG_OK(h, status); h = NLMSG_NEXT(h, status)) {
            if (h->nlmsg_seq != sequence_number) {
                // sequence_number is not equal
                // h = NLMSG_NEXT(h, status);
                continue;
            }

            if (h->nlmsg_type == NLMSG_DONE) {
                close(fd);
                return true;
            } else if (h->nlmsg_type == NLMSG_ERROR) {
                if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    LOG_WARNING(sLogger, ("ReadNetLink ", "message truncated"));
                    
                } else {
                    auto msg_error = (struct nlmsgerr*)NLMSG_DATA(h);
                    LOG_WARNING(sLogger, ("ReadNetLink, Received error, error msg: ", msg_error));
                    
                }
                close(fd);
                return false;
            }
            auto r = (struct inet_diag_msg*)NLMSG_DATA(h);
            /*This code does not(need to) distinguish between IPv4 and IPv6.*/
            if (r->idiag_state > TCP_CLOSING || r->idiag_state < TCP_ESTABLISHED) {
                // Ignoring connection with unknown state
                continue;
            }
            tcpStateCount[r->idiag_state]++;
            // h = NLMSG_NEXT(h, status);
        }
    }
    close(fd);
    return true;
}

bool NetCollector::ReadSocketStat(const std::filesystem::path& path, int& tcp) {
    tcp = 0;
    bool ret = false;
    if (!path.empty()) {
        std::vector<std::string> sockstatLines;
        std::string errorMessage;
        ret = GetHostSystemStatWithPath(sockstatLines, errorMessage, path);
        if (ret && !sockstatLines.empty()) {
            for (auto const& line : sockstatLines) {
                std::vector<std::string> metrics;
                boost::split(metrics, line, boost::is_any_of(" "), boost::token_compress_on);
                std::string key = metrics.front();
                boost::algorithm::trim(key);
                if (metrics.size() >= 9 && (key == "TCP:" || key == "TCP6:")) {
                    tcp += std::stoi(metrics[6]); // tw
                    tcp += std::stoi(metrics[8]); // alloc
                }
            }
        }
    }
    return ret;
}

//
// bool NetCollector::GetNetRateInfo(std::map<std::string, ResNetRate>& resNetRateMap) {
//     std::vector<NetInterfaceMetric> netInterfaceMetrics
//     if (!GetInterfaceConfigs(netInterfaceMetrics)){
//         return false;
//     }
//     for (auto& netInterfaceMetric : netInterfaceMetrics) {
//         if (netInterfaceMetric.name.empty()) {
//             continue;
//         }
//         ResNetRate resNetRate;
//         resNetRate.rxDropRate = netInterfaceMetric.rxPackets == 0?0.0 : netInterfaceMetric.rxDropped /
//         netInterfaceMetric.rxPackets; ResNetRate. resNetRateMap[netInterfaceMetric.name] =
//     }
// }


bool NetCollector::GetNetRateInfo(std::vector<NetInterfaceMetric>& netInterfaceMetrics) {
    
    //  /proc/net/dev
    std::vector<std::string> netDevLines = {};
    std::string errorMessage;
    bool ret = GetHostSystemStatWithPath(netDevLines, errorMessage, PROCESS_DIR / PROCESS_NET_DEV);
    if (!ret || netDevLines.empty()) {
        return false;
    }

    for (size_t i = 2; i < netDevLines.size(); ++i) {
        auto pos = netDevLines[i].find_first_of(':');
        std::string devCounterStr = netDevLines[i].substr(pos + 1);
        std::string devName = netDevLines[i].substr(0, pos);
        std::vector<std::string> netDevMetric;
        boost::algorithm::trim(devCounterStr);
        boost::split(netDevMetric, devCounterStr, boost::is_any_of(" "), boost::token_compress_on);
        // int hh = 1;
        // for (const auto& metric : netDevMetric) {
        //     std::cout <<hh++<<"  "<< metric <<"   "<<metric.size()<< std::endl;
        // }
        if (netDevMetric.size() >= 16) {
            NetInterfaceMetric information;
            int index = 0;
            boost::algorithm::trim(devName);
            // std::cout<<"devName after trim: "<< devName<<std::endl;
            information.name = devName;
            information.rxBytes = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.rxPackets = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.rxErrors = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.rxDropped = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.rxOverruns = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.rxFrame = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            // skip compressed multicast
            index += 2;
            information.txBytes = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txPackets = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txErrors = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txDropped = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txOverruns = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txCollisions = boost::lexical_cast<uint64_t>(netDevMetric[index++]);
            information.txCarrier = boost::lexical_cast<uint64_t>(netDevMetric[index++]);

            information.speed = -1;
            netInterfaceMetrics.push_back(information);
        }
    }
    return true;
}

// bool NetCollector::GetInterfaceInfo(const std::string& name, NetInterfaceMetric& netInterfaceMetric) {
// }
// int LinuxSystemInformationCollector::SicGetInterfaceInformation(const std::string& name,
//                                                                 SicNetInterfaceInformation& information) {
//     std::vector<std::string> netDevLines = {};
//     int ret = GetFileLines(PROCESS_DIR / PROCESS_NET_DEV, netDevLines, true, SicPtr()->errorMessage);
//     if (res!= 0 || netDevLines.empty()) {
//         return ret;
//     }

//     for (size_t i = 2; i < netDevLines.size(); ++i) {
//         auto pos = netDevLines[i].find_first_of(':');
//         std::string devCounterStr = netDevLines[i].substr(pos + 1);
//         std::string devName = netDevLines[i].substr(0, pos);
//         std::vector<std::string> netDevMetric = split(devCounterStr, ' ', true);
//         if (netDevMetric.size() >= 16) {
//             int index = 0;
//             devName = TrimSpace(devName);
//             if (devName == name) {
//                 information.rxBytes = convert<uint64_t>(netDevMetric[index++]);
//                 information.rxPackets = convert<uint64_t>(netDevMetric[index++]);
//                 information.rxErrors = convert<uint64_t>(netDevMetric[index++]);
//                 information.rxDropped = convert<uint64_t>(netDevMetric[index++]);
//                 information.rxOverruns = convert<uint64_t>(netDevMetric[index++]);
//                 information.rxFrame = convert<uint64_t>(netDevMetric[index++]);
//                 // skip compressed multicast
//                 index += 2;
//                 information.txBytes = convert<uint64_t>(netDevMetric[index++]);
//                 information.txPackets = convert<uint64_t>(netDevMetric[index++]);
//                 information.txErrors = convert<uint64_t>(netDevMetric[index++]);
//                 information.txDropped = convert<uint64_t>(netDevMetric[index++]);
//                 information.txOverruns = convert<uint64_t>(netDevMetric[index++]);
//                 information.txCollisions = convert<uint64_t>(netDevMetric[index++]);
//                 information.txCarrier = convert<uint64_t>(netDevMetric[index++]);

//                 information.speed = -1;
//                 return SIC_EXECUTABLE_SUCCESS;
//             }
//         }
//     }

//     SicPtr()->SetErrorMessage("Dev " + name + " Not Found!\n");
//     return SIC_EXECUTABLE_FAILED;
// }


} // namespace logtail
