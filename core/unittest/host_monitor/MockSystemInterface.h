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
    static MockSystemInterface* GetInstance() {
        static MockSystemInterface instance;
        return &instance;
    }

private:
    bool GetSystemInformationOnce(SystemInformation& systemInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        systemInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetCPUInformationOnce(CPUInformation& cpuInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        cpuInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetProcessListInformationOnce(ProcessListInformation& processListInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        processListInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        processInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        systemLoadInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        cpuCoreNumInfo.collectTime = time(nullptr);
        return true;
    }
    bool GetTCPStatInformationOnce(TCPStatInformation& tcpStatInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        tcpStatInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetNetInterfaceInformationOnce(NetInterfaceInformation& netInterfaceInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        netInterfaceInfo.collectTime = time(nullptr);
        return true;
    }

    bool GetHostMemInformationStatOnce(MemoryInformation& meminfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        ++mMockCalledCount;
        meminfo.collectTime = time(nullptr);
        return true;
    }
    bool GetProcessCmdlineStringOnce(pid_t pid, ProcessCmdlineString& cmdline) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        cmdline.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetProcessStatmOnce(pid_t pid, ProcessMemoryInformation& processMemory) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        processMemory.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetProcessCredNameOnce(pid_t pid, ProcessCredName& processCredName) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        processCredName.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetExecutablePathOnce(pid_t pid, ProcessExecutePath& executePath) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        executePath.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetProcessOpenFilesOnce(pid_t pid, ProcessFd& processFd) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        processFd.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetFileSystemListInformationOnce(FileSystemListInformation& fileSystemListInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        fileSystemListInfo.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetSystemUptimeInformationOnce(SystemUptimeInformation& systemUptimeInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        systemUptimeInfo.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetDiskSerialIdInformationOnce(std::string diskName, SerialIdInformation& serialIdInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        serialIdInfo.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetDiskStateInformationOnce(DiskStateInformation& diskStateInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        diskStateInfo.collectTime = time(nullptr);
        ++mMockCalledCount;
        return true;
    }

    bool GetFileSystemInformationOnce(std::string dirName, FileSystemInformation& fileSystemInfo) override {
        if (mBlockTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mBlockTime));
        }
        fileSystemInfo.collectTime = time(nullptr);
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
