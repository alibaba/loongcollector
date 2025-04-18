// Copyright 2023 iLogtail Authors
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

#include <coolbpf/security/bpf_process_event_type.h>
#include <cstdint>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "common/ProcParser.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcParserUnittest : public ::testing::Test {
public:
    void TestGetPIDCmdline();
    void TestGetPIDComm();
    void TestGetPIDEnviron();
    void TestGetPIDCWD();
    void TestGetPIDDockerId();
    void TestGetPIDExePath();
    void TestGetLoginUid();
    void TestGetPIDNsInode();
    void TestProcsFilename();
    void TestReadStat();
    void TestReadStatus();

protected:
    void SetUp() override {
        mTestRoot = std::filesystem::path(GetProcessExecutionDir()) / "ProcParserUnittestDir";
        mProcDir = mTestRoot / "proc";
        std::filesystem::create_directories(mProcDir);
        mParser = std::make_unique<ProcParser>(mTestRoot.string());
    }

    void TearDown() override { std::filesystem::remove_all(mTestRoot); }

    void WriteStringWithNulls(const std::filesystem::path& path, const char* data, size_t size) {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(data, size);
    }

    void CreateProcTestFiles(int pid) {
        auto pidDir = mProcDir / std::to_string(pid);
        std::filesystem::create_directories(pidDir);

        // Create cmdline file with null separators
        const char cmdline[] = {'t', 'e',  's', 't', ' ', 'p', 'r',  'o', 'g', 'r', 'a',
                                'm', '\0', 'a', 'r', 'g', '1', '\0', 'a', 'r', 'g', '2'};
        WriteStringWithNulls(pidDir / "cmdline", cmdline, sizeof(cmdline));

        // Create comm file
        std::ofstream(pidDir / "comm") << "test_program";

        // Create environ file with null separators
        const char environ[]
            = {'P', 'A', 'T', 'H', '=', '/',  'u', 's', 'r', '/', 'b', 'i', 'n', '\0', 'U', 'S', 'E', 'R',
               '=', 'r', 'o', 'o', 't', '\0', 'H', 'O', 'M', 'E', '=', '/', 'r', 'o',  'o', 't', '\0'};
        WriteStringWithNulls(pidDir / "environ", environ, sizeof(environ));

        // Create status file
        std::ofstream status(pidDir / "status");
        status << "Name:   test_program\n"
               << "Uid:    1000    1000    1000    1000\n"
               << "Gid:    1000    1000    1000    1000\n";
        status.close();

        // Create loginuid file
        std::ofstream(pidDir / "loginuid") << "1000";

        // Create cgroup file
        std::ofstream(pidDir / "cgroup")
            << "0::/docker/1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

        // Create exe symlink
        std::filesystem::create_directories(mTestRoot / "usr" / "bin");
        std::ofstream(mTestRoot / "usr" / "bin" / "test_program") << "test program binary";
        std::filesystem::create_symlink(mTestRoot / "usr" / "bin" / "test_program", pidDir / "exe");

        // Create cwd symlink
        std::filesystem::create_directories(mTestRoot / "home" / "user");
        std::filesystem::create_symlink(mTestRoot / "home" / "user", pidDir / "cwd");

        // Create ns directory and net symlink
        std::filesystem::create_directories(pidDir / "ns");
        std::filesystem::create_symlink("net:[4026531992]", pidDir / "ns" / "net");
    }

    void CreateProcTestFile(int pid, const std::string& filename, const std::string& cgroupContent) {
        auto pidDir = mProcDir / std::to_string(pid);
        std::filesystem::create_directories(pidDir);
        std::ofstream(pidDir / filename) << cgroupContent;
    }

    void CreateProcCgroupTestFile(int pid, const std::string& cgroupContent) {
        CreateProcTestFile(pid, "cgroup", cgroupContent);
    }

    void CreateProcStatTestFile(int pid, const std::string& cgroupContent) {
        CreateProcTestFile(pid, "stat", cgroupContent);
    }

    void CreateProcStatusTestFile(int pid, const std::string& cgroupContent) {
        CreateProcTestFile(pid, "status", cgroupContent);
    }

