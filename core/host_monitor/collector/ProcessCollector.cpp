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

#include "host_monitor/collector/ProcessCollector.h"

#include <grp.h>
#include <pwd.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "host_monitor/Constants.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/common/FastFieldParser.h"
#include "logger/Logger.h"

namespace logtail {

DEFINE_FLAG_INT32(host_monitor_process_report_top_N, "number of process reported with Top N cpu percent", 5);
DEFINE_FLAG_INT32(basic_host_monitor_process_collect_interval,
                  "basic host monitor process collect interval, seconds",
                  5);
#define PATH_MAX 4096

// topN进行的缓存为55s
const std::chrono::seconds ProcessSortInterval{55};

const std::string ProcessCollector::sName = "process";
const std::string kMetricLabelProcess = "valueTag";
const std::string kMetricLabelMode = "mode";

std::string GetPathBase(std::string filePath) {
    if (filePath.empty()) {
        return ".";
    }

    if (filePath.back() == '\\' || filePath.back() == '/') {
        filePath.pop_back();
    }
    if (!filePath.empty()) {
        std::filesystem::path path{filePath};
        if (path.has_filename()) {
            return path.filename().string();
        }
    }

    const char sep = std::filesystem::path::preferred_separator;
    return {&sep, &sep + 1};
}

static inline void GetProcessCpuSorted(std::vector<ProcessAllStat>& allPidStats) {
    std::sort(allPidStats.begin(), allPidStats.end(), [](const ProcessAllStat& a, const ProcessAllStat& b) {
        return a.processCpu.percent > b.processCpu.percent;
    });
}

ProcessCollector::ProcessCollector() : mTopN(INT32_FLAG(host_monitor_process_report_top_N)) {
}

bool ProcessCollector::Init(HostMonitorContext& collectContext) {
    if (!BaseCollector::Init(collectContext)) {
        return false;
    }

    MemoryInformation meminfo;
    if (!SystemInterface::GetInstance()->GetHostMemInformationStat(collectContext.GetMetricTime(), meminfo)) {
        return false;
    }
    mTotalMemory = meminfo.memStat.total;
    return true;
}

bool ProcessCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    collectContext.mCount++;

    ProcessListInformation processListInfo;

    if (!SystemInterface::GetInstance()->GetProcessListInformation(collectContext.GetMetricTime(), processListInfo)) {
        return false;
    }

    pids = processListInfo.pids;

    // 获取每一个进程的信息
    std::vector<ProcessAllStat> allPidStats;
    for (auto pid : pids) {
        ProcessAllStat stat;
        if (!GetProcessAllStat(collectContext.mCollectTime, pid, stat)) {
            continue;
        }
        allPidStats.push_back(stat);
    }

    // GetProcessCpuSorted(allPidStats);

    int processNum = allPidStats.size();

    VMProcessNumStat processNumStat;
    processNumStat.vmProcessNum = processNum;

    std::vector<ProcessPushMertic> pushMerticList;
    for (auto& stat : allPidStats) {
        // 生成每个pid统计信息的推送对象
        ProcessPushMertic pushMertic;
        pushMertic.pid = stat.pid;
        pushMertic.allNumProcess = processNum;
        pushMertic.fdNum = stat.fdNum;
        pushMertic.numThreads = stat.processState.numThreads;
        pushMertic.memPercent = stat.memPercent;
        pushMertic.cpuPercent = stat.processCpu.percent;
        pushMerticList.push_back(pushMertic);
    }

    // set calculation
    // 为vmState添加多值计算
    mVMProcessNumStat.AddValue(processNumStat);
    // 给每个pid推送对象设定其多值体系
    // mProcessPushMertic是一个字典，key
    // 为pid，对应的value为多值vector，里面存储了每一个pid的多值体系
    for (auto& metric : pushMerticList) {
        uint64_t thisPid = metric.pid;
        if (mProcessPushMertic.find(thisPid) != mProcessPushMertic.end()) {
            // 这个pid存在了
            // 多值添加对象
            mProcessPushMertic[thisPid].AddValue(metric);
        } else {
            // 这个pid不存在，需要创建一个多值体系
            MetricCalculate<ProcessPushMertic> mcObj{};
            mProcessPushMertic.insert(std::make_pair(thisPid, mcObj));
            // 多值添加对象
            mProcessPushMertic[thisPid].AddValue(metric);
        }
    }

