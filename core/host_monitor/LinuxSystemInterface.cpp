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

#include "host_monitor/LinuxSystemInterface.h"

#include <chrono>

using namespace std;
using namespace std::chrono;

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "logger/Logger.h"

namespace logtail {

bool GetHostSystemStat(vector<string>& lines, string& errorMessage) {
    errorMessage.clear();
    if (!CheckExistance(PROCESS_DIR / PROCESS_STAT)) {
        errorMessage = "file does not exist: " + (PROCESS_DIR / PROCESS_STAT).string();
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_STAT, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}

double ParseMetric(const std::vector<std::string>& cpuMetric, EnumCpuKey key) {
    if (cpuMetric.size() <= static_cast<size_t>(key)) {
        return 0.0;
    }
    double value = 0.0;
    if (!StringTo(cpuMetric[static_cast<size_t>(key)], value)) {
        LOG_WARNING(
            sLogger,
            ("failed to parse cpu metric", static_cast<size_t>(key))("value", cpuMetric[static_cast<size_t>(key)]));
    }
    return value;
}

bool GetHostLoadavg(vector<string>& lines, string& errorMessage) {
    errorMessage.clear();
    if (!CheckExistance(PROCESS_DIR / PROCESS_LOADAVG)) {
        errorMessage = "file does not exist: " + (PROCESS_DIR / PROCESS_LOADAVG).string();
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_LOADAVG, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}

bool LinuxSystemInterface::GetSystemInformationOnce(SystemInformation& systemInfo) {
    std::vector<std::string> lines;
    std::string errorMessage;
    if (!GetHostSystemStat(lines, errorMessage)) {
        LOG_ERROR(sLogger, ("failed to get system information", errorMessage));
        return false;
    }
    for (auto const& line : lines) {
        auto cpuMetric = SplitString(line);
        // example: btime 1719922762
        if (cpuMetric.size() >= 2 && cpuMetric[0] == "btime") {
            if (!StringTo(cpuMetric[1], systemInfo.bootTime)) {
                LOG_WARNING(sLogger,
                            ("failed to get system boot time", "use current time instead")("error msg", cpuMetric[1]));
                return false;
            }
            break;
        }
    }
    systemInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetCPUInformationOnce(CPUInformation& cpuInfo) {
    std::vector<std::string> cpuLines;
    std::string errorMessage;
    if (!GetHostSystemStat(cpuLines, errorMessage)) {
        return false;
    }
    // cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0
    // cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0
    // ...
    cpuInfo.stats.clear();
    cpuInfo.stats.reserve(cpuLines.size());
    for (auto const& line : cpuLines) {
        std::vector<std::string> cpuMetric;
        boost::split(cpuMetric, line, boost::is_any_of(" "), boost::token_compress_on);
        if (cpuMetric.size() > 0 && cpuMetric[0].substr(0, 3) == "cpu") {
            CPUStat cpuStat{};
            if (cpuMetric[0] == "cpu") {
                cpuStat.index = -1;
            } else {
                if (!StringTo(cpuMetric[0].substr(3), cpuStat.index)) {
                    LOG_ERROR(sLogger, ("failed to parse cpu index", "skip")("wrong cpu index", cpuMetric[0]));
                    continue;
                }
            }
            cpuStat.user = ParseMetric(cpuMetric, EnumCpuKey::user);
            cpuStat.nice = ParseMetric(cpuMetric, EnumCpuKey::nice);
            cpuStat.system = ParseMetric(cpuMetric, EnumCpuKey::system);
            cpuStat.idle = ParseMetric(cpuMetric, EnumCpuKey::idle);
            cpuStat.iowait = ParseMetric(cpuMetric, EnumCpuKey::iowait);
            cpuStat.irq = ParseMetric(cpuMetric, EnumCpuKey::irq);
            cpuStat.softirq = ParseMetric(cpuMetric, EnumCpuKey::softirq);
            cpuStat.steal = ParseMetric(cpuMetric, EnumCpuKey::steal);
            cpuStat.guest = ParseMetric(cpuMetric, EnumCpuKey::guest);
            cpuStat.guestNice = ParseMetric(cpuMetric, EnumCpuKey::guest_nice);
            cpuInfo.stats.push_back(cpuStat);
        }
    }
    cpuInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetProcessListInformationOnce(ProcessListInformation& processListInfo) {
    processListInfo.pids.clear();
    if (!std::filesystem::exists(PROCESS_DIR) || !std::filesystem::is_directory(PROCESS_DIR)) {
        LOG_ERROR(sLogger, ("process root path is not a directory or not exist", PROCESS_DIR));
        return false;
    }

    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(
             PROCESS_DIR, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::directory_iterator();
         ++it) {
        if (ec) {
            LOG_ERROR(sLogger, ("failed to iterate process directory", PROCESS_DIR)("error", ec.message()));
            return false;
        }
        const auto& dirEntry = *it;
        std::string dirName = dirEntry.path().filename().string();
        if (IsInt(dirName)) {
            pid_t pid{};
            if (!StringTo(dirName, pid)) {
                LOG_ERROR(sLogger, ("failed to parse pid", dirName));
            } else {
                processListInfo.pids.push_back(pid);
            }
        }
    }
    processListInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) {
    auto processStat = PROCESS_DIR / std::to_string(pid) / PROCESS_STAT;
    std::string line;
    if (FileReadResult::kOK != ReadFileContent(processStat.string(), line)) {
        LOG_ERROR(sLogger, ("read process stat", "fail")("file", processStat));
        return false;
    }
    mProcParser.ParseProcessStat(pid, line, processInfo.stat);
    processInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) {
    std::vector<std::string> loadLines;
    std::string errorMessage;
    if (!GetHostLoadavg(loadLines, errorMessage) || loadLines.empty()) {
        LOG_WARNING(sLogger, ("failed to get system load", "invalid System collector")("error msg", errorMessage));
        return false;
    }

    // cat /proc/loadavg
    // 0.10 0.07 0.03 1/561 78450
    std::vector<std::string> loadMetric;
    boost::split(loadMetric, loadLines[0], boost::is_any_of(" "), boost::token_compress_on);

    if (loadMetric.size() < 3) {
        LOG_WARNING(sLogger, ("failed to split load metric", "invalid System collector"));
        return false;
    }

    CpuCoreNumInformation cpuCoreNumInfo;
    if (!SystemInterface::GetInstance()->GetCPUCoreNumInformation(cpuCoreNumInfo)) {
        LOG_WARNING(sLogger, ("failed to get cpu core num", "invalid System collector"));
        return false;
    }
    systemLoadInfo.systemStat.load1 = std::stod(loadMetric[0]);
    systemLoadInfo.systemStat.load5 = std::stod(loadMetric[1]);
    systemLoadInfo.systemStat.load15 = std::stod(loadMetric[2]);

    systemLoadInfo.systemStat.load1PerCore
        = systemLoadInfo.systemStat.load1 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);
    systemLoadInfo.systemStat.load5PerCore
        = systemLoadInfo.systemStat.load5 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);
    systemLoadInfo.systemStat.load15PerCore
        = systemLoadInfo.systemStat.load15 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);

    return true;
}
bool LinuxSystemInterface::GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) {
    cpuCoreNumInfo.cpuCoreNum = std::thread::hardware_concurrency();
    cpuCoreNumInfo.cpuCoreNum = cpuCoreNumInfo.cpuCoreNum < 1 ? 1 : cpuCoreNumInfo.cpuCoreNum;
    return true;
}

static inline double Diff(double a, double b) {
    return a - b > 0 ? a - b : 0;
}

uint64_t LinuxSystemInterface::GetMemoryValue(char unit, uint64_t value) {
    if (unit == 'k' || unit == 'K') {
        value *= 1024;
    } else if (unit == 'm' || unit == 'M') {
        value *= 1024 * 1024;
    }
    return value;
}

/*
样例: /proc/meminfo:
MemTotal:        4026104 kB
MemFree:         2246280 kB
MemAvailable:    3081592 kB
Buffers:          124380 kB
Cached:          1216756 kB
SwapCached:            0 kB
Active:           417452 kB
Inactive:        1131312 kB
 */
bool LinuxSystemInterface::GetHostMemInformationStatOnce(MemoryInformation& meminfo) {
    auto memInfoStat = PROCESS_DIR / PROCESS_MEMINFO;
    std::vector<std::string> memInfoStr;
    const uint64_t mb = 1024 * 1024;

    std::ifstream file(static_cast<std::string>(memInfoStat));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open meminfo file", "fail")("file", memInfoStat));
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        memInfoStr.push_back(line);
    }

