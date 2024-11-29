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

#include "SystemInformationTools.h"

#include "Logger.h"
#include "host_monitor/Constants.h"

namespace logtail {

int64_t GetSystemBootSeconds() {
    static int64_t systemBootSeconds;
    if (systemBootSeconds != 0) {
        return systemBootSeconds;
    }

    std::vector<std::string> cpuLines = {};
    std::string errorMessage;
    int ret = GetFileLines(PROCESS_DIR / PROCESS_STAT, cpuLines, true, &errorMessage);
    LOG_WARNING(sLogger, ("failed to get cpu lines", errorMessage)("ret", ret)("cpuLines", cpuLines.size()));
    if (ret != 0 || cpuLines.empty()) {
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    for (auto const& cpuLine : cpuLines) {
        auto cpuMetric = SplitString(cpuLine);
        if (cpuMetric.size() >= 2 && cpuMetric[0] == "btime") {
            constexpr size_t bootTimeIndex = 1;
            return bootTimeIndex < cpuMetric.size() ? StringTo<int64_t>(cpuMetric[bootTimeIndex]) : 0;
        }
    }

    systemBootSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    return systemBootSeconds;
}

} // namespace logtail
