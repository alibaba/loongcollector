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

#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <filesystem>
#include <string>
#include <vector>

#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "monitor/Monitor.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {

extern const uint32_t kHostMonitorMinInterval;
extern const uint32_t kHostMonitorDefaultInterval;


class NetCollector : public BaseCollector {
public:
    NetCollector() = default;
    ~NetCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;

    const std::string& Name() const override { return sName; }


private:
    std::chrono::steady_clock::time_point mLastTime;
    std::map<std::string, NetInterfaceMetric> mLastInterfaceMetrics;
    MetricCalculate<ResTCPStat, uint64_t> mTCPCal;
    std::map<std::string, MetricCalculate<ResNetRatePerSec>> mRatePerSecCalMap;
    std::map<std::string, std::string> mDevIp;
};


} // namespace logtail
