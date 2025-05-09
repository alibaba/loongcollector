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

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "common/FileSystemUtil.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "logger/Logger.h"

using namespace std;
using namespace std::chrono;

namespace logtail {

bool GetHostSystemStatWithPath(vector<string>& lines, string& errorMessage, filesystem::path PROC_DIR) {
    errorMessage.clear();
    if (!CheckExistance(PROC_DIR)) {
        errorMessage = "file does not exist: " + PROC_DIR.string();
        return false;
    }

    int ret = GetFileLines(PROC_DIR, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}

} // namespace logtail
