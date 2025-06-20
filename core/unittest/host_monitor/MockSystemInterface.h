// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>

#include <thread>

#include "host_monitor/SystemInterface.h"

namespace logtail {

struct MockInformation : public BaseInformation {
    int64_t id;
};

template class SystemInterface::template SystemInformationCache<MockInformation>;
template class SystemInterface::template SystemInformationCache<MockInformation, int>;

class MockSystemInterface : public SystemInterface {
public:
    MockSystemInterface() = default;
    ~MockSystemInterface() override = default;

private:
    bool GetSystemInformationOnce(SystemInformation& systemInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        systemInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    bool GetCPUInformationOnce(CPUInformation& cpuInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        cpuInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    bool GetProcessListInformationOnce(ProcessListInformation& processListInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        processListInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    bool GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        processInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    bool GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        systemLoadInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    bool GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        cpuCoreNumInfo.collectTime = std::chrono::steady_clock::now();
        ++mMockCalledCount;
        return true;
    }

    int64_t mBlockTime = 0;
    int64_t mMockCalledCount = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SystemInterfaceUnittest;
#endif
};

} // namespace logtail
