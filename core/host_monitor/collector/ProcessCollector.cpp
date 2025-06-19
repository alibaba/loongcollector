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

#include <filesystem>
#include <string>
#include <algorithm>
#include <thread>
#include <boost/program_options.hpp>
#include <pwd.h>
#include <grp.h>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"
#include "common/TimeUtil.h"

int64_t ToMillis(std::chrono::system_clock::time_point &t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
}

#define Diff(a, b)  a-b>0 ? a-b : 0;
#define PATH_MAX 4096

//topN进行的缓存为55s
const std::chrono::seconds ProcessSortInterval{55};

// TIndex为索引的Vector
template<typename TIndex>
class TVector {
    const std::vector<std::string> &data;
    const TIndex offset;
public:
    const std::string empty{};

    TVector(const std::vector<std::string> &d, TIndex of = (TIndex) 0)
            : data(d), offset(of) {
    }

    const std::string &operator[](TIndex key) const {
        int index = (int) key - (int) offset;
        return 0 <= index && index < static_cast<int>(data.size()) ? data[index] : empty;
    }
};

extern const int ClkTck = sysconf(_SC_CLK_TCK); // 一般为100

static uint64_t Tick2Millisecond(uint64_t tick) {
    constexpr const uint64_t MILLISECOND = 1000;
    return tick * MILLISECOND / ClkTck;
}

static uint64_t Tick2Millisecond(const std::string &tick) {
    return Tick2Millisecond(static_cast<uint64_t>(std::stoll(tick)));
}

// T: uint64_t、std::chrono::milliseconds
template<typename T>
T Tick2(const std::string &tick) {
    return T{Tick2Millisecond(tick)};
}

template<typename T, typename TIndex>
class MillisVector {
    const TVector<TIndex> &v;
public:
    static constexpr const T zero{0};

    explicit MillisVector(const TVector<TIndex> &r) : v(r) {
    }

    T operator[](TIndex key) const {
        return Tick2<T>(v[key]);
    }
};

namespace logtail {

const std::string ProcessCollector::sName = "process";
const std::string kMetricLabelProcess = "valueTag";
const std::string kMetricLabelMode = "mode";


ProcessCollector::ProcessCollector(){
    Init();
}

static uint64_t GetMemoryValue(char unit, uint64_t value) {
    if (unit == 'k' || unit == 'K') {
        value *= 1024;
    } else if (unit == 'm' || unit == 'M') {
        value *= 1024 * 1024;
    }
    return value;
}

static double GetMemoryStat(std::vector<std::string>& memoryLines) {
    int ret = false;
    double totalMemory = 0.0;

    if (memoryLines.empty()) {
        return ret;
    }

    std::unordered_map<std::string, double &> memoryProc{
            {"MemTotal:",     totalMemory},
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

    return totalMemory;
}

int ProcessCollector::Init(int totalCount) {
    std::vector<std::string> memLines;
    std::string errorMessage;

    if (!GetHostSystemStatWithPath(memLines, errorMessage, "/proc/meminfo")) {
        LOG_WARNING(sLogger, ("failed to get system load", "invalid System collector")("error msg", errorMessage));
        mTotalMemory = 0;
        return -1;
    }

    double totalMemory = GetMemoryStat(memLines);
    mTotalMemory = totalMemory;
    mTotalCount = totalCount;

    return 0;
}

bool ProcessCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }
    if (!GetPids(pids)) {
        return false;
    }

    time_t now = time(nullptr);

    // 获取每一个进程的信息
    std::vector<ProcessAllStat> allPidStats;
    for (auto pid : pids) {
        ProcessAllStat stat;
        if (GetProcessAllStat(pid, stat) != EXECUTE_SUCCESS) {
            continue;
        }
        allPidStats.push_back(stat);
    }

    int processNum = allPidStats.size();

    VMProcessNumStat processNumStat;
    processNumStat.vmProcessNum = processNum;

