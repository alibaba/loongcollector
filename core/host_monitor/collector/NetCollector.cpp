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

#include <boost/lexical_cast.hpp>
#include <chrono>
#include <filesystem>
#include <string>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::string NetCollector::sName = "net";

NetCollector::NetCollector() {
    Init();
}

int NetCollector::Init(int totalCount) {
    mCountPerReport = totalCount;
    mCount = 0;
    mLastTime = std::chrono::steady_clock::now();
    return 0;
}

bool NetCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    TCPStatInformation resTCPStat;
    NetRateInformation netInterfaceMetrics;
    NetInterfaceInformation netInterfaces;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    if (!(SystemInterface::GetInstance()->GetTCPStatInformation(resTCPStat)
          && SystemInterface::GetInstance()->GetNetRateInformation(netInterfaceMetrics)
          && SystemInterface::GetInstance()->GetNetInterfaceInformation(netInterfaces))) {
        mLastTime = start;
        return false;
    }

    // 更新记录ip
    for (auto& netInterface : netInterfaces.configs) {
        if (netInterface.name.empty()) {
            continue;
        }

        mDevIp[netInterface.name] = netInterface.address.str();
        if (mDevIp[netInterface.name].empty()) {
            mDevIp[netInterface.name] = netInterface.address6.str();
        }
    }

    mCount++;
    double interval = std::chrono::duration_cast<std::chrono::duration<double>>(start - mLastTime).count();

    // tcp
    mTCPCal.AddValue(resTCPStat.stat);

    // rate
    for (auto& netInterfaceMetric : netInterfaceMetrics.metrics) {
        if (netInterfaceMetric.name.empty()) {
            continue;
        }

        std::string curname = netInterfaceMetric.name;

        // 每秒发、收 的 字节数,每秒收包数，每秒收包错误数
        if (mLastInterfaceMetrics.find(curname) != mLastInterfaceMetrics.end()) {
            ResNetRatePerSec resRatePerSec;

            resRatePerSec.rxByteRate
                = mLastInterfaceMetrics[curname].rxBytes > netInterfaceMetric.rxBytes || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxBytes - mLastInterfaceMetrics[curname].rxBytes) * 8
                    / interval;

            resRatePerSec.rxPackRate
                = mLastInterfaceMetrics[curname].rxPackets > netInterfaceMetric.rxPackets || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxPackets - mLastInterfaceMetrics[curname].rxPackets)
                    / interval;

            resRatePerSec.txPackRate
                = mLastInterfaceMetrics[curname].txPackets > netInterfaceMetric.txPackets || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txPackets - mLastInterfaceMetrics[curname].txPackets)
                    / interval;

            resRatePerSec.txByteRate
                = mLastInterfaceMetrics[curname].txBytes > netInterfaceMetric.txBytes || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txBytes - mLastInterfaceMetrics[curname].txBytes) * 8
                    / interval;

            resRatePerSec.txErrorRate
                = mLastInterfaceMetrics[curname].txErrors > netInterfaceMetric.txErrors || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txErrors - mLastInterfaceMetrics[curname].txErrors) / interval;

            resRatePerSec.rxErrorRate
                = mLastInterfaceMetrics[curname].rxErrors > netInterfaceMetric.rxErrors || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxErrors - mLastInterfaceMetrics[curname].rxErrors) / interval;

            resRatePerSec.rxDropRate
                = mLastInterfaceMetrics[curname].rxDropped > netInterfaceMetric.rxDropped || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.rxDropped - mLastInterfaceMetrics[curname].rxDropped)
                    / interval;

            resRatePerSec.txDropRate
                = mLastInterfaceMetrics[curname].txDropped > netInterfaceMetric.txDropped || interval <= 0
                ? 0.0
                : static_cast<double>(netInterfaceMetric.txDropped - mLastInterfaceMetrics[curname].txDropped)
                    / interval;
            // mRatePerSecCalMap没有这个接口的数据
            if (mRatePerSecCalMap.find(curname) == mRatePerSecCalMap.end()) {
                mRatePerSecCalMap[curname] = MetricCalculate<ResNetRatePerSec>();
            }
            mRatePerSecCalMap[curname].AddValue(resRatePerSec);
        }
        // 第一次统计这个接口的数据，无法计算每秒收发的数据，只更新last内容
        mLastInterfaceMetrics[curname] = netInterfaceMetric;
    }

    if (mCount < mCountPerReport) {
        mLastTime = start;
        return true;
    }

    const time_t now = time(nullptr);
    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;


    // 入方向、出方向 的 丢包率
    // 每秒发、收 的 字节数、包数

    for (auto& packRateCal : mRatePerSecCalMap) {
        std::string curname = packRateCal.first;

        MetricEvent* metricEvent = group->AddMetricEvent(true);
        if (!metricEvent) {
            mLastTime = start;
            return false;
        }

        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetTag(std::string("hostname"), hostname);
        metricEvent->SetTag(std::string("device"), curname);
        metricEvent->SetTag(std::string("IP"), mDevIp[curname]);
        metricEvent->SetTag(std::string("m"), std::string("system.net_original"));
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();

        std::vector<std::string> packRateNames = {};

        std::vector<double> packRateValues = {};

        auto& rateCalculator = mRatePerSecCalMap[curname];
        ResNetRatePerSec minRatePerSec, maxRatePerSec, avgRatePerSec;
        rateCalculator.Stat(maxRatePerSec, minRatePerSec, avgRatePerSec);
        rateCalculator.Reset();
        std::vector<std::pair<std::string, double>> rateEntries = {
            {"networkout_packages_min", minRatePerSec.txPackRate},
            {"networkout_packages_max", maxRatePerSec.txPackRate},
            {"networkout_packages_avg", avgRatePerSec.txPackRate},
            {"networkin_packages_min", minRatePerSec.rxPackRate},
            {"networkin_packages_max", maxRatePerSec.rxPackRate},
            {"networkin_packages_avg", avgRatePerSec.rxPackRate},
            {"networkout_errorpackages_min", minRatePerSec.txErrorRate},
            {"networkout_errorpackages_max", maxRatePerSec.txErrorRate},
            {"networkout_errorpackages_avg", avgRatePerSec.txErrorRate},
            {"networkin_errorpackages_min", minRatePerSec.rxErrorRate},
            {"networkin_errorpackages_max", maxRatePerSec.rxErrorRate},
            {"networkin_errorpackages_avg", avgRatePerSec.rxErrorRate},
            {"networkout_rate_min", minRatePerSec.txByteRate},
            {"networkout_rate_max", maxRatePerSec.txByteRate},
            {"networkout_rate_avg", avgRatePerSec.txByteRate},
            {"networkin_rate_min", minRatePerSec.rxByteRate},
            {"networkin_rate_max", maxRatePerSec.rxByteRate},
            {"networkin_rate_avg", avgRatePerSec.rxByteRate},
            {"networkout_droppackages_min", minRatePerSec.txDropRate},
            {"networkout_droppackages_max", maxRatePerSec.txDropRate},
            {"networkout_droppackages_avg", avgRatePerSec.txDropRate},
            {"networkin_droppackages_min", minRatePerSec.rxDropRate},
            {"networkin_droppackages_max", maxRatePerSec.rxDropRate},
            {"networkin_droppackages_avg", avgRatePerSec.rxDropRate},
        };
        for (auto& entry : rateEntries) {
            packRateNames.push_back(entry.first);
            packRateValues.push_back(entry.second);
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
    listenEvent->SetTag(std::string("m"), std::string("system.tcp"));
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
    establishedEvent->SetTag(std::string("m"), std::string("system.tcp"));
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
    nonestablishedEvent->SetTag(std::string("m"), std::string("system.tcp"));
    nonestablishedEvent->SetValue<UntypedMultiDoubleValues>(nonestablishedEvent);
    auto* nonestablishedMultiDoubleValues = nonestablishedEvent->MutableValue<UntypedMultiDoubleValues>();
    nonestablishedMultiDoubleValues->SetValue(std::string("net_tcpconnection_min"),
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(minTCP.tcpNonEstablished)});

    nonestablishedMultiDoubleValues->SetValue(std::string("net_tcpconnection_max"),
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(maxTCP.tcpNonEstablished)});

    nonestablishedMultiDoubleValues->SetValue(std::string("net_tcpconnection_avg"),
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(avgTCP.tcpNonEstablished)});

    MetricEvent* totalEvent = group->AddMetricEvent(true);
    if (!totalEvent) {
        mLastTime = start;
        return false;
    }
    totalEvent->SetTimestamp(now, 0);
    totalEvent->SetTag(std::string("state"), std::string("TCP_TOTAL"));
    totalEvent->SetTag(std::string("m"), std::string("system.tcp"));
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


} // namespace logtail
