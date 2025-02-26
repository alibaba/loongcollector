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

#include "host_monitor/SystemInformationTools.h"

#include <string>
#include <vector>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "logger/Logger.h"

using namespace std;
using namespace std::chrono;

namespace logtail {

int64_t GetHostSystemBootTime() {
    static int64_t systemBootSeconds = 0;
    if (systemBootSeconds != 0) {
        return systemBootSeconds;
    }
    int64_t currentSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    if (!CheckExistance(PROCESS_DIR / PROCESS_STAT)) {
        LOG_WARNING(sLogger,
                    ("failed to get system boot time", "use process start time instead")(
                        "error msg", "file not exists")("process start time", currentSeconds));
        return currentSeconds;
    }

    vector<string> cpuLines = {};
    string errorMessage;
    int ret = GetFileLines(PROCESS_DIR / PROCESS_STAT, cpuLines, true, &errorMessage);
    if (ret != 0 || cpuLines.empty()) {
        LOG_WARNING(sLogger,
                    ("failed to get system boot time", "use process start time instead")("error msg", errorMessage)(
                        "process start time", currentSeconds));
        return currentSeconds;
    }

    for (auto const& cpuLine : cpuLines) {
        auto cpuMetric = SplitString(cpuLine);
        // example: btime 1719922762
        if (cpuMetric.size() >= 2 && cpuMetric[0] == "btime") {
            systemBootSeconds = StringTo<int64_t>(cpuMetric[1]);
            return systemBootSeconds;
        }
    }

    LOG_WARNING(sLogger,
                ("failed to get system boot time", "use process start time instead")(
                    "error msg", "btime not found in stat")("process start time", currentSeconds));
    return currentSeconds;
}

} // namespace logtail