    if (collectContext.mCount < collectContext.mCountPerReport) {
        return true;
    }

    GetProcessCpuSorted(allPidStats); // 排序

    // 记录count满足条件以后，计算并推送多值指标；如果没有到达条件，只需要往多值体系内添加统计对象即可
    VMProcessNumStat minVMProcessNum, maxVMProcessNum, avgVMProcessNum, lastVMProcessNum;
    mVMProcessNumStat.Stat(minVMProcessNum, maxVMProcessNum, avgVMProcessNum, &lastVMProcessNum);

    // 指标推送
    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    metricEvent->SetTimestamp(processListInfo.collectTime, 0);
    metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
    auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();
    std::vector<std::string> vmNames = {
        "vm_process_min",
        "vm_process_max",
        "vm_process_avg",
    };
    std::vector<double> vmValues = {
        minVMProcessNum.vmProcessNum,
        maxVMProcessNum.vmProcessNum,
        avgVMProcessNum.vmProcessNum,
    };
    // vm的系统信息上传
    for (size_t i = 0; i < vmNames.size(); i++) {
        multiDoubleValues->SetValue(std::string(vmNames[i]),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, vmValues[i]});
    }
    metricEvent->SetTag(std::string("m"), std::string("system.processCount"));

    // 每个pid一条记录上报
    for (size_t i = 0; i < mTopN && i < pushMerticList.size(); i++) {
        MetricEvent* metricEventEachPid = group->AddMetricEvent(true);
        metricEventEachPid->SetTimestamp(processListInfo.collectTime, 0);
        metricEventEachPid->SetValue<UntypedMultiDoubleValues>(metricEventEachPid);
        auto* multiDoubleValuesEachPid = metricEventEachPid->MutableValue<UntypedMultiDoubleValues>();
        // 上传每一个pid对应的值
        double value = 0.0;
        pid_t pid = pushMerticList[i].pid;

        // 计算pid的多值信息
        ProcessPushMertic minMetric, maxMetric, avgMetric;
        mProcessPushMertic[pid].Stat(minMetric, maxMetric, avgMetric);

        // 获取pid对应的processinfo
        ProcessInfo processInfo;
        if (!GetProcessInfo(collectContext.mCollectTime.mMetricTime, pid, processInfo)) {
            continue;
        }

        // cpu percent
        value = static_cast<double>(avgMetric.cpuPercent);
        multiDoubleValuesEachPid->SetValue(std::string("process_cpu_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // mem percent
        value = static_cast<double>(avgMetric.memPercent);
        multiDoubleValuesEachPid->SetValue(std::string("process_memory_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // open file number
        value = static_cast<double>(avgMetric.fdNum);
        multiDoubleValuesEachPid->SetValue(std::string("process_openfile_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // process number
        value = static_cast<double>(avgMetric.numThreads);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(maxMetric.numThreads);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_max"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(minMetric.numThreads);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_min"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        metricEventEachPid->SetTag("pid", std::to_string(pid));
        metricEventEachPid->SetTag("name", processInfo.name);
        metricEventEachPid->SetTag("user", processInfo.user);
        metricEventEachPid->SetTag(std::string("m"), std::string("system.process"));
    }

    // 清空所有多值体系，因为有的pid后面可能会消失
    collectContext.mCount = 0;
    mVMProcessNumStat.Reset();
    mProcessPushMertic.clear();
    mAvgProcessCpuPercent.clear();
    mAvgProcessMemPercent.clear();
    mAvgProcessFd.clear();
    mMinProcessNumThreads.clear();
    mMaxProcessNumThreads.clear();
    mAvgProcessNumThreads.clear();
    pidNameMap.clear();
    pushMerticList.clear();
    ClearProcessCpuTimeCache();
    return true;
}

// 获取某个pid的信息
bool ProcessCollector::GetProcessAllStat(const CollectTime& collectTime, pid_t pid, ProcessAllStat& processStat) {
    // 获取这个pid的cpu信息
    processStat.pid = pid;

    if (!GetProcessCpuInformation(collectTime, pid, processStat.processCpu)) {
        return false;
    }

    if (!GetProcessState(collectTime.mMetricTime, pid, processStat.processState)) {
        return false;
    }

    if (!GetProcessMemory(collectTime.mMetricTime, pid, processStat.processMemory)) {
        return false;
    }

    ProcessFd procFd;
    if (!GetProcessFdNumber(collectTime.mMetricTime, pid, procFd)) {
        return false;
    }
    processStat.fdNum = procFd.total;
    processStat.fdNumExact = procFd.exact;

    processStat.memPercent = mTotalMemory == 0 ? 0 : 100.0 * processStat.processMemory.resident / mTotalMemory;
    return true;
}

bool ProcessCollector::GetProcessCredName(time_t now, pid_t pid, ProcessCredName& processCredName) {
    if (!SystemInterface::GetInstance()->GetProcessCredNameObj(now, pid, processCredName)) {
        return false;
    }
    return true;
}

bool ProcessCollector::GetProcessArgs(time_t now, pid_t pid, std::vector<std::string>& args) {
    std::string cmdline;

    ProcessCmdlineString processCMDline;
    if (!SystemInterface::GetInstance()->GetProcessCmdlineString(now, pid, processCMDline)) {
        return false;
    }
    if (processCMDline.cmdline.empty()) {
        // /proc/pid/cmdline have no content
        return false;
    }
    cmdline = processCMDline.cmdline.front();

    FastFieldParser parser(cmdline);
    size_t fieldCount = parser.GetFieldCount();

    args.reserve(fieldCount);
    for (size_t i = 0; i < fieldCount; ++i) {
        auto field = parser.GetField(i);
        if (!field.empty()) {
            args.emplace_back(field);
        }
    }
    return true;
}

std::string ProcessCollector::GetExecutablePath(time_t now, pid_t pid) {
    ProcessExecutePath processExecutePath;
    if (!SystemInterface::GetInstance()->GetExecutablePathCache(now, pid, processExecutePath)) {
        return "";
    }
    return processExecutePath.path;
}

// argus 的进程path是通过readlink /proc/pid/exe 获取的
// args 是通过 /proc/pid/cmdline 获取的
bool ProcessCollector::GetProcessInfo(time_t now, pid_t pid, ProcessInfo& processInfo) {
    std::string user = "unknown";
    ProcessCredName processCredName;

    if (GetProcessCredName(now, pid, processCredName)) {
        user = processCredName.user;
    }

    std::vector<std::string> args;
    if (GetProcessArgs(now, pid, args)) {
        processInfo.args = args.front();
        for (size_t i = 1; i < args.size(); i++) {
            processInfo.args += " " + args[i];
        }
    }

    processInfo.path = GetExecutablePath(now, pid);
    processCredName.name = GetPathBase(processInfo.path);
    if (processCredName.name == ".") {
        processCredName.name = "unknown";
    }

    processInfo.pid = pid;
    processInfo.name = processCredName.name;
    processInfo.user = user;

    return true;
}

// 获取进程文件数信息
bool ProcessCollector::GetProcessFdNumber(time_t now, pid_t pid, ProcessFd& processFd) {
    if (!SystemInterface::GetInstance()->GetProcessOpenFiles(now, pid, processFd)) {
        return false;
    }

    return true;
}

// 获取pid的内存信息
bool ProcessCollector::GetProcessMemory(time_t now, pid_t pid, ProcessMemoryInformation& processMemory) {
    ProcessInformation processInfo;

    if (!ReadProcessStat(now, pid, processInfo)) {
        return false;
    }
    processMemory.minorFaults = processInfo.stat.minorFaults;
    processMemory.majorFaults = processInfo.stat.majorFaults;
    processMemory.pageFaults = processInfo.stat.minorFaults + processInfo.stat.majorFaults;

    if (!SystemInterface::GetInstance()->GetPorcessStatm(now, pid, processMemory)) {
        return false;
    }

    return true;
}

// 获取pid的状态信息
bool ProcessCollector::GetProcessState(time_t now, pid_t pid, ProcessStat& processState) {
    ProcessInformation processInfo;

    if (!ReadProcessStat(now, pid, processInfo)) {
        return false;
    }

    processState.state = processInfo.stat.state;
    processState.tty = processInfo.stat.tty;
    processState.parentPid = processInfo.stat.parentPid;
    processState.priority = processInfo.stat.priority;
    processState.nice = processInfo.stat.nice;
    processState.processor = processInfo.stat.processor;
    processState.numThreads = processInfo.stat.numThreads;

    return true;
}

// 给pid做cache
bool ProcessCollector::GetProcessCpuInCache(pid_t pid) {
    if (cpuTimeCache.find(pid) != cpuTimeCache.end()) {
        return true;
    } else {
        return false;
    }
}


bool ProcessCollector::GetProcessCpuInformation(const CollectTime& collectTime,
                                                pid_t pid,
                                                ProcessCpuInformation& information) {
    bool findCache = false;
    ProcessCpuInformation* prev = nullptr;

    // 由于计算CPU时间需要获取一个时间间隔
    // 但是我们这里不应该睡眠，因此只能做一个cache，保存上一次获取的数据
    findCache = GetProcessCpuInCache(pid);

    ProcessTime processTime{};

    if (!GetProcessTime(collectTime.mMetricTime, pid, processTime)) {
        return false;
    }
    information.lastTime = collectTime.GetShiftSteadyTime(processTime.collectTime);

    if (findCache) {
        // cache found, calculate the cpu percent
        auto recordedEntity = cpuTimeCache.find(pid);
        if (recordedEntity != cpuTimeCache.end()) {
            prev = &recordedEntity->second;
        }
    } else {
        information.percent = 0.0;
        information.sys = processTime.sys.count();
        information.user = processTime.user.count();
        information.total = processTime.total.count();
        cpuTimeCache[pid] = information;
        return true;
    }

    int64_t timeDiff
        = std::chrono::duration_cast<std::chrono::milliseconds>(information.lastTime - prev->lastTime).count();

    // update the cache
    using namespace std::chrono;
    information.startTime = processTime.startTime;
    information.user = processTime.user.count();
    information.sys = processTime.sys.count();
    information.total = processTime.total.count();

    // calculate cpuPercent = (thisTotal - prevTotal)/HZ;
    auto totalCPUDiff = static_cast<double>(information.total - prev->total) / SYSTEM_HERTZ;
    information.percent = 100 * totalCPUDiff / (static_cast<double>(timeDiff) / SYSTEM_HERTZ); // 100%
    cpuTimeCache[pid] = information;
    return true;
}

bool ProcessCollector::GetProcessTime(time_t now, pid_t pid, ProcessTime& output) {
    ProcessInformation processInfo;

    if (!ReadProcessStat(now, pid, processInfo)) {
        return false;
    }

    output.startTime = processInfo.stat.startTicks;

    output.cutime = std::chrono::milliseconds(processInfo.stat.cutimeTicks);
    output.cstime = std::chrono::milliseconds(processInfo.stat.cstimeTicks);
    output.user = std::chrono::milliseconds(processInfo.stat.utimeTicks + processInfo.stat.cutimeTicks);
    output.sys = std::chrono::milliseconds(processInfo.stat.stimeTicks + processInfo.stat.cstimeTicks);

    output.total = std::chrono::milliseconds(output.user + output.sys);

    output.collectTime = processInfo.collectTime;
    return true;
}

// 数据样例: /proc/1/stat, 解析/proc/pid/stat
// 1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600
// 171 18446744073709551615 4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17
// 3 0 0 0 0 0 6336016 6337300 21442560 140727020027760 140727020027777
// 140727020027777 140727020027887 0
bool ProcessCollector::ReadProcessStat(time_t now, pid_t pid, ProcessInformation& processInfo) {
    if (!SystemInterface::GetInstance()->GetProcessInformation(now, pid, processInfo)) {
        return false;
    }
    return true;
}

void ProcessCollector::ClearProcessCpuTimeCache() {
    try {
        // 清除超时的cache
        const auto now = std::chrono::steady_clock::now();
        auto it = cpuTimeCache.begin();

        while (it != cpuTimeCache.end()) {
            // 检查当前元素是否超时
            if (now - it->second.lastTime > ProcessSortInterval) {
                // 超时，删除该元素
                it = cpuTimeCache.erase(it);
            } else {
                // 未超时，继续检查下一个元素
                ++it;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("ClearProcessCpuTimeCache error", e.what()));
    }

    return;
}

const std::chrono::seconds ProcessCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_process_collect_interval));
}

} // namespace logtail