private:
    std::filesystem::path mTestRoot;
    std::filesystem::path mProcDir;
    std::unique_ptr<ProcParser> mParser;
};

void ProcParserUnittest::TestGetPIDCmdline() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string cmdline = mParser->GetPIDCmdline(testPid);
    const char expected[] = "test program\0arg1\0arg2";
    APSARA_TEST_TRUE_DESC(cmdline == std::string(expected, sizeof(expected) - 1), "Cmdline should match");
}

void ProcParserUnittest::TestGetPIDComm() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string comm = mParser->GetPIDComm(testPid);
    APSARA_TEST_STREQ_DESC(comm.c_str(), "test_program", "Comm should match");
}

void ProcParserUnittest::TestGetPIDEnviron() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string environ = mParser->GetPIDEnviron(testPid);
    APSARA_TEST_TRUE(environ.find("PATH=/usr/bin") != std::string::npos);
    APSARA_TEST_TRUE(environ.find("USER=root") != std::string::npos);
    APSARA_TEST_TRUE(environ.find("HOME=/root") != std::string::npos);
}

void ProcParserUnittest::TestGetPIDCWD() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string cwd;
    uint32_t flags = mParser->GetPIDCWD(testPid, cwd);
    APSARA_TEST_TRUE(cwd.find("/home/user") != std::string::npos);
    APSARA_TEST_EQUAL(flags & static_cast<uint32_t>(EVENT_ROOT_CWD), 0U);
}

void ProcParserUnittest::TestReadStat() {
    const int testPid = 661798;
    CreateProcStatTestFile(
        testPid,
        R"(661798 (docker-init) S 661766 661798 661798 0 -1 1077936384 1010 0 0 0 4115 7535 0 0 20 0 1 0 1149556817 1060864 1 18446744073709551615 1 1 0 0 0 0 0 3145728 0 0 0 0 17 86 0 0 0 0 0 0 0 0 0 0 0 0 0)");
    ProcessStat ps;
    APSARA_TEST_TRUE(mParser->ReadProcessStat(testPid, ps));
    APSARA_TEST_EQUAL(661798, ps.pid);
    APSARA_TEST_EQUAL("docker-init", ps.name);
    APSARA_TEST_EQUAL('S', ps.state);
    APSARA_TEST_EQUAL(661766, ps.parentPid);
    APSARA_TEST_EQUAL(0, ps.tty);
    APSARA_TEST_EQUAL(1010UL, ps.minorFaults);
    APSARA_TEST_EQUAL(0UL, ps.majorFaults);
    APSARA_TEST_EQUAL(4115UL, ps.utimeTicks);
    APSARA_TEST_EQUAL(7535UL, ps.stimeTicks);
    APSARA_TEST_EQUAL(0UL, ps.cutimeTicks);
    APSARA_TEST_EQUAL(0UL, ps.cstimeTicks);
    APSARA_TEST_EQUAL(20, ps.priority);
    APSARA_TEST_EQUAL(0, ps.nice);
    APSARA_TEST_EQUAL(1, ps.numThreads);
    APSARA_TEST_EQUAL(1149556817L, ps.startTicks);
    APSARA_TEST_EQUAL(1060864UL, ps.vSize);
    APSARA_TEST_EQUAL(1UL, ps.rss);
    APSARA_TEST_EQUAL(86, ps.processor);
}