    std::vector<ProcessPushMertic> pushMerticList;
    for (auto &stat : allPidStats) {
        // 生成每个pid统计信息的推送对象
        ProcessPushMertic pushMertic;
        pushMertic.pid = stat.pid;
        pushMertic.allNumProcess = processNum;
        pushMertic.fdNum  = stat.fdNum;
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
    for (auto &metric : pushMerticList) {
        uint64_t thisPid = metric.pid;
        // auto met = mProcessPushMertic.find(thisPid);
        if ( mProcessPushMertic.find(thisPid) != mProcessPushMertic.end()) {
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
    if (mCount < mTotalCount) {
        return true;
    }

    // 记录count满足条件以后，计算并推送多值指标；如果没有到达条件，只需要往多值体系内添加统计对象即可
    VMProcessNumStat minVMProcessNum, maxVMProcessNum, avgVMProcessNum, lastVMProcessNum;
    mVMProcessNumStat.Stat(minVMProcessNum, maxVMProcessNum, avgVMProcessNum, &lastVMProcessNum);

    for (auto &metric : pushMerticList) {
        //处理每一个pid的推送数据
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
        "vm_process_minimum",
        "vm_process_maximum",
        "vm_process_average",
    };
    std::vector<double> vmValues = {
        minVMProcessNum.vmProcessNum,
        maxVMProcessNum.vmProcessNum,
        avgVMProcessNum.vmProcessNum,
    };
    // vm的系统信息上传
    for (size_t  i = 0; i < vmNames.size(); ++i) {
        multiDoubleValues->SetValue(std::string(vmNames[i]),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, vmValues[i]});
    }

    // 每个pid一条记录上报
    for (size_t i = 0; i < pushMerticList.size(); ++i) {
        MetricEvent* metricEventEachPid = group->AddMetricEvent(true);
        metricEventEachPid->SetTimestamp(now, 0);
        metricEventEachPid->SetValue<UntypedMultiDoubleValues>(metricEventEachPid);
        auto* multiDoubleValuesEachPid = metricEventEachPid->MutableValue<UntypedMultiDoubleValues>();
        // 上传每一个pid对应的值
        double value = 0.0;
        pid_t pid = pushMerticList[i].pid;
        // cpu percent
        value = static_cast<double>(mAvgProcessCpuPercent.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_cpu_average"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // mem percent
        value = static_cast<double>(mAvgProcessMemPercent.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_memory_average"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // open file number
        value = static_cast<double>(mAvgProcessFd.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_openfile_average"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // process number
        value = static_cast<double>(mAvgProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_average"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(mMaxProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_maximum"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        value = static_cast<double>(mMinProcessNumThreads.find(pid)->second);
        multiDoubleValuesEachPid->SetValue(std::string("process_number_minimum"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        metricEventEachPid->SetTag("pid", std::to_string(pid));
        metricEventEachPid->SetTag("name", pushMerticList[i].name);
        metricEventEachPid->SetTag("user", pushMerticList[i].user);
    }

    // 打包记录，process_expand
    // average record
    for (size_t i = 0; i < pushMerticList.size(); ++i) {
        MetricEvent* metricEventEachPidExpand = group->AddMetricEvent(true);
        metricEventEachPidExpand->SetTimestamp(now, 0);
        metricEventEachPidExpand->SetValue<UntypedMultiDoubleValues>(metricEventEachPidExpand);
        auto* multiDoubleValuesEachPidExpand = metricEventEachPidExpand->MutableValue<UntypedMultiDoubleValues>();
        double value = 0.0;
        pid_t pid = pushMerticList[i].pid;
        // cpu
        value = static_cast<double>(pushMerticList[i].cpuPercent);
        multiDoubleValuesEachPidExpand->SetValue(std::string("process_expand_cpu_percent"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // mem
        value = static_cast<double>(pushMerticList[i].memPercent);
        multiDoubleValuesEachPidExpand->SetValue(std::string("process_expand_memory_percent"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});
        // openfile
        value = static_cast<double>(pushMerticList[i].fdNum);
        multiDoubleValuesEachPidExpand->SetValue(std::string("process_expand_openfile_number"),
                                    UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, value});

        metricEventEachPidExpand->SetTag("pid", std::to_string(pid));
        metricEventEachPidExpand->SetTag("name", pushMerticList[i].name);
        metricEventEachPidExpand->SetTag("user", pushMerticList[i].user);
        metricEventEachPidExpand->SetTag("args", pushMerticList[i].args);
        metricEventEachPidExpand->SetTag("path", pushMerticList[i].path);
    }

    //清空所有多值体系，因为有的pid后面可能会消失
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

//////////////////////////////////////////////
bool ProcessCollector::GetPids(std::vector<pid_t>& pids) {
    pids.clear();
    bool ret;
    ret = WalkAllDigitDirs(PROCESS_DIR, [&](const std::string &dirName) {
            pid_t pid{};
            if (!StringTo(dirName, pid)) {
                return;
            }
            if (pid != 0) {
                pids.push_back(pid);
            }
        });

    return ret;
}

bool ProcessCollector::WalkAllDigitDirs(const std::filesystem::path& root, const std::function<void(const std::string&)>& callback) {
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        return false;
    }

    for (const auto& dirEntry :
         std::filesystem::directory_iterator{root, std::filesystem::directory_options::skip_permission_denied}) {
        std::string filename = dirEntry.path().filename().string();
        if (IsInt(filename)) {
            callback(filename);
        }
    }
    return true;
}

int ProcessCollector::CountNumsDir(const std::filesystem::path& root, ProcessFd& procFd) {
    int count = 0;
    for (const auto& dirEntry :
         std::filesystem::directory_iterator{root, std::filesystem::directory_options::skip_permission_denied}) {
        std::string filename = dirEntry.path().filename().string();
        count++;
    }
    procFd.total = count;
    return EXECUTE_SUCCESS;
}

// pid - self
// pid - parent id
void ProcessCollector::RemovePid(std::vector<pid_t> &pids, pid_t pid, pid_t ppid) {
    size_t count = pids.size();
    for (size_t i = 0; i < count;) {
        pid_t curPid = pids[i];
        if (curPid == pid || curPid == ppid) {
            count--;
            pids[i] = pids[count];
        } else {
            ++i;
        }
    }
    if (count != pids.size()) {
        pids.resize(count);
    }
}

void ProcessCollector::GetSelfPid(pid_t &pid, pid_t &ppid) {
    pid = getpid();
    ppid = getppid();
}

// 获取某个pid的信息
int ProcessCollector::GetProcessAllStat(pid_t pid, ProcessAllStat &processStat) {
    // 获取这个pid的cpu信息
    processStat.pid = pid;
    int ret = GetProcessCpuInformation(pid, processStat.processCpu, false);
    if (ret != 0) {
        return ret;
    }

    ret = GetProcessState(pid, processStat.processState);
    if (ret != 0) {
        return ret;
    }

    ret = GetProcessMemory(pid, processStat.processMemory);
    if (ret != 0) {
        return ret;
    }

    ProcessFd procFd;
    ret = GetProcessFdNumber(pid, procFd);
    if (ret != EXECUTE_SUCCESS) {
        return ret;
    }
    processStat.fdNum = procFd.total;
    processStat.fdNumExact = procFd.exact;

    ret = GetProcessInfo(pid, processStat.processInfo);
    if (ret != EXECUTE_SUCCESS) {
        return ret;
    }
    
    processStat.memPercent = mTotalMemory == 0 ? 0 : 100.0 * processStat.processMemory.resident / mTotalMemory;
    return EXECUTE_SUCCESS;
}

int ProcessCollector::GetProcessCredName(pid_t pid,ProcessCredName &processCredName) {
    std::vector<std::string> processStatusLines = {};
    std::string errorMessage;
    std::filesystem::path path = PROCESS_DIR / std::to_string(pid) / PROCESS_STATUS;
    if (!GetHostSystemStatWithPath(processStatusLines, errorMessage, path)
        || processStatusLines.size() < 2) {
        return EXECUTE_FAIL;
    }

    ProcessCred cred{};
    for (size_t i = 0; i < processStatusLines.size(); ++i) {
        auto metric = split(processStatusLines[i], '\t', false);
        if (metric.front() == "Name:") {
            processCredName.name = metric[1];
        }
        if (metric.size() >= 3 && metric.front() == "Uid:") {
            int index = 1;
            cred.uid = static_cast<uint64_t>(std::stoull(metric[index++]));
            cred.euid = static_cast<uint64_t>(std::stoull(metric[index]));
        } else if (metric.size() >= 3 && metric.front() == "Gid:") {
            int index = 1;
            cred.gid = static_cast<uint64_t>(std::stoull(metric[index++]));
            cred.egid = static_cast<uint64_t>(std::stoull(metric[index]));
        }
    }

    passwd *pw = nullptr;
    passwd pwbuffer;
    char buffer[2048];
    if (getpwuid_r(cred.uid, &pwbuffer, buffer, sizeof(buffer), &pw) != 0) {
        return EXECUTE_FAIL;
    }
    if (pw == nullptr) {
        return EXECUTE_FAIL;
    }
    processCredName.user = pw->pw_name;

    group *grp = nullptr;
    group grpbuffer{};
    char groupBuffer[2048];
    if (getgrgid_r(cred.gid, &grpbuffer, groupBuffer, sizeof(groupBuffer), &grp)) {
        return EXECUTE_FAIL;
    }

    if (grp != nullptr && grp->gr_name != nullptr) {
        processCredName.group = grp->gr_name;
    }

    return EXECUTE_SUCCESS;
}

int ProcessCollector::GetProcessArgs(pid_t pid, std::vector<std::string> &args) {
    std::vector<std::string> args_content;
    std::string cmdline;
    std::string errorMessage;
    std::filesystem::path procArgsPath = PROCESS_DIR / std::to_string(pid) / PROCESS_CMDLINE;
    GetHostSystemStatWithPath(args_content, errorMessage, procArgsPath);
    if (args_content.empty()) {
        // /proc/pid/cmdline have no content
        return EXECUTE_FAIL;
    }
    cmdline = args_content.front();
    if (cmdline.empty()) {
        return errorMessage.empty() ? EXECUTE_FAIL : EXECUTE_SUCCESS;
    }
    auto cmdlineMetric = split(cmdline, ' ', {false, true});
    for (auto const &metric: cmdlineMetric) {
        args.push_back(metric);
    }
    return EXECUTE_SUCCESS;
}

std::string ProcessCollector::GetExecutablePath(pid_t pid) { 
    std::filesystem::path procExePath = PROCESS_DIR / std::to_string(pid) / PROCESS_EXE;
    char buffer[PATH_MAX];
    ssize_t len = readlink(procExePath.c_str(), buffer, sizeof(buffer));
    if (len < 0) {
        return {};
    }
    return {buffer, buffer + len};
}

// argus 的进程path是通过readlink /proc/pid/exe 获取的
// args 是通过 /proc/pid/cmdline 获取的
int ProcessCollector::GetProcessInfo(pid_t pid, ProcessInfo &processInfo) {
    std::string user = "unknown";
    ProcessCredName processCredName;

    if (GetProcessCredName(pid, processCredName) == EXECUTE_SUCCESS) {
        user = processCredName.user;
    }

    std::vector<std::string> args;
    if (GetProcessArgs(pid, args) == EXECUTE_SUCCESS) {
        processInfo.args = args.front();
        for (size_t i = 1; i < args.size(); ++i) {
            processInfo.args += " " + args[i];
        }
    }

    processInfo.path = GetExecutablePath(pid);

    processInfo.pid = pid;
    processInfo.name = processCredName.name;
    processInfo.user = user;

    return EXECUTE_SUCCESS;

}

//获取进程文件数信息
int ProcessCollector::GetProcessFdNumber(pid_t pid, ProcessFd &processFd) {
    std::filesystem::path procFdPath = PROCESS_DIR / std::to_string(pid) / PROCESS_FD;
    int ret = EXECUTE_SUCCESS;

    if (CountNumsDir(procFdPath, processFd) != EXECUTE_SUCCESS) {
        processFd.total = 0;
    }
    processFd.exact = true;

    return ret;
}

// 获取pid的内存信息
int ProcessCollector::GetProcessMemory(pid_t pid, ProcessMemoryInformation &processMemory) {
    std::string errorMessage;
    LinuxProcessInfo linuxProcessInfo;
    char* endptr;
    int status = ReadProcessStat(pid, linuxProcessInfo);
    if (status != 0) {
        return status;
    }
    processMemory.minorFaults = linuxProcessInfo.minorFaults;
    processMemory.majorFaults = linuxProcessInfo.majorFaults;
    processMemory.pageFaults = linuxProcessInfo.minorFaults + linuxProcessInfo.majorFaults;

    const auto procStatm = PROCESS_DIR / std::to_string(pid) / PROCESS_STATM;

    std::vector<std::string> loadLines;

    if (!GetHostSystemStatWithPath(loadLines, errorMessage, procStatm)) {
        return EXECUTE_FAIL;
    }
    std::vector<std::string> processMemoryMetric = split((loadLines.empty() ? "" : loadLines.front()), ' ', false);
    if (processMemoryMetric.size() < 3) {
        return EXECUTE_FAIL;
    }

    long pagesize = sysconf(_SC_PAGESIZE); // 获取系统页大小
    int index = 0;
    processMemory.size = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10));
    processMemory.size = processMemory.size * pagesize;
    processMemory.resident = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10)); 
    processMemory.resident = processMemory.resident * pagesize;
    processMemory.share = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10));
    processMemory.share = processMemory.share * pagesize;

    return EXECUTE_SUCCESS;
}

//获取pid的状态信息
int ProcessCollector::GetProcessState(pid_t pid, ProcessStat &processState) {
    LinuxProcessInfo linuxProcessInfo;
    int status = ReadProcessStat(pid, linuxProcessInfo);
    if (status != 0) {
        return status;
    }

    processState.state = linuxProcessInfo.state;
    processState.tty = linuxProcessInfo.tty;
    processState.parentPid = linuxProcessInfo.parentPid;
    processState.priority = linuxProcessInfo.priority;
    processState.nice = linuxProcessInfo.nice;
    processState.processor = linuxProcessInfo.processor;
    processState.numThreads = linuxProcessInfo.numThreads;

    return EXECUTE_SUCCESS;
}

// 获取每个Pid的CPU信息
int ProcessCollector::GetPidsCpu(const std::vector<pid_t> &pids, std::map<pid_t, uint64_t> &pidMap) {
    int readCount = 0;
    for (pid_t pid: pids) {
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
    return 0;
}


// 给pid做cache
bool ProcessCollector::GetProcessCpuInCache(pid_t pid, bool includeCTime) {
    if (cpuTimeCache.find(pid)!= cpuTimeCache.end()) {
        return true;
    } else {
        return false;
    }
}

double ProcessCollector::GetSysHz() {
    static double hz = 0.0;
    if (hz == 0.0) {
        hz = static_cast<double>(sysconf(_SC_CLK_TCK));
    }
    if (hz == -1) {
        // set default hz
        hz = 1000.0;
    }
    return hz;
}

int ProcessCollector::GetProcessCpuInformation(pid_t pid, ProcessCpuInformation &information,bool includeCTime) {
    const auto now = std::chrono::steady_clock::now();
    bool findCache = false;
    ProcessCpuInformation* prev = nullptr;

    // 由于计算CPU时间需要获取一个时间间隔
    // 但是我们这里不应该睡眠，因此只能做一个cache，保存上一次获取的数据
    findCache = GetProcessCpuInCache(pid, includeCTime);

    information.lastTime = now;
    ProcessTime processTime{};
    int res = GetProcessTime(pid, processTime, includeCTime);

    if (res != EXECUTE_SUCCESS) {
        return EXECUTE_FAIL;
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
         return 0;
    }

    int64_t timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev->lastTime).count(); 

    // update the cache
    using namespace std::chrono;
    information.startTime = ToMillis(processTime.startTime);
    information.lastTime = now;
    information.user = processTime.user.count();
    information.sys = processTime.sys.count();
    information.total = processTime.total.count();

    // calculate cpuPercent = (thisTotal - prevTotal)/HZ;
    auto totalCPUDiff = static_cast<double>(information.total - prev->total) / GetSysHz();
    information.percent = 100 * totalCPUDiff / (static_cast<double>(timeDiff)/ GetSysHz()); //100%
    cpuTimeCache[pid] = information;

    return EXECUTE_SUCCESS;
}

int ProcessCollector::GetProcessTime(pid_t pid, ProcessTime &output, bool includeCTime) {
    LinuxProcessInfo processInfo{};
    int stat = ReadProcessStat(pid, processInfo);
    if (stat != EXECUTE_SUCCESS) {
        return stat;
    }

    output.startTime = processInfo.startTime;

    output.cutime = processInfo.cutime;
    output.cstime = processInfo.cstime;
    output.user = processInfo.utime + output.cutime;
    output.sys = processInfo.stime + output.cstime;

    output.total = output.user + output.sys;

    return EXECUTE_SUCCESS;
}

// 数据样例: /proc/1/stat, 解析/proc/pid/stat
// 1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 140727020027760 140727020027777 140727020027777 140727020027887 0
int ProcessCollector::ReadProcessStat(pid_t pid, LinuxProcessInfo &processInfo) {
    processInfo.pid = pid;

    std::vector<std::string> loadLines;
    std::string first_line;
    std::string errorMessage;
    std::filesystem::path statPath = PROCESS_DIR / std::to_string(pid) / PROCESS_STAT;

    if (!GetHostSystemStatWithPath(loadLines, errorMessage, statPath)) {
        if (mValidState) {
            LOG_WARNING(sLogger, ("failed to get system load", "invalid System collector")("error msg", errorMessage));
            mValidState = false;
        }
        return EXECUTE_FAIL;
    }

    first_line = loadLines.front();
    auto nameStartPos = first_line.find_first_of('(');
    auto nameEndPos = first_line.find_last_of(')');

    if (nameStartPos == std::string::npos || nameEndPos == std::string::npos) {
        return EXECUTE_FAIL;
    }

    nameStartPos++; // 跳过左括号
    processInfo.name = first_line.substr(nameStartPos, nameEndPos - nameStartPos);
    first_line = first_line.substr(nameEndPos + 2); // 跳过右括号及空格

    std::vector<std::string> words = split(first_line, ' ', false);

    const EnumProcessStat offset = EnumProcessStat::state;  // 跳过pid, comm
    const int minCount = EnumProcessStat::processor - offset + 1;  // 37

    if (words.size() < minCount) {
        return EXECUTE_FAIL;
    }

    TVector<EnumProcessStat> v{words, offset};

    processInfo.state = v[EnumProcessStat::state].front();
    processInfo.parentPid = static_cast<pid_t>(atoi(v[EnumProcessStat::ppid].c_str()));
    processInfo.priority = static_cast<int>(atoi(v[EnumProcessStat::priority].c_str()));
    processInfo.nice = static_cast<int>(atoi(v[EnumProcessStat::nice].c_str()));
    processInfo.numThreads = static_cast<int>(atoi(v[EnumProcessStat::num_threads].c_str()));
    processInfo.tty = static_cast<int>(atoi(v[EnumProcessStat::tty_nr].c_str()));
    processInfo.minorFaults = static_cast<uint64_t>(atoi(v[EnumProcessStat::minflt].c_str()));
    processInfo.majorFaults = static_cast<uint64_t>(atoi(v[EnumProcessStat::majflt].c_str()));

    MillisVector<milliseconds, EnumProcessStat> mv{v};
    processInfo.utime = mv[EnumProcessStat::utime];
    processInfo.stime = mv[EnumProcessStat::stime];
    processInfo.cutime = mv[EnumProcessStat::cutime];
    processInfo.cstime = mv[EnumProcessStat::cstime];

    processInfo.startTime = std::chrono::system_clock::time_point{mv[EnumProcessStat::starttime]}; // for testing
    processInfo.vSize = static_cast<uint64_t>(atoi(v[EnumProcessStat::vsize].c_str()));
    processInfo.rss = static_cast<uint64_t>(atoi(v[EnumProcessStat::rss].c_str()));
    processInfo.processor = static_cast<int>(atoi(v[EnumProcessStat::processor].c_str()));

    return EXECUTE_SUCCESS;
}


} // namespace logtail
