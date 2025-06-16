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

#include "host_monitor/collector/MemCollector.h"

#include <filesystem>
#include <string>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "logger/Logger.h"

#define Diff(a, b)  a-b>0 ? a-b : 0;

namespace logtail {

const std::string MemCollector::sName = "Memory";
const std::string kMetricLabelMem = "valueTag";
const std::string kMetricLabelMode = "mode";

MemCollector::MemCollector(){
    Init();
}

int MemCollector::Init(int totalCount) {
    mTotalCount = totalCount;
    mCount = 0;
    return 0;
}

bool MemCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }

    MemoryInformation memStat;
    SwapInformation swapStat;
    if (!GetHostMeminfoStat(memStat, swapStat)) {
        return false;
    }

    mCalculateMeminfo.AddValue(memStat);
    mCount++;
    if (mCount < mTotalCount) {
        return true;
    }
    MemoryInformation minMem,maxMem,avgMem,lastMem;

    mCalculateMeminfo.Stat(maxMem, minMem, avgMem, &lastMem);

    mCount=0;
    mCalculateMeminfo.Reset();
    
    const time_t now = time(nullptr);

    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(now, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    multiDoubleValues->SetValue(std::string("memory_usedutilization_min"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_usedutilization_max"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_usedutilization_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.usedPercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_min"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_max"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_freeutilization_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.freePercent});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_min"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_max"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_actualusedspace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.actualUsed});
    multiDoubleValues->SetValue(std::string("memory_freespace_min"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.free});
    multiDoubleValues->SetValue(std::string("memory_freespace_max"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.free});
    multiDoubleValues->SetValue(std::string("memory_freespace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.free});
    multiDoubleValues->SetValue(std::string("memory_usedspace_min"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, minMem.used});
    multiDoubleValues->SetValue(std::string("memory_usedspace_max"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, maxMem.used});
    multiDoubleValues->SetValue(std::string("memory_usedspace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.used});
    multiDoubleValues->SetValue(std::string("memory_totalspace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});
    multiDoubleValues->SetValue(std::string("memory_totalspace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});
    multiDoubleValues->SetValue(std::string("memory_totalspace_avg"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, avgMem.total});                                
    
    return true;
}

bool MemCollector::GetHostMeminfoStat(MemoryInformation& memStat, SwapInformation& swapStat) {
    MemoryInformationString meminfoStr;

    if (!SystemInterface::GetInstance()->GetHostMeminfoStatString(meminfoStr)) {
        return false;
    }

    mValidState = true;

    if (!GetMemoryStat(memStat, meminfoStr.meminfoString)) {
        return false;
    }

    // Swap is not needed right now
    if (!GetSwapStat(swapStat, meminfoStr.meminfoString)) {
        return false;
    }

    return true;
}

/*
样例: /proc/meminfo:
MemTotal:        4026104 kB
MemFree:         2246280 kB
MemAvailable:    3081592 kB // 低版本Linux内核上可能没有该行
Buffers:          124380 kB
Cached:          1216756 kB
SwapCached:            0 kB
Active:           417452 kB
Inactive:        1131312 kB
 */
bool MemCollector::GetMemoryStat(MemoryInformation& information, std::vector<std::string>& memoryLines) {
    int ret = false;

    if (memoryLines.empty()) {
        return ret;
    }

    std::unordered_map<std::string, double &> memoryProc{
            {"MemTotal:",     information.total},
            {"MemFree:",      information.free},
            {"MemAvailable:", information.available},
            {"Buffers:",      information.buffers},
            {"Cached:",       information.cached},
    };
    /* 字符串处理，处理成对应的类型以及值*/
    for (size_t i = 0; i < memoryLines.size() && !memoryProc.empty(); i++) {
        std::vector<std::string> words = split(memoryLines[i], ' ', true);
        // words-> MemTotal: / 12344 / kB

        auto entry = memoryProc.find(words[0]);
        if (entry != memoryProc.end()) {
            entry->second = GetMemoryValue(words.back()[0], std::stoi(words[1]));
            memoryProc.erase(entry);
        }
    }
    information.used = Diff(information.total, information.free);
    completeMemoryInformation(information);

    return true;
}

int MemCollector::GetSwapStat(SwapInformation& swap, std::vector<std::string>& memoryLines) {
    int ret = false;

    if (memoryLines.empty()) {
        return ret;
    }

    std::unordered_map<std::string, double &> swapProc{
            {"SwapTotal:",     swap.total},
            {"SwapFree:",      swap.free},
    };

    /* 字符串处理，处理成对应的类型以及值*/
    for (size_t i = 0; i < memoryLines.size() && !swapProc.empty(); i++) {
        std::vector<std::string> words = split(memoryLines[i], ' ', true);
        // words-> MemTotal: / 12344 / kB

        auto entry = swapProc.find(words[0]);
        if (entry != swapProc.end()) {
            entry->second = GetMemoryValue(words.back()[0], std::stoi(words[1]));
            swapProc.erase(entry);
        }
    }

    swap.used = Diff(swap.total, swap.free);
    swap.pageIn = swap.pageOut = -1;

    return GetSwapPageInfo(swap);
}

uint64_t MemCollector::GetMemoryValue(char unit, uint64_t value) {
    if (unit == 'k' || unit == 'K') {
        value *= 1024;
    } else if (unit == 'm' || unit == 'M') {
        value *= 1024 * 1024;
    }
    return value;
}

void MemCollector::completeMemoryInformation(MemoryInformation &memInfo) {
    std::vector<std::string> errorMessage;
    const uint64_t mb = 1024 * 1024;
    // 不需要考虑MemAvailable不存在的情况
    memInfo.actualUsed = Diff(memInfo.total, memInfo.available);
    memInfo.actualFree = memInfo.available;
    memInfo.usedPercent =
            memInfo.total > 0 ? static_cast<double>(memInfo.actualUsed) * 100 / memInfo.total : 0.0;
    memInfo.freePercent =
            memInfo.total > 0 ? static_cast<double>(memInfo.actualFree) * 100 / memInfo.total : 0.0;
    memInfo.ram = memInfo.total / mb;

    uint64_t diff = Diff(memInfo.total, memInfo.actualFree);
    memInfo.usedPercent = memInfo.total > 0 ? static_cast<double>(diff) * 100 / memInfo.total : 0.0;
    diff = Diff(memInfo.total, memInfo.actualUsed);
    memInfo.freePercent = memInfo.total > 0 ? static_cast<double>(diff) * 100 / memInfo.total : 0.0;

}

bool MemCollector::GetMemoryRam(MemoryInformation &memInfo) {
    uint64_t ram = 0;
    MTRRInformationString mtrrInfo;
    
    if (!SystemInterface::GetInstance()->GetMTRRInformationString(mtrrInfo)) {
        return false;
    }

    ram = parseProcMtrr(mtrrInfo.mtrrString);
    memInfo.ram = ram;

    return true;
}

// /proc/mtrr
// reg00: base=0x00000000 (   0MB), size= 256MB: write-back, count=1
// reg01: base=0xe8000000 (3712MB), size=  32MB: write-combining, count=1
// /proc/mtrr格式2：
// [root@7227ded95607 ilogtail]# cat /proc/mtrr 
// reg00: base=0x000000000 (    0MB), size=262144MB, count=1: write-back
// reg01: base=0x4000000000 (262144MB), size=131072MB, count=1: write-back
// reg02: base=0x6000000000 (393216MB), size= 2048MB, count=1: write-back
// reg03: base=0x6070000000 (395008MB), size=  256MB, count=1: uncachable
// reg04: base=0x080000000 ( 2048MB), size= 2048MB, count=1: uncachable
// reg05: base=0x070000000 ( 1792MB), size=   64MB, count=1: uncachable
uint64_t MemCollector::parseProcMtrr(std::vector<std::string> &lines) {
    uint64_t ram = 0;
    for (auto const &line: lines) {
        if (line.find("write-back") == std::string::npos) {
            continue;
        }
        size_t start = line.find("size=");
        if (start != std::string::npos) {
            start += 5; // 5 -> strlen("size=")
            size_t end = line.find("MB", start);
            if (end != std::string::npos) {
                std::string str = TrimSpace(line.substr(start, end));
                ram += (std::stoi(str) * 1024 * 1024);
            }
        }
    }

    return ram;
}

int MemCollector::GetSwapPageInfo(SwapInformation& swap) {
    return 1;
}

} // namespace logtail
