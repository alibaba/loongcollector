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
#include <string>

#include "common/StringView.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/collector/CollectorConstants.h"
#include "models/MetricValue.h"

DEFINE_FLAG_INT32(basic_host_monitor_net_collect_interval, "basic host monitor net collect interval, seconds", 1);

namespace logtail {

const std::string NetCollector::sName = "net";

bool NetCollector::Init(HostMonitorContext& collectContext) {
    if (!BaseCollector::Init(collectContext)) {
        return false;
    }
    mLastTime = std::chrono::steady_clock::now();
    return true;
}

bool NetCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    TCPStatInformation resTCPStat;
    NetInterfaceInformation netInterfaces;

    std::chrono::steady_clock::time_point start = collectContext.GetScheduleTime();

    if (!(SystemInterface::GetInstance()->GetTCPStatInformation(collectContext.GetMetricTime(), resTCPStat)
          && SystemInterface::GetInstance()->GetNetInterfaceInformation(collectContext.GetMetricTime(),
                                                                        netInterfaces))) {
        mLastTime = start;
        return false;
    }

    std::vector<std::string> curDevNames; // 本次采集到的所有设备名

    // 更新记录ip
    for (auto& netInterface : netInterfaces.configs) {
        if (netInterface.name.empty()) {
            continue;
        }
        curDevNames.push_back(netInterface.name);
        mDevIp[netInterface.name] = netInterface.address.str();
        if (mDevIp[netInterface.name].empty()) {
            mDevIp[netInterface.name] = netInterface.address6.str();
        }
    }

    double interval = std::chrono::duration_cast<std::chrono::duration<double>>(start - mLastTime).count();

    // tcp
    mTCPCal.AddValue(resTCPStat.stat);

    // rate
    for (auto& netInterfaceMetric : netInterfaces.metrics) {
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

    // If group is not provided, just collect data without generating metrics
    if (!groupPtr) {
        mLastTime = start;
        return true;
    }

    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;

    // 入方向、出方向 的 丢包率
    // 每秒发、收 的 字节数、包数

    for (auto& packRateCal : mRatePerSecCalMap) {
        std::string curname = packRateCal.first;

        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        if (!metricEvent) {
            mLastTime = start;
            return false;
        }

        metricEvent->SetTimestamp(netInterfaces.collectTime, 0);
        const StringBuffer& hostnameBuffer = metricEvent->GetSourceBuffer()->CopyString(hostname);
        metricEvent->SetTagNoCopy(kTagKeyHostname, StringView(hostnameBuffer.data, hostnameBuffer.size));
        const StringBuffer& curnameBuffer = metricEvent->GetSourceBuffer()->CopyString(curname);
        metricEvent->SetTagNoCopy(kTagKeyDevice, StringView(curnameBuffer.data, curnameBuffer.size));
        const StringBuffer& ipBuffer = metricEvent->GetSourceBuffer()->CopyString(mDevIp[curname]);
        metricEvent->SetTagNoCopy(kTagKeyIp, StringView(ipBuffer.data, ipBuffer.size));

        metricEvent->SetTagNoCopy(kTagKeyM, kMetricSystemNetOriginal);
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();

        auto& rateCalculator = mRatePerSecCalMap[curname];
        ResNetRatePerSec minRatePerSec, maxRatePerSec, avgRatePerSec;
        rateCalculator.Stat(maxRatePerSec, minRatePerSec, avgRatePerSec);
        rateCalculator.Reset();

        struct MetricDef {
            StringView name;
            double value;
        } rateMetrics[] = {
            {kNetworkoutPackagesMin, minRatePerSec.txPackRate},
            {kNetworkoutPackagesMax, maxRatePerSec.txPackRate},
            {kNetworkoutPackagesAvg, avgRatePerSec.txPackRate},
            {kNetworkinPackagesMin, minRatePerSec.rxPackRate},
            {kNetworkinPackagesMax, maxRatePerSec.rxPackRate},
            {kNetworkinPackagesAvg, avgRatePerSec.rxPackRate},
            {kNetworkoutErrorpackagesMin, minRatePerSec.txErrorRate},
            {kNetworkoutErrorpackagesMax, maxRatePerSec.txErrorRate},
            {kNetworkoutErrorpackagesAvg, avgRatePerSec.txErrorRate},
            {kNetworkinErrorpackagesMin, minRatePerSec.rxErrorRate},
            {kNetworkinErrorpackagesMax, maxRatePerSec.rxErrorRate},
            {kNetworkinErrorpackagesAvg, avgRatePerSec.rxErrorRate},
            {kNetworkoutRateMin, minRatePerSec.txByteRate},
            {kNetworkoutRateMax, maxRatePerSec.txByteRate},
            {kNetworkoutRateAvg, avgRatePerSec.txByteRate},
            {kNetworkinRateMin, minRatePerSec.rxByteRate},
            {kNetworkinRateMax, maxRatePerSec.rxByteRate},
            {kNetworkinRateAvg, avgRatePerSec.rxByteRate},
            {kNetworkoutDroppackagesMin, minRatePerSec.txDropRate},
            {kNetworkoutDroppackagesMax, maxRatePerSec.txDropRate},
            {kNetworkoutDroppackagesAvg, avgRatePerSec.txDropRate},
            {kNetworkinDroppackagesMin, minRatePerSec.rxDropRate},
            {kNetworkinDroppackagesMax, maxRatePerSec.rxDropRate},
            {kNetworkinDroppackagesAvg, avgRatePerSec.rxDropRate},
        };

        for (const auto& def : rateMetrics) {
            multiDoubleValues->SetValue(def.name,
                                        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, def.value});
        }
    }