    file.close();

    int count = 0;

    /* 字符串处理，处理成对应的类型以及值*/
    for (size_t i = 0; i < memInfoStr.size() && count < 5; i++) {
        std::vector<std::string> words;
        boost::algorithm::split(words, memInfoStr[i], boost::is_any_of(" "), boost::token_compress_on);
        // words-> MemTotal: / 12344 / kB
        if (words.size() < 2) {
            continue;
        }
        double val;
        uint64_t orival;
        if (words.size() == 2) {
            if (!StringTo(words[1], val)) {
                val = 0.0;
            }
        } else if (words.back().size() > 0 && StringTo(words[1], orival)) {
            val = GetMemoryValue(words.back()[0], orival);
        }
        if (words[0] == "MemTotal:") {
            meminfo.memStat.total = val;
            count++;
        } else if (words[0] == "MemFree:") {
            meminfo.memStat.free = val;
            count++;
        } else if (words[0] == "MemAvailable:") {
            meminfo.memStat.available = val;
            count++;
        } else if (words[0] == "Buffers:") {
            meminfo.memStat.buffers = val;
            count++;
        } else if (words[0] == "Cached:") {
            meminfo.memStat.cached = val;
            count++;
        }
    }
    meminfo.memStat.used = Diff(meminfo.memStat.total, meminfo.memStat.free);
    meminfo.memStat.actualUsed = Diff(meminfo.memStat.total, meminfo.memStat.available);
    meminfo.memStat.actualFree = meminfo.memStat.available;
    meminfo.memStat.ram = meminfo.memStat.total / mb;

    double diff = Diff(meminfo.memStat.total, meminfo.memStat.actualFree);
    meminfo.memStat.usedPercent = meminfo.memStat.total > 0 ? diff * 100 / meminfo.memStat.total : 0.0;
    diff = Diff(meminfo.memStat.total, meminfo.memStat.actualUsed);
    meminfo.memStat.freePercent = meminfo.memStat.total > 0 ? diff * 100 / meminfo.memStat.total : 0.0;
    return true;
}


} // namespace logtail