void ProcParserUnittest::TestReadStatus() {
    const int testPid = 661798;
    CreateProcStatusTestFile(testPid, R"(Name:	docker-init
Umask:	0022
State:	S (sleeping)
Tgid:	661798
Ngid:	0
Pid:	661798
PPid:	661766
TracerPid:	0
Uid:	10	10	10	10
Gid:	10	10	10	10
FDSize:	64
Groups:	0 1 2 3 4 6 10 11 20 26 27
NStgid:	661798	1
NSpid:	661798	1
NSpgid:	661798	1
NSsid:	661798	1
VmPeak:	    1036 kB
VmSize:	    1036 kB
VmLck:	       0 kB
VmPin:	       0 kB
VmHWM:	       4 kB
VmRSS:	       4 kB
RssAnon:	       4 kB
RssFile:	       0 kB
RssShmem:	       0 kB
VmData:	     172 kB
VmStk:	     132 kB
VmExe:	     708 kB
VmLib:	       8 kB
VmPTE:	      32 kB
VmSwap:	       0 kB
HugetlbPages:	       0 kB
CoreDumping:	0
THP_enabled:	1
Threads:	1
SigQ:	10/1543275
SigPnd:	0000000000000000
ShdPnd:	0000000000000000
SigBlk:	0000000000000000
SigIgn:	0000000000300000
SigCgt:	0000000000000000
CapInh:	000000000000ffff
CapPrm:	000001ffffffffff
CapEff:	000001ffffffffff
CapBnd:	000001ffffffffff
CapAmb:	000000000000ffff
NoNewPrivs:	0
Seccomp:	0
Seccomp_filters:	0
Speculation_Store_Bypass:	thread vulnerable
Cpus_allowed:	ffffffff,ffffffff,ffffffff
Cpus_allowed_list:	0-95
Mems_allowed:	00000000,00000001
Mems_allowed_list:	0
voluntary_ctxt_switches:	9245512
nonvoluntary_ctxt_switches:	5468)");
    ProcessStatus ps;
    APSARA_TEST_TRUE(mParser->ReadProcessStatus(testPid, ps));
    APSARA_TEST_EQUAL(661798, ps.pid);
    // Test UID information (all values are 0 in the test data)
    APSARA_TEST_EQUAL(10U, ps.realUid);
    APSARA_TEST_EQUAL(10U, ps.effectiveUid);
    APSARA_TEST_EQUAL(10U, ps.savedUid);
    APSARA_TEST_EQUAL(10U, ps.fsUid);

    // Test GID information (all values are 0 in the test data)
    APSARA_TEST_EQUAL(10U, ps.realGid);
    APSARA_TEST_EQUAL(10U, ps.effectiveGid);
    APSARA_TEST_EQUAL(10U, ps.savedGid);
    APSARA_TEST_EQUAL(10U, ps.fsGid);

    // Test namespace thread group IDs
    APSARA_TEST_EQUAL(2UL, ps.nstgid.size());
    if (ps.nstgid.size() >= 2) {
        APSARA_TEST_EQUAL(661798, ps.nstgid[0]);
        APSARA_TEST_EQUAL(1, ps.nstgid[1]);
    }

    // Test process capabilities (as hex values from the status file)
    APSARA_TEST_EQUAL(0x000000000000ffffUL, ps.capInh); // 000000000000ffff
    APSARA_TEST_EQUAL(0x000001ffffffffffUL, ps.capPrm); // 000001ffffffffff
    APSARA_TEST_EQUAL(0x000001ffffffffffUL, ps.capEff); // 000001ffffffffff
}

