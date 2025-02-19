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
    void TestGetStatus();
    void TestGetPIDCaps();
    void TestGetPIDNsInode();
    void TestProcsFilename();

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
        const char cmdline[] = {'t', 'e',  's', 't', '\0', 'p', 'r',  'o', 'g', 'r', 'a',
                                'm', '\0', 'a', 'r', 'g',  '1', '\0', 'a', 'r', 'g', '2'};
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

private:
    std::filesystem::path mTestRoot;
    std::filesystem::path mProcDir;
    std::unique_ptr<ProcParser> mParser;
};

void ProcParserUnittest::TestGetPIDCmdline() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string cmdline = mParser->GetPIDCmdline(testPid);
    APSARA_TEST_STREQ_DESC(cmdline.c_str(), "test program arg1 arg2", "Cmdline should match");
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

    auto [cwd, flags] = mParser->GetPIDCWD(testPid);
    APSARA_TEST_TRUE(cwd.find("/home/user") != std::string::npos);
    APSARA_TEST_EQUAL(flags & static_cast<uint32_t>(ApiEventFlag::RootCWD), 0);
}

void ProcParserUnittest::TestGetPIDDockerId() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string dockerId = mParser->GetPIDDockerId(testPid);
    APSARA_TEST_STREQ_DESC(
        dockerId.c_str(), "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef", "Docker ID should match");
}

void ProcParserUnittest::TestGetPIDExePath() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    std::string exePath = mParser->GetPIDExePath(testPid);
    APSARA_TEST_TRUE(exePath.find("/usr/bin/test_program") != std::string::npos);
}

void ProcParserUnittest::TestGetStatus() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    auto status = mParser->GetStatus(testPid);
    APSARA_TEST_TRUE(status != nullptr);

    auto uids = status->GetUids();
    APSARA_TEST_EQUAL(uids.size(), 4);
    for (const auto& uid : uids) {
        APSARA_TEST_EQUAL(uid, 1000);
    }

    auto gids = status->GetGids();
    APSARA_TEST_EQUAL(gids.size(), 4);
    for (const auto& gid : gids) {
        APSARA_TEST_EQUAL(gid, 1000);
    }

    APSARA_TEST_EQUAL(status->GetLoginUid(), 1000);
}

void ProcParserUnittest::TestGetPIDNsInode() {
    const int testPid = 12345;
    CreateProcTestFiles(testPid);

    uint32_t nsInode = mParser->GetPIDNsInode(testPid, "net");
    APSARA_TEST_EQUAL(nsInode, 4026531992);
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
UNIT_TEST_CASE(ProcParserUnittest, TestGetStatus);
UNIT_TEST_CASE(ProcParserUnittest, TestGetPIDNsInode);
UNIT_TEST_CASE(ProcParserUnittest, TestProcsFilename);

} // namespace logtail

UNIT_TEST_MAIN
