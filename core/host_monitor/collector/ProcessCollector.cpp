/*
 * Copyright 2024 iLogtail Authors
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

#include "ProcessCollector.h"

#include <sched.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "FileSystemUtil.h"
#include "Logger.h"
#include "PipelineEventGroup.h"
#include "StringTools.h"
#include "constants/EntityConstants.h"
#include "host_monitor/SystemInformationTools.h"

namespace logtail {

const size_t ProcessTopN = 20;

void ProcessCollector::Collect(PipelineEventGroup& group) {
    group.SetMetadata(EventGroupMetaKey::COLLECT_TIME, std::to_string(time(nullptr)));
    std::vector<ProcessStatPtr> processes;
    SortProcessByCpu(processes, ProcessTopN);
    for (auto process : processes) {
        auto event = group.AddLogEvent();
        time_t logtime = time(nullptr);
        event->SetTimestamp(logtime);

        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PID, std::to_string(process->pid));
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID, std::to_string(process->parentPid));
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME,
                          std::to_string(duration_cast<seconds>(process->startTime.time_since_epoch()).count()));
    }
}

void ProcessCollector::SortProcessByCpu(std::vector<ProcessStatPtr>& processStats, size_t topN) {
    steady_clock::time_point now = steady_clock::now();
    auto compare = [](const std::pair<ProcessStatPtr, uint64_t>& a, const std::pair<ProcessStatPtr, uint64_t>& b) {
        return a.second < b.second;
    };
    std::priority_queue<std::pair<ProcessStatPtr, uint64_t>,
                        std::vector<std::pair<ProcessStatPtr, uint64_t>>,
                        decltype(compare)>
        queue(compare);

    int readCount = 0;
    WalkAllProcess(PROCESS_DIR, [&](const std::string& dirName) {
        if (++readCount > mProcessSilentCount) {
            readCount = 0;
            std::this_thread::sleep_for(milliseconds{100});
        }
        auto pid = StringTo<pid_t>(dirName);
        if (pid != 0) {
            bool isFirstCollect = false;
            auto ptr = GetProcessStat(pid, isFirstCollect);
            if (ptr && !isFirstCollect) {
                queue.push(std::make_pair(ptr, ptr->cpuInfo.total));
            }
            if (queue.size() > topN) {
                queue.pop();
            }
        }
    });

    processStats.clear();
    processStats.reserve(queue.size());
    while (!queue.empty()) {
        processStats.push_back(queue.top().first);
        queue.pop();
    }
    std::reverse(processStats.begin(), processStats.end());

    if (processStats.empty()) {
        LOG_INFO(sLogger, ("first collect Process Cpu info", "empty"));
    }
    LOG_DEBUG(sLogger, ("collect Process Cpu info, top", processStats.size()));

    mProcessSortTime = now;
    mSortProcessStats = processStats;
}

ProcessStatPtr ProcessCollector::GetProcessStat(pid_t pid, bool& isFirstCollect) {
    const auto now = steady_clock::now();

    // TODO: more accurate cache
    auto prev = GetPreProcessStat(pid);
    isFirstCollect = prev == nullptr || prev->lastTime.time_since_epoch().count() == 0;
    // proc/[pid]/stat的统计粒度通常为10ms，两次采样之间需要足够大才能平滑。
    if (prev && now < prev->lastTime + seconds{1}) {
        return prev;
    }
    auto ptr = ReadProcessStat(pid);
    if (!ptr) {
        return nullptr;
    }

    // calculate CPU related fields
    {
        ptr->lastTime = now;
        ptr->cpuInfo.user = ptr->utime.count();
        ptr->cpuInfo.sys = ptr->stime.count();
        ptr->cpuInfo.total = ptr->cpuInfo.user + ptr->cpuInfo.sys;
        if (isFirstCollect || ptr->cpuInfo.total <= prev->cpuInfo.total) {
            // first time called
            ptr->cpuInfo.percent = 0.0;
        } else {
            auto totalDiff = static_cast<double>(ptr->cpuInfo.total - prev->cpuInfo.total);
            auto timeDiff = static_cast<double>(ptr->lastTime.time_since_epoch().count()
                                                - prev->lastTime.time_since_epoch().count());
            ptr->cpuInfo.percent = totalDiff / timeDiff * 100;
        }
    }

    prev = ptr;
    mPrevProcessStat[pid] = ptr;
    return ptr;
}

ProcessStatPtr ProcessCollector::ReadProcessStat(pid_t pid) {
    LOG_DEBUG(sLogger, ("read process stat", pid));
    auto processStat = PROCESS_DIR / std::to_string(pid) / PROCESS_STAT;

    std::string line;
    if (!ReadFileContent(processStat.string(), line)) {
        LOG_ERROR(sLogger, ("read process stat", "fail")("file", processStat));
        return nullptr;
    }
    return ParseProcessStat(pid, line);
}


// 数据样例: /proc/1/stat
// 1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 4194304 4238788
// 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 140727020027760 140727020027777
// 140727020027777 140727020027887 0
ProcessStatPtr ProcessCollector::ParseProcessStat(pid_t pid, std::string& line) {
    ProcessStatPtr ptr = std::make_shared<ProcessStat>();
    ptr->pid = pid;
    auto nameStartPos = line.find_first_of('(');
    auto nameEndPos = line.find_last_of(')');
    if (nameStartPos == std::string::npos || nameEndPos == std::string::npos) {
        LOG_ERROR(sLogger, ("can't find process name", pid)("stat", line));
        return nullptr;
    }
    nameStartPos++; // 跳过左括号
    ptr->name = line.substr(nameStartPos, nameEndPos - nameStartPos);
    line = line.substr(nameEndPos + 2); // 跳过右括号及空格

    std::vector<std::string> words = SplitString(line);

    constexpr const EnumProcessStat offset = EnumProcessStat::state; // 跳过pid, comm
    constexpr const int minCount = EnumProcessStat::processor - offset + 1; // 37
    if (words.size() < minCount) {
        LOG_ERROR(sLogger, ("unexpected item count", pid)("stat", line));
        return nullptr;
    }

    ptr->state = words[EnumProcessStat::state - offset].front();
    ptr->parentPid = StringTo<pid_t>(words[EnumProcessStat::ppid - offset]);
    ptr->tty = StringTo<int>(words[EnumProcessStat::tty_nr - offset]);
    ptr->minorFaults = StringTo<uint64_t>(words[EnumProcessStat::minflt - offset]);
    ptr->majorFaults = StringTo<uint64_t>(words[EnumProcessStat::majflt - offset]);

    ptr->utime = static_cast<milliseconds>(StringTo<uint64_t>(words[EnumProcessStat::utime - offset]));
    ptr->stime = static_cast<milliseconds>(StringTo<uint64_t>(words[EnumProcessStat::stime - offset]));
    ptr->cutime = static_cast<milliseconds>(StringTo<uint64_t>(words[EnumProcessStat::cutime - offset]));
    ptr->cstime = static_cast<milliseconds>(StringTo<uint64_t>(words[EnumProcessStat::cstime - offset]));

    ptr->priority = StringTo<int>(words[EnumProcessStat::priority - offset]);
    ptr->nice = StringTo<int>(words[EnumProcessStat::nice - offset]);
    ptr->numThreads = StringTo<int>(words[EnumProcessStat::num_threads - offset]);

    ptr->startTime = system_clock::time_point{
        static_cast<milliseconds>(StringTo<uint32_t>(words[EnumProcessStat::starttime - offset]))
        + milliseconds{GetSystemBootSeconds() * 1000}};
    ptr->vSize = StringTo<uint64_t>(words[EnumProcessStat::vsize - offset]);
    ptr->rss = StringTo<uint64_t>(words[EnumProcessStat::rss - offset]) << (getpagesize());
    ptr->processor = StringTo<int>(words[EnumProcessStat::processor - offset]);
    return ptr;
}

bool ProcessCollector::WalkAllProcess(const std::filesystem::path& root,
                                      const std::function<void(const std::string&)>& callback) {
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        LOG_ERROR(sLogger, ("ProcessCollector", "root path is not a directory or not exist")("root", root));
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

} // namespace logtail