void ProcParserUnittest::TestGetPIDDockerId() {
    const int testPid = 12345;
    // K8s containerd cgroup file
    CreateProcCgroupTestFile(
        testPid,
        R"(13:pids:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
12:hugetlb:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
11:perf_event:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
10:freezer:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
9:blkio:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
8:cpuset:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
7:net_cls,net_prio:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
6:devices:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
5:memory:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
4:ioasids:/
3:cpu,cpuacct:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
2:rdma:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
1:name=systemd:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod4d40fb1a_bbeb_4ba7_b4d7_29e268072415.slice/cri-containerd-8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09.scope
0::/
)");

    std::string dockerId = mParser->GetPIDDockerId(testPid);
    APSARA_TEST_STREQ_DESC(
        dockerId.c_str(), "8a53be7205b36249cca2cd327abc1932233426c717a5a9008eac6724dfc62f09", "Docker ID should match");

    // K8s docker cgroup file
    CreateProcCgroupTestFile(
        testPid,
        R"(12:memory:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
11:rdma:/
10:cpu,cpuacct:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
9:pids:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
8:cpuset:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
7:devices:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
6:perf_event:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
5:hugetlb:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
4:freezer:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
3:blkio:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
2:net_cls,net_prio:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
1:name=systemd:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-poddc9b221f_eb6e_4af5_8328_c9cd9058c064.slice/docker-212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60.scope
)");
    dockerId = mParser->GetPIDDockerId(testPid);
    APSARA_TEST_STREQ_DESC(
        dockerId.c_str(), "212634dc854800cbb27247c97d5314161b09b9f592bb3da2245b2f8805d81f60", "Docker ID should match");

    // local docker cgroup file
    CreateProcCgroupTestFile(testPid,
                             R"(12:blkio:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
11:ioasids:/
10:hugetlb:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
9:pids:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
8:rdma:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
7:devices:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
6:freezer:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
5:perf_event:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
4:memory:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
3:cpuset,cpu,cpuacct:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
2:net_cls,net_prio:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
1:name=systemd:/docker/f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b
0::/
)");
    dockerId = mParser->GetPIDDockerId(testPid);
    APSARA_TEST_STREQ_DESC(
        dockerId.c_str(), "f1549dc3c94d8114b4de51487eb0c31cb8169bde5f798dea5cb980ea2018771b", "Docker ID should match");

    // no containerid cgroup file
    CreateProcCgroupTestFile(testPid,
                             R"(12:blkio:/user.slice
11:ioasids:/
10:hugetlb:/
9:pids:/user.slice/user-2917.slice/session-168660.scope
8:rdma:/
7:devices:/
6:freezer:/
5:perf_event:/
4:memory:/user.slice/user-2917.slice/session-168660.scope
3:cpuset,cpu,cpuacct:/
2:net_cls,net_prio:/
1:name=systemd:/user.slice/user-2917.slice/session-168660.scope
0::/
)");
    dockerId = mParser->GetPIDDockerId(testPid);
    APSARA_TEST_STREQ_DESC(dockerId.c_str(), "", "Docker ID should match");
}

void ProcParserUnittest::TestGetPIDExePath() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string exePath = mParser->GetPIDExePath(testPid);
    APSARA_TEST_TRUE(exePath.find("/usr/bin/test_program") != std::string::npos);
}

void ProcParserUnittest::TestGetLoginUid() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);
    APSARA_TEST_EQUAL(mParser->GetLoginUid(testPid), 1000U);
}

void ProcParserUnittest::TestGetPIDNsInode() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    uint32_t nsInode = mParser->GetPIDNsInode(testPid, "net");
    APSARA_TEST_EQUAL(nsInode, 4026531992U);
}

void ProcParserUnittest::TestProcsFilename() {
    const char args[] = {'t', 'e',  's', 't', '\0', 'p', 'r',  'o', 'g', 'r', 'a',
                         'm', '\0', 'a', 'r', 'g',  '1', '\0', 'a', 'r', 'g', '2'};
    std::string argsStr(args, sizeof(args));
    auto [cmds, filename] = mParser->ProcsFilename(argsStr);
    auto idx = argsStr.find('\0');
    auto fn = argsStr.substr(0, idx);
    auto cmd = argsStr.substr(idx);
    APSARA_TEST_STREQ_DESC(filename.c_str(), "test", "Filename should match");
    APSARA_TEST_TRUE(cmds.find("program") != std::string::npos);
}

UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDCmdline);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDComm);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDEnviron);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDCWD);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDDockerId);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDExePath);
UNIT_TEST_CASE(ProcParserUnittest, TestGetLoginUid);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDNsInode);
UNIT_TEST_CASE(ProcParserUnittest, TestProcsFilename);
UNIT_TEST_CASE(ProcParserUnittest, TestReadStat);
UNIT_TEST_CASE(ProcParserUnittest, TestReadStatus);

} // namespace logtail

UNIT_TEST_MAIN
