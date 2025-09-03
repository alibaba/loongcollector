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

#pragma once

#include <cstdint>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "HostMonitorTimerEvent.h"
#include "common/ProcParser.h"
#include "common/StringView.h"
#include "constants/EntityConstants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/collector/BaseCollector.h"

using namespace std::chrono;

namespace logtail {


struct ProcessCpuInfo {
    uint64_t user = 0;
    uint64_t sys = 0;
    uint64_t total = 0;
    double percent = 0.0;
};

struct ExtendedProcessStat {
    ProcessStat stat;
    ProcessCpuInfo cpuInfo;
    steady_clock::time_point lastStatTime;
};

using ExtendedProcessStatPtr = std::shared_ptr<ExtendedProcessStat>;


class ProcessEntityCollector : public BaseCollector {
public:
    ProcessEntityCollector();
    ~ProcessEntityCollector() override = default;

    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override { return std::chrono::seconds(0); }

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    system_clock::time_point TicksToUnixTime(int64_t startTicks);
    time_t
    GetSortedProcess(std::vector<ExtendedProcessStatPtr>& processStats, size_t topN, const CollectTime& collectTime);
    ExtendedProcessStatPtr GetProcessStat(pid_t pid, bool& isFirstCollect, const CollectTime& collectTime);

    std::string GetProcessEntityID(StringView pid, StringView createTime, StringView hostEntityID);
    void FetchDomainInfo(std::string& domain,
                         std::string& entityType,
                         std::string& hostEntityType,
                         StringView& hostEntityID);

    std::chrono::steady_clock::time_point mProcessSortTime;
    std::unordered_map<pid_t, ExtendedProcessStatPtr> mPrevProcessStat;
    ProcParser mProcParser;

    const int mProcessSilentCount;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessEntityCollectorUnittest;
#endif
};

} // namespace logtail
