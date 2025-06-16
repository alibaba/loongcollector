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

#pragma once

#include "common/ProcParser.h"
#include "host_monitor/SystemInterface.h"

namespace logtail {
class LinuxSystemInterface : public SystemInterface {
public:
    LinuxSystemInterface(const LinuxSystemInterface&) = delete;
    LinuxSystemInterface(LinuxSystemInterface&&) = delete;
    LinuxSystemInterface& operator=(const LinuxSystemInterface&) = delete;
    LinuxSystemInterface& operator=(LinuxSystemInterface&&) = delete;
    static LinuxSystemInterface* GetInstance() {
        static LinuxSystemInterface instance;
        return &instance;
    }

private:
    explicit LinuxSystemInterface() : mProcParser("") {}
    ~LinuxSystemInterface() = default;

    bool GetSystemInformationOnce(SystemInformation& systemInfo) override;
    bool GetCPUInformationOnce(CPUInformation& cpuInfo) override;
    bool GetProcessListInformationOnce(ProcessListInformation& processListInfo) override;
    bool GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) override;

    ProcParser mProcParser;
};
} // namespace logtail