    // TCP各种状态下的连接数
    ResTCPStat minTCP, maxTCP, avgTCP;
    mTCPCal.Stat(maxTCP, minTCP, avgTCP);
    mTCPCal.Reset();

    MetricEvent* listenEvent = groupPtr->AddMetricEvent(true);
    if (!listenEvent) {
        mLastTime = start;
        return false;
    }
    listenEvent->SetTimestamp(resTCPStat.collectTime, 0);
    listenEvent->SetTagNoCopy(kTagKeyState, kTcpStateListen);
    listenEvent->SetTagNoCopy(kTagKeyM, kMetricSystemTcp);
    listenEvent->SetValue<UntypedMultiDoubleValues>(listenEvent);
    auto* listenMultiDoubleValues = listenEvent->MutableValue<UntypedMultiDoubleValues>();
    listenMultiDoubleValues->SetValue(
        kNetTcpConnectionMin,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpListen)});

    listenMultiDoubleValues->SetValue(
        kNetTcpConnectionMax,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpListen)});

    listenMultiDoubleValues->SetValue(
        kNetTcpConnectionAvg,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpListen)});

    MetricEvent* establishedEvent = groupPtr->AddMetricEvent(true);
    if (!establishedEvent) {
        mLastTime = start;
        return false;
    }
    establishedEvent->SetTimestamp(resTCPStat.collectTime, 0);
    establishedEvent->SetTagNoCopy(kTagKeyState, kTcpStateEstablished);
    establishedEvent->SetTagNoCopy(kTagKeyM, kMetricSystemTcp);
    establishedEvent->SetValue<UntypedMultiDoubleValues>(establishedEvent);
    auto* establishedMultiDoubleValues = establishedEvent->MutableValue<UntypedMultiDoubleValues>();
    establishedMultiDoubleValues->SetValue(
        kNetTcpConnectionMin,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpEstablished)});

    establishedMultiDoubleValues->SetValue(
        kNetTcpConnectionMax,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpEstablished)});

    establishedMultiDoubleValues->SetValue(
        kNetTcpConnectionAvg,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpEstablished)});

    MetricEvent* nonestablishedEvent = groupPtr->AddMetricEvent(true);
    if (!nonestablishedEvent) {
        mLastTime = start;
        return false;
    }
    nonestablishedEvent->SetTimestamp(resTCPStat.collectTime, 0);
    nonestablishedEvent->SetTagNoCopy(kTagKeyState, kTcpStateNonEstablished);
    nonestablishedEvent->SetTagNoCopy(kTagKeyM, kMetricSystemTcp);
    nonestablishedEvent->SetValue<UntypedMultiDoubleValues>(nonestablishedEvent);
    auto* nonestablishedMultiDoubleValues = nonestablishedEvent->MutableValue<UntypedMultiDoubleValues>();
    nonestablishedMultiDoubleValues->SetValue(kNetTcpConnectionMin,
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(minTCP.tcpNonEstablished)});

    nonestablishedMultiDoubleValues->SetValue(kNetTcpConnectionMax,
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(maxTCP.tcpNonEstablished)});

    nonestablishedMultiDoubleValues->SetValue(kNetTcpConnectionAvg,
                                              UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge,
                                                                      static_cast<double>(avgTCP.tcpNonEstablished)});

    MetricEvent* totalEvent = groupPtr->AddMetricEvent(true);
    if (!totalEvent) {
        mLastTime = start;
        return false;
    }
    totalEvent->SetTimestamp(resTCPStat.collectTime, 0);
    totalEvent->SetTagNoCopy(kTagKeyState, kTcpStateTotal);
    totalEvent->SetTagNoCopy(kTagKeyM, kMetricSystemTcp);
    totalEvent->SetValue<UntypedMultiDoubleValues>(totalEvent);
    auto* totalMultiDoubleValues = totalEvent->MutableValue<UntypedMultiDoubleValues>();
    totalMultiDoubleValues->SetValue(
        kNetTcpConnectionMin,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(minTCP.tcpTotal)});

    totalMultiDoubleValues->SetValue(
        kNetTcpConnectionMax,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(maxTCP.tcpTotal)});

    totalMultiDoubleValues->SetValue(
        kNetTcpConnectionAvg,
        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(avgTCP.tcpTotal)});

    mLastTime = start;

    // 清理掉mLastInterfaceMetrics中，curDevNames中不存在的设备名
    for (auto it = mLastInterfaceMetrics.begin(); it != mLastInterfaceMetrics.end();) {
        if (std::find(curDevNames.begin(), curDevNames.end(), it->first) == curDevNames.end()) {
            it = mLastInterfaceMetrics.erase(it);
        } else {
            ++it;
        }
    }

    // 清理掉mRatePerSecCalMap中，curDevNames中不存在的设备名
    for (auto it = mRatePerSecCalMap.begin(); it != mRatePerSecCalMap.end();) {
        if (std::find(curDevNames.begin(), curDevNames.end(), it->first) == curDevNames.end()) {
            it = mRatePerSecCalMap.erase(it);
        } else {
            ++it;
        }
    }

    // 清理掉mDevIp中，curDevNames中不存在的设备名
    for (auto it = mDevIp.begin(); it != mDevIp.end();) {
        if (std::find(curDevNames.begin(), curDevNames.end(), it->first) == curDevNames.end()) {
            it = mDevIp.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

const std::chrono::seconds NetCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_net_collect_interval));
}

} // namespace logtail
