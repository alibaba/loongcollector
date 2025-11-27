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

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <system_error>

#include "host_monitor/Constants.h"
#include "host_monitor/LinuxSystemInterface.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {


class LinuxSystemInterfaceUnittest : public testing::Test {
public:
    void TestGetSystemInformationOnce() const;
    void TestGetCPUInformationOnce() const;
    void TestGetProcessListInformationOnce() const;
    void TestGetProcessInformationOnce() const;
    void TestGetProcessListInformationOncePathDeleting() const;
    void TestGetProcessOpenFilesOnce() const;
    void TestGetProcessOpenFilesOncePermissionDenied() const;
    void TestGetProcessOpenFilesOnceFilesystemError() const;

protected:
    void SetUp() override {
        bfs::create_directories("./1");
        ofstream ofs1("./stat", std::ios::trunc);
        ofs1 << "btime 1731142542\n";
        ofs1 << "cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0\n";
        ofs1 << "cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 1 2\n";
        ofs1 << "cpua a b c d e f 424744 0 1 2\n";
        ofs1 << "cpu1 14708487 14216 4613031 2108180843\n"; // test old linux kernel
        ofs1 << "cpu3 14708487 14216 4613031 2108180843"; // test old linux kernel
        ofs1.close();

        PROCESS_DIR = ".";
        bfs::create_directories("./1");
        ofstream ofs2("./1/stat", std::ios::trunc);
        ofs2 << "1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 "
                "4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 "
                "140727020027760 140727020027777 140727020027777 140727020027887 0";
        ofs2.close();
    }

    void TearDown() override {
        bfs::remove_all("./1");
        bfs::remove_all("./stat");
    }
};

void LinuxSystemInterfaceUnittest::TestGetSystemInformationOnce() const {
    SystemInformation systemInfo;
    LinuxSystemInterface::GetInstance()->GetSystemInformationOnce(systemInfo);
    APSARA_TEST_EQUAL_FATAL(systemInfo.bootTime, 1731142542);
};

void LinuxSystemInterfaceUnittest::TestGetCPUInformationOnce() const {
    CPUInformation cpuInfo;
    LinuxSystemInterface::GetInstance()->GetCPUInformationOnce(cpuInfo);
    auto cpus = cpuInfo.stats;
    APSARA_TEST_EQUAL_FATAL(4, cpus.size());
    APSARA_TEST_EQUAL_FATAL(-1, cpus[0].index);
    APSARA_TEST_EQUAL_FATAL(1195061569, cpus[0].user);
    APSARA_TEST_EQUAL_FATAL(1728645, cpus[0].nice);
    APSARA_TEST_EQUAL_FATAL(418424132, cpus[0].system);
    APSARA_TEST_EQUAL_FATAL(203670447952, cpus[0].idle);
    APSARA_TEST_EQUAL_FATAL(14723544, cpus[0].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].irq);
    APSARA_TEST_EQUAL_FATAL(773400, cpus[0].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[0].guestNice);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[1].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[1].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[1].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[1].idle);
    APSARA_TEST_EQUAL_FATAL(57199, cpus[1].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].irq);
    APSARA_TEST_EQUAL_FATAL(424744, cpus[1].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[1].steal);
    APSARA_TEST_EQUAL_FATAL(1, cpus[1].guest);
    APSARA_TEST_EQUAL_FATAL(2, cpus[1].guestNice);
    APSARA_TEST_EQUAL_FATAL(1, cpus[2].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[2].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[2].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[2].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[2].idle);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].irq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[2].guestNice);
    APSARA_TEST_EQUAL_FATAL(3, cpus[3].index);
    APSARA_TEST_EQUAL_FATAL(14708487, cpus[3].user);
    APSARA_TEST_EQUAL_FATAL(14216, cpus[3].nice);
    APSARA_TEST_EQUAL_FATAL(4613031, cpus[3].system);
    APSARA_TEST_EQUAL_FATAL(2108180843, cpus[3].idle);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].iowait);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].irq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].softirq);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].steal);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].guest);
    APSARA_TEST_EQUAL_FATAL(0, cpus[3].guestNice);
};

