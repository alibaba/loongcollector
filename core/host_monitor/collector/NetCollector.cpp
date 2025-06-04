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

const static int NET_INTERFACE_LIST_MAX = 20;

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

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    if (!(SystemInterface::GetInstance()->GetTCPStatInformation(resTCPStat)
          && SystemInterface::GetInstance()->GetNetRateInformation(netInterfaceMetrics))) {
        mLastTime = start;
        return false;
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
