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
#include "logger/Logger.h"


namespace logtail {

DEFINE_FLAG_INT32(process_report_top_N, "number of process reported with Top N cpu percent", 5);
DEFINE_FLAG_INT32(process_total_count, "number of each calculate epoch to report", 3);
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

ProcessCollector::ProcessCollector() {
    Init(INT32_FLAG(process_total_count), INT32_FLAG(process_report_top_N));
}

static inline void GetProcessCpuSorted(std::vector<ProcessAllStat>& allPidStats) {
    std::sort(allPidStats.begin(), allPidStats.end(), [](const ProcessAllStat& a, const ProcessAllStat& b) {
        return a.processCpu.percent > b.processCpu.percent;
    });
}

int ProcessCollector::Init(int processTotalCount, int processReportTopN) {
    MemoryInformation meminfo;

    if (!SystemInterface::GetInstance()->GetHostMemInformationStat(meminfo)) {
        return false;
    }

    mTotalMemory = meminfo.memStat.total;
    mCountPerReport = processTotalCount;
    mTopN = processReportTopN;

    return 0;
}


bool ProcessCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }

    ProcessListInformation processListInfo;

    if (!SystemInterface::GetInstance()->GetProcessListInformation(processListInfo)) {
        return false;
    }

    pids = processListInfo.pids;

    time_t now = time(nullptr);

    // 获取每一个进程的信息
    std::vector<ProcessAllStat> allPidStats;
    for (auto pid : pids) {
        ProcessAllStat stat;
        if (!GetProcessAllStat(pid, stat)) {
            continue;
        }
        allPidStats.push_back(stat);
    }

    GetProcessCpuSorted(allPidStats);

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
        pushMertic.name = stat.processInfo.name;
        pushMertic.user = stat.processInfo.user;
        pushMertic.args = stat.processInfo.args;
        pushMertic.path = stat.processInfo.path;
        pushMerticList.push_back(pushMertic);
    }

    // set calculation
    // 为vmState添加多值计算
    mVMProcessNumStat.AddValue(processNumStat);
    // 给每个pid推送对象设定其多值体系
    // mProcessPushMertic是一个字典，key 为pid，对应的value为多值vector，里面存储了每一个pid的多值体系
    for (auto& metric : pushMerticList) {
        uint64_t thisPid = metric.pid;
        // auto met = mProcessPushMertic.find(thisPid);
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

    mCount++;
    if (mCount < mCountPerReport) {
        return true;
    }

    // 记录count满足条件以后，计算并推送多值指标；如果没有到达条件，只需要往多值体系内添加统计对象即可
    VMProcessNumStat minVMProcessNum, maxVMProcessNum, avgVMProcessNum, lastVMProcessNum;
    mVMProcessNumStat.Stat(minVMProcessNum, maxVMProcessNum, avgVMProcessNum, &lastVMProcessNum);

    for (auto& metric : pushMerticList) {
        // 处理每一个pid的推送数据
        uint64_t thisPid = metric.pid;

        // 分别计算每个指标下对应pid的多值,分别算入对应的指标对象
        ProcessPushMertic minMetric, maxMetric, avgMetric;
        mProcessPushMertic[thisPid].Stat(minMetric, maxMetric, avgMetric);
        // map，比如mAvgProcessCpuPercent，存储的是pid对应的CPU平均利用率
        mAvgProcessCpuPercent.insert(std::make_pair(thisPid, avgMetric.cpuPercent));
        mAvgProcessMemPercent.insert(std::make_pair(thisPid, avgMetric.memPercent));
        mAvgProcessFd.insert(std::make_pair(thisPid, avgMetric.fdNum));
        mMinProcessNumThreads.insert(std::make_pair(thisPid, minMetric.numThreads));
        mMaxProcessNumThreads.insert(std::make_pair(thisPid, maxMetric.numThreads));
        mAvgProcessNumThreads.insert(std::make_pair(thisPid, avgMetric.numThreads));
        // 每个pid下的多值体系添加完毕
    }

    // 指标推送
    MetricEvent* metricEvent = group->AddMetricEvent(true);
    if (!metricEvent) {
        return false;
    }
    // refresh the time point
    now = time(nullptr);
    metricEvent->SetTimestamp(now, 0);
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
        metricEventEachPid->SetTimestamp(now, 0);
        metricEventEachPid->SetValue<UntypedMultiDoubleValues>(metricEventEachPid);
        auto* multiDoubleValuesEachPid = metricEventEachPid->MutableValue<UntypedMultiDoubleValues>();
        // 上传每一个pid对应的值
        double value = 0.0;
        pid_t pid = pushMerticList[i].pid;
        // cpu percent
        value = static_cast<double>(mAvgProcessCpuPercent.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_cpu_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // mem percent
        value = static_cast<double>(mAvgProcessMemPercent.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_memory_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // open file number
        value = static_cast<double>(mAvgProcessFd.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_openfile_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // process number
        value = static_cast<double>(mAvgProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_avg"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(mMaxProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_max"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(mMinProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_min"),
                                           UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        metricEventEachPid->SetTag("pid", std::to_string(pid));
        metricEventEachPid->SetTag("name", pushMerticList[i].name);
        metricEventEachPid->SetTag("user", pushMerticList[i].user);
        metricEventEachPid->SetTag(std::string("m"), std::string("system.process"));
    }

    // 清空所有多值体系，因为有的pid后面可能会消失
    mCount = 0;
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
    return true;
}


// 获取某个pid的信息
bool ProcessCollector::GetProcessAllStat(pid_t pid, ProcessAllStat& processStat) {
    // 获取这个pid的cpu信息
    processStat.pid = pid;

    if (!GetProcessCpuInformation(pid, processStat.processCpu, false)) {
        return false;
    }

    if (!GetProcessState(pid, processStat.processState)) {
        return false;
    }

    if (!GetProcessMemory(pid, processStat.processMemory)) {
        return false;
    }

    ProcessFd procFd;
    if (!GetProcessFdNumber(pid, procFd)) {
        return false;
    }
    processStat.fdNum = procFd.total;
    processStat.fdNumExact = procFd.exact;

    if (!GetProcessInfo(pid, processStat.processInfo)) {
        return false;
    }

    processStat.memPercent = mTotalMemory == 0 ? 0 : 100.0 * processStat.processMemory.resident / mTotalMemory;
    return true;
}

bool ProcessCollector::GetProcessCredName(pid_t pid, ProcessCredName& processCredName) {
    if (!SystemInterface::GetInstance()->GetProcessCredNameObj(pid, processCredName)) {
        return false;
    }
    return true;
}

bool ProcessCollector::GetProcessArgs(pid_t pid, std::vector<std::string>& args) {
    std::string cmdline;

    ProcessCmdlineString processCMDline;
    if (!SystemInterface::GetInstance()->GetProcessCmdlineString(pid, processCMDline)) {
        return false;
    }
    if (processCMDline.cmdline.empty()) {
        // /proc/pid/cmdline have no content
        return false;
    }
    cmdline = processCMDline.cmdline.front();
    std::vector<std::string> cmdlineMetric;
    boost::algorithm::split(cmdlineMetric, cmdline, boost::is_any_of(" "), boost::token_compress_on);
    for (auto const& metric : cmdlineMetric) {
        args.push_back(metric);
    }
    return true;
}

std::string ProcessCollector::GetExecutablePath(pid_t pid) {
    ProcessExecutePath processExecutePath;
    if (!SystemInterface::GetInstance()->GetExecutablePathCache(pid, processExecutePath)) {
        return "";
    }
    return processExecutePath.path;
}

// argus 的进程path是通过readlink /proc/pid/exe 获取的
// args 是通过 /proc/pid/cmdline 获取的
bool ProcessCollector::GetProcessInfo(pid_t pid, ProcessInfo& processInfo) {
    std::string user = "unknown";
    ProcessCredName processCredName;

    if (GetProcessCredName(pid, processCredName)) {
        user = processCredName.user;
    }

    std::vector<std::string> args;
    if (GetProcessArgs(pid, args)) {
        processInfo.args = args.front();
        for (size_t i = 1; i < args.size(); i++) {
            processInfo.args += " " + args[i];
        }
    }

    processInfo.path = GetExecutablePath(pid);
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
bool ProcessCollector::GetProcessFdNumber(pid_t pid, ProcessFd& processFd) {
    if (!SystemInterface::GetInstance()->GetProcessOpenFiles(pid, processFd)) {
        return false;
    }

    return true;
}

// 获取pid的内存信息
bool ProcessCollector::GetProcessMemory(pid_t pid, ProcessMemoryInformation& processMemory) {
    ProcessStat processStat;

    if (!ReadProcessStat(pid, processStat)) {
        return false;
    }
    processMemory.minorFaults = processStat.minorFaults;
    processMemory.majorFaults = processStat.majorFaults;
    processMemory.pageFaults = processStat.minorFaults + processStat.majorFaults;

    if (!SystemInterface::GetInstance()->GetPorcessStatm(pid, processMemory)) {
        return false;
    }

    return true;
}

// 获取pid的状态信息
bool ProcessCollector::GetProcessState(pid_t pid, ProcessStat& processState) {
    ProcessInformation processInfo;

    if (!ReadProcessStat(pid, processInfo.stat)) {
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

// 获取每个Pid的CPU信息
bool ProcessCollector::GetPidsCpu(const std::vector<pid_t>& pids, std::map<pid_t, uint64_t>& pidMap) {
    int readCount = 0;
    for (pid_t pid : pids) {
        if (++readCount > mProcessSilentCount) { // 每读一段时间就要停下，防止进程过多占用太多时间
            readCount = 0;
            std::this_thread::sleep_for(milliseconds{100});
        }
        // 获取每个Pid的CPU信息
        ProcessCpuInformation procCpu;
        if (0 == GetProcessCpuInformation(pid, procCpu, false)) {
            pidMap[pid] = procCpu.total;
        }
    }
    return true;
}


// 给pid做cache
bool ProcessCollector::GetProcessCpuInCache(pid_t pid, bool includeCTime) {
    if (cpuTimeCache.find(pid) != cpuTimeCache.end()) {
        return true;
    } else {
        return false;
    }
}


bool ProcessCollector::GetProcessCpuInformation(pid_t pid, ProcessCpuInformation& information, bool includeCTime) {
    const auto now = std::chrono::steady_clock::now();
    bool findCache = false;
    ProcessCpuInformation* prev = nullptr;

    // 由于计算CPU时间需要获取一个时间间隔
    // 但是我们这里不应该睡眠，因此只能做一个cache，保存上一次获取的数据
    findCache = GetProcessCpuInCache(pid, includeCTime);

    information.lastTime = now;
    ProcessTime processTime{};

    if (!GetProcessTime(pid, processTime, includeCTime)) {
        return false;
    }

    if (findCache) {
        // cache found, calculate the cpu percent
        auto recordedEntity = cpuTimeCache.find(pid);
        if (recordedEntity != cpuTimeCache.end()) {
            prev = &recordedEntity->second;
        }
    } else {
        information.lastTime = now;
        information.percent = 0.0;
        information.sys = processTime.sys.count();
        information.user = processTime.user.count();
        information.total = processTime.total.count();
        cpuTimeCache[pid] = information;
        return true;
    }

    int64_t timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev->lastTime).count();

    // update the cache
    using namespace std::chrono;
    information.startTime = processTime.startTime;
    information.lastTime = now;
    information.user = processTime.user.count();
    information.sys = processTime.sys.count();
    information.total = processTime.total.count();

    // calculate cpuPercent = (thisTotal - prevTotal)/HZ;
    auto totalCPUDiff = static_cast<double>(information.total - prev->total) / SYSTEM_HERTZ;
    information.percent = 100 * totalCPUDiff / (static_cast<double>(timeDiff) / SYSTEM_HERTZ); // 100%
    cpuTimeCache[pid] = information;
    return true;
}

bool ProcessCollector::GetProcessTime(pid_t pid, ProcessTime& output, bool includeCTime) {
    ProcessInformation processInfo;

    if (!ReadProcessStat(pid, processInfo.stat)) {
        return false;
    }

    output.startTime = processInfo.stat.startTicks;

    output.cutime = std::chrono::milliseconds(processInfo.stat.cutimeTicks);
    output.cstime = std::chrono::milliseconds(processInfo.stat.cstimeTicks);
    output.user = std::chrono::milliseconds(processInfo.stat.utimeTicks + processInfo.stat.cutimeTicks);
    output.sys = std::chrono::milliseconds(processInfo.stat.stimeTicks + processInfo.stat.cstimeTicks);

    output.total = std::chrono::milliseconds(output.user + output.sys);

    return true;
}

// 数据样例: /proc/1/stat, 解析/proc/pid/stat
// 1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 4194304 4238788
// 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 140727020027760 140727020027777
// 140727020027777 140727020027887 0
bool ProcessCollector::ReadProcessStat(pid_t pid, ProcessStat& processStat) {
    processStat.pid = pid;
    ProcessInformation processInfo{};
    if (!SystemInterface::GetInstance()->GetProcessInformation(pid, processInfo)) {
        return false;
    }

    processStat.name = processInfo.stat.name;

    processStat.state = processInfo.stat.state;
    processStat.parentPid = processInfo.stat.parentPid;
    processStat.priority = processInfo.stat.priority;
    processStat.nice = processInfo.stat.nice;
    processStat.numThreads = processInfo.stat.numThreads;
    processStat.tty = processInfo.stat.tty;
    processStat.minorFaults = processInfo.stat.minorFaults;
    processStat.majorFaults = processInfo.stat.majorFaults;

    processStat.utimeTicks = processInfo.stat.utimeTicks;
    processStat.stimeTicks = processInfo.stat.stimeTicks;
    processStat.cutimeTicks = processInfo.stat.cutimeTicks;
    processStat.cstimeTicks = processInfo.stat.cstimeTicks;

    // startTicks is int64_t type
    processStat.startTicks = processInfo.stat.startTicks;
    processStat.vSize = processInfo.stat.vSize;
    processStat.rss = processInfo.stat.rss;
    processStat.processor = processInfo.stat.processor;

    return true;
}

} // namespace logtail