void LinuxSystemInterfaceUnittest::TestGetProcessListInformationOnce() const {
    ProcessListInformation processListInfo;
    LinuxSystemInterface::GetInstance()->GetProcessListInformationOnce(processListInfo);
    APSARA_TEST_EQUAL_FATAL(1, processListInfo.pids.size());
    APSARA_TEST_EQUAL_FATAL(1, processListInfo.pids[0]);
};

void LinuxSystemInterfaceUnittest::TestGetProcessInformationOnce() const {
    ProcessInformation processInfo;
    LinuxSystemInterface::GetInstance()->GetProcessInformationOnce(1, processInfo);
    APSARA_TEST_EQUAL_FATAL(1, processInfo.stat.pid);
    APSARA_TEST_EQUAL_FATAL("cat", processInfo.stat.name);
    APSARA_TEST_EQUAL_FATAL('R', processInfo.stat.state);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.parentPid);
    APSARA_TEST_EQUAL_FATAL(34816, processInfo.stat.tty);
    APSARA_TEST_EQUAL_FATAL(1110, processInfo.stat.minorFaults);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.majorFaults);
    APSARA_TEST_EQUAL_FATAL(20, processInfo.stat.priority);
    APSARA_TEST_EQUAL_FATAL(0, processInfo.stat.nice);
    APSARA_TEST_EQUAL_FATAL(1, processInfo.stat.numThreads);
    APSARA_TEST_EQUAL_FATAL(171, processInfo.stat.rss);
};

void LinuxSystemInterfaceUnittest::TestGetProcessListInformationOncePathDeleting() const {
    std::string mTestDir = "./tmp";
    bfs::create_directories(mTestDir);
    // 添加一些进程
    int n = 5;
    int del = 1;
    for (int i = 1; i <= n; ++i) {
        std::string path = mTestDir + "/" + std::to_string(i);
        if (!bfs::exists(path)) {
            bfs::create_directories(path);
        }
    }

    ProcessListInformation processListInfo;
    std::atomic<bool> threadStarted(false);

    // 遍历过程中，删除进程
    PROCESS_DIR = mTestDir;

    // 启动单独线程执行进程列表获取
    std::thread t([&]() {
        threadStarted = true;
        LinuxSystemInterface::GetInstance()->GetProcessListInformationOnce(processListInfo);
    });

    // 等待线程开始执行（确保已进入目录遍历）
    while (!threadStarted) {
        std::this_thread::yield();
    }

    // 模拟在遍历过程中删除部分进程目录
    for (int i = n; i > n - del; --i) {
        bfs::remove_all(mTestDir + "/" + std::to_string(i));
    }

    // 等待获取进程列表操作完成
    t.join();

    // 验证结果：应该只包含未被删除的进程
    APSARA_TEST_EQUAL_FATAL(n - del, processListInfo.pids.size());

    // 验证剩余进程ID的正确性
    for (int i = 1; i <= n - del; ++i) {
        bool found = false;
        for (auto pid : processListInfo.pids) {
            if (pid == i) {
                found = true;
                break;
            }
        }
        APSARA_TEST_TRUE_FATAL(found);
    }

    // 验证被删除的进程不在列表中
    for (int i = n - del + 1; i < n; ++i) {
        for (auto pid : processListInfo.pids) {
            APSARA_TEST_NOT_EQUAL_FATAL(i, pid);
        }
    }

    // 清理
    for (int i = 1; i <= n - del; i++) {
        bfs::remove_all(mTestDir + "/" + std::to_string(i));
    }
    PROCESS_DIR = ".";
};

void LinuxSystemInterfaceUnittest::TestGetProcessOpenFilesOnce() const {
    int pid = 2;
    std::string mTestDir = "./tmp";
    PROCESS_DIR = mTestDir;
    bfs::create_directories(mTestDir);
    bfs::create_directories(mTestDir + "/" + std::to_string(pid));
    bfs::create_directories(mTestDir + "/" + std::to_string(pid) + "/fd");
    ProcessFd processFd;

    // 创建5个测试文件作为文件描述符目标
    for (int i = 0; i < 5; ++i) {
        std::string targetFile = mTestDir + "/target" + std::to_string(i);
        std::ofstream ofs(targetFile); // 创建空文件
        // 创建文件描述符符号链接 (0,1,2,3,4)
        bfs::create_symlink(targetFile, mTestDir + "/" + std::to_string(pid) + "/fd/" + std::to_string(i));
    }

    // 启动单独线程执行文件读取
    std::atomic<bool> threadStarted(false);
    bool result = false;
    std::thread t([&]() {
        threadStarted = true;
        result = LinuxSystemInterface::GetInstance()->GetProcessOpenFilesOnce(pid, processFd);
    });

    // 等待线程开始执行（确保已进入读取文件进程）
    while (!threadStarted) {
        std::this_thread::yield();
    }

    sleep(1);
    // 删除
    bfs::remove_all(mTestDir + "/" + std::to_string(pid) + "/fd/4");

    // 等待获取进程列表操作完成
    t.join();

    bfs::remove_all(mTestDir);
    PROCESS_DIR = ".";
};

