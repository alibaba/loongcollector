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

#include "host_monitor/collector/BaseCollector.h"

namespace logtail {

// man proc: https://man7.org/linux/man-pages/man5/proc.5.html
// search key: /proc/stat
enum class EnumCpuKey : int {
    user = 1,
    nice,
    system,
    idle,
    iowait, // since Linux 2.5.41
    irq, // since Linux 2.6.0
    softirq, // since Linux 2.6.0
    steal, // since Linux 2.6.11
    guest, // since Linux 2.6.24
    guest_nice, // since Linux 2.6.33
};

struct CPUStat {
    int32_t index; // -1 means total cpu
    double user;
    double nice;
    double system;
    double idle;
    double iowait;
    double irq;
    double softirq;
    double steal;
    double guest;
    double guestNice;
};

class CPUCollector : public BaseCollector {
public:
    ~CPUCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    bool GetHostSystemCPUStat(std::vector<CPUStat>& cpus);
    double ParseMetric(const std::vector<std::string>& cpuMetric, EnumCpuKey key) const;
};

} // namespace logtail
