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

#include "host_monitor/Constants.h"

#include <unistd.h>

namespace logtail {

std::filesystem::path PROCESS_DIR = "/proc";
const std::filesystem::path PROCESS_STAT = "stat";
const std::filesystem::path PROCESS_LOADAVG = "loadavg";
std::filesystem::path PROCESS_MEMINFO = "meminfo";
std::filesystem::path PROCESS_MTRR = "mtrr";
const std::filesystem::path PROCESS_NET_SOCKSTAT = "net/sockstat";
const std::filesystem::path PROCESS_NET_SOCKSTAT6 = "net/sockstat6";
const std::filesystem::path PROCESS_NET_DEV = "net/dev";
const std::filesystem::path PROCESS_STATM = "statm";
const std::filesystem::path PROCESS_FD = "fd";
const std::filesystem::path PROCESS_CMDLINE = "cmdline";
const std::filesystem::path PROCESS_CWD = "cwd";
const std::filesystem::path PROCESS_EXE = "exe";
const std::filesystem::path PROCESS_ROOT = "root";
const std::filesystem::path PROCESS_STATUS = "status";
const std::filesystem::path PROCESS_UPTIME = "uptime";
const int EXECUTE_SUCCESS = 0;
const int EXECUTE_FAIL = -1;
const int64_t SYSTEM_HERTZ = sysconf(_SC_CLK_TCK);

} // namespace logtail