void LinuxSystemInterfaceUnittest::TestGetProcessOpenFilesOncePermissionDenied() const {
    int pid = 2;
    std::string mTestDir = "./tmp_permission";
    PROCESS_DIR = mTestDir;
    bfs::create_directories(mTestDir + "/" + std::to_string(pid) + "/fd");

    // 设置fd目录为不可读权限
    chmod((mTestDir + "/" + std::to_string(pid) + "/fd").c_str(), 0000);

    ProcessFd processFd;
    LinuxSystemInterface::GetInstance()->GetProcessOpenFilesOnce(pid, processFd);


    // 恢复权限以便清理
    chmod((mTestDir + "/" + std::to_string(pid) + "/fd").c_str(), 0755);
    bfs::remove_all(mTestDir);
    PROCESS_DIR = ".";
}

void LinuxSystemInterfaceUnittest::TestGetProcessOpenFilesOnceFilesystemError() const {
    // 测试访问损坏的符号链接
    int pid = 2;
    std::string mTestDir = "./tmp_fs_error";
    PROCESS_DIR = mTestDir;
    bfs::create_directories(mTestDir + "/" + std::to_string(pid));

    // 创建一个损坏的符号链接，触发filesystem_error
    symlink("/non-existent-target", (mTestDir + "/" + std::to_string(pid) + "/fd").c_str());

    ProcessFd processFd;
    bool result = LinuxSystemInterface::GetInstance()->GetProcessOpenFilesOnce(pid, processFd);

    // 验证：当遇到filesystem_error时，应返回false且不添加任何结果
    APSARA_TEST_FALSE_FATAL(result);


    bfs::remove_all(mTestDir);
    PROCESS_DIR = ".";

    // 测试删除目录

    mTestDir = "./tmp";
    PROCESS_DIR = mTestDir;
    bfs::create_directories(mTestDir);
    bfs::create_directories(mTestDir + "/" + std::to_string(pid));
    bfs::create_directories(mTestDir + "/" + std::to_string(pid) + "/fd");

    // 创建5个测试文件作为文件描述符目标
    for (int i = 0; i < 5; ++i) {
        std::string targetFile = mTestDir + "/target" + std::to_string(i);
        std::ofstream ofs(targetFile); // 创建空文件
        // 创建文件描述符符号链接 (0,1,2,3,4)
        bfs::create_symlink(targetFile, mTestDir + "/" + std::to_string(pid) + "/fd/" + std::to_string(i));
    }

    // 启动单独线程执行文件读取
    std::atomic<bool> threadStarted(false);
    std::thread t([&]() {
        threadStarted = true;
        result = LinuxSystemInterface::GetInstance()->GetProcessOpenFilesOnce(pid, processFd);
    });

    // 等待线程开始执行（确保已进入读取文件进程）
    while (!threadStarted) {
        std::this_thread::yield();
    }

    sleep(1);
    // 删除
    bfs::remove_all(mTestDir + "/" + std::to_string(pid) + "/fd");

    // 等待获取进程列表操作完成
    t.join();

    bfs::remove_all(mTestDir);
    PROCESS_DIR = ".";
}


UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetSystemInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetCPUInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessListInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessInformationOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessListInformationOncePathDeleting);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessOpenFilesOnce);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessOpenFilesOncePermissionDenied);
UNIT_TEST_CASE(LinuxSystemInterfaceUnittest, TestGetProcessOpenFilesOnceFilesystemError);

} // namespace logtail

UNIT_TEST_MAIN
