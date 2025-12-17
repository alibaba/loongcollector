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

#include <filesystem>
#include <string>

#include "common/StringView.h"

namespace logtail {

extern std::filesystem::path PROCESS_DIR;
extern std::filesystem::path SYSTEM_BLOCK_DIR;
extern std::filesystem::path ETC_DIR;
const extern std::filesystem::path PROCESS_STAT;
const extern std::filesystem::path PROCESS_LOADAVG;
const extern std::filesystem::path PROCESS_MEMINFO;
const extern std::filesystem::path ETC_MTAB;
const extern std::filesystem::path PROCESS_UPTIME;
const extern std::filesystem::path SERIAL;
const extern std::filesystem::path PROCESS_DISKSTATS;
const extern int64_t SYSTEM_HERTZ;

const extern std::filesystem::path PROCESS_NET_SOCKSTAT;
const extern std::filesystem::path PROCESS_NET_SOCKSTAT6;
const extern std::filesystem::path PROCESS_NET_DEV;
const extern std::filesystem::path PROCESS_NET_TCP;
const extern std::filesystem::path PROCESS_NET_TCP6;
const extern std::filesystem::path PROCESS_STATM;
const extern std::filesystem::path PROCESS_FD;
const extern std::filesystem::path PROCESS_CMDLINE;
const extern std::filesystem::path PROCESS_EXE;
const extern std::filesystem::path PROCESS_ROOT;
const extern std::filesystem::path PROCESS_STATUS;
const extern std::filesystem::path PROCESS_CWD;
const extern std::filesystem::path PROCESS_UPTIME;
const extern int EXECUTE_SUCCESS;
const extern int EXECUTE_FAIL;
const extern std::filesystem::path PROCESS_NET_IF_INET6;
const extern int64_t SYSTEM_HERTZ;
const extern long PAGE_SIZE;
#ifdef __ENTERPRISE__
inline constexpr StringView DEFAULT_INSTANCE_ID_LABEL = "instance_id";
inline constexpr StringView DEFAULT_USER_ID_LABEL = "user_id";
#else
inline constexpr StringView DEFAULT_HOST_IP_LABEL = "host_ip";
#endif
inline constexpr StringView DEFAULT_HOST_NAME_LABEL = "host_name";
const extern std::string NVSMI;
const extern std::string LIB_DCGM;
const extern std::filesystem::path NVIDIACTL;

} // namespace logtail
