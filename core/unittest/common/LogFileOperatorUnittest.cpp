// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <string>
#include <cstdlib>
#if defined(__linux__)
#include <unistd.h>
#endif
#include "common/LogFileOperator.h"
#include "common/FileSystemUtil.h"
#if defined(__linux__)
#include "unittest/UnittestHelper.h"
#endif

namespace logtail {

const char* gTestFile = "test.txt";

class LogFileOperatorUnittest : public ::testing::Test {
    static std::string gRootDir;

    void MountFuseDir(std::string fuse_dir);
    void UmountFuseDir(std::string fuse_dir);
    std::string GenerateData(size_t lines, size_t line_size);

    void MockTruncate(const char* path, off_t keep_length, off_t offset, size_t filesize);

public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    void TestCons();
    void TestOpen();
    void TestSeek();
    void TestStat();
    void TestPread();
    void TestSkipHoleRead();
    void TestTell();
    void TestClose();
    void TestFuseTruncate();
};

APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestCons, 0);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestOpen, 1);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestSeek, 2);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestStat, 3);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestPread, 4);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestSkipHoleRead, 5);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestTell, 6);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestClose, 7);
APSARA_UNIT_TEST_CASE(LogFileOperatorUnittest, TestFuseTruncate, 8);

std::string LogFileOperatorUnittest::gRootDir = "";

void LogFileOperatorUnittest::MountFuseDir(std::string fuse_dir) {
    std::string cmd;
    cmd = std::string("mkdir -p ") + fuse_dir;
    system(cmd.c_str());
    cmd = std::string("sudo mount -t tmpfs -o size=128m tmpfs ") + fuse_dir;
    system(cmd.c_str());
}

void LogFileOperatorUnittest::UmountFuseDir(std::string fuse_dir) {
    std::string cmd;
    cmd = std::string("sudo umount ") + fuse_dir;
    system(cmd.c_str());
    cmd = std::string("rm -rf ") + fuse_dir;
    system(cmd.c_str());
}

void LogFileOperatorUnittest::SetUpTestCase() {
    srand(time(NULL));
    gRootDir = GetProcessExecutionDir();
    gRootDir += "LogFileOperatorUnittest";
    bfs::remove_all(gRootDir);
    bfs::create_directories(gRootDir);
}

void LogFileOperatorUnittest::TearDownTestCase() {
    bfs::remove_all(gRootDir);
}

std::string LogFileOperatorUnittest::GenerateData(size_t lines, size_t line_size) {
    std::string rawLog;
    int32_t mod;
    for (size_t i = 0; i < lines; ++i) {
        for (size_t j = 0; j < line_size; ++j) {
            mod = rand() % 26;
            rawLog.append(1, char('A' + mod));
        }
        rawLog.append("\n");
    }
    return rawLog;
}

#if defined(_MSC_VER)
#define ftruncate _chsize
#endif

void LogFileOperatorUnittest::MockTruncate(const char* path, off_t keep_length, off_t offset, size_t filesize) {
    size_t len = filesize - (size_t)offset;
    FILE* pFile = fopen(path, "wb");
    char* buf = new char[len];
    fseek(pFile, offset, SEEK_SET);
    fread(buf, 1, len, pFile);
    ftruncate(fileno(pFile), keep_length);
    fseek(pFile, offset, SEEK_SET);
    fwrite(buf, 1, len, pFile);
    delete[] buf;
    fclose(pFile);
}

void LogFileOperatorUnittest::TestCons() {
    LogFileOperator logFileOp;
    APSARA_TEST_EQUAL(logFileOp.mFuseMode, false);
    APSARA_TEST_EQUAL(logFileOp.mFd, -1);
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);
}

void LogFileOperatorUnittest::TestOpen() {
    std::string cmd;
    int fd;

    // non fuse, file no existed & existed
    {
        std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
        bfs::remove(file);

        LogFileOperator logFileOp;

        fd = logFileOp.Open("");
        APSARA_TEST_TRUE(fd < 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);
        APSARA_TEST_EQUAL(logFileOp.mFuseMode, false);

        fd = logFileOp.Open(file.c_str());
        APSARA_TEST_TRUE(fd < 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);
        APSARA_TEST_EQUAL(logFileOp.mFuseMode, false);

        { std::ofstream(file, std::ios_base::binary) << ""; }

        // non fuse, fise existed
        fd = logFileOp.Open(file.c_str());
        APSARA_TEST_TRUE(fd >= 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        APSARA_TEST_EQUAL(logFileOp.mFuseMode, false);
    }

#if defined(ENABLE_FUSE)
    // fuse, file no existed & existed
    {
        std::string fuse_dir = gRootDir + PATH_SEPARATOR + "fuse_dir";
        MountFuseDir(fuse_dir);
        std::string file = fuse_dir + PATH_SEPARATOR + gTestFile;

        LogFileOperator logFileOp;

        fd = logFileOp.Open("");
        APSARA_TEST_TRUE(fd < 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        fd = logFileOp.Open(file.c_str(), true);
        APSARA_TEST_TRUE(fd < 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        cmd = std::string("touch ") + file;
        system(cmd.c_str());

        fd = logFileOp.Open(file.c_str(), true);
        APSARA_TEST_TRUE(fd >= 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        APSARA_TEST_EQUAL(logFileOp.mFuseMode, true);

        int ret = logFileOp.Close();
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        UmountFuseDir(fuse_dir);
    }
#endif
}

void LogFileOperatorUnittest::TestSeek() {
    int ret;
    std::string cmd;
    long int offset;
    int fd;

    // non fuse
    {
        std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
        { std::ofstream(file, std::ios_base::binary) << ""; }

        offset = 0;

        LogFileOperator logFileOp;
        ret = logFileOp.Seek(offset, SEEK_SET);
        APSARA_TEST_NOT_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_END);
        APSARA_TEST_NOT_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_CUR);
        APSARA_TEST_NOT_EQUAL(ret, 0);

        fd = logFileOp.Open(file.c_str());
        APSARA_TEST_TRUE(fd >= 0);

        ret = logFileOp.Seek(offset, SEEK_SET);
        APSARA_TEST_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_END);
        APSARA_TEST_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_CUR);
        APSARA_TEST_EQUAL(ret, 0);
    }

#if defined(ENABLE_FUSE)
    // fuse
    {
        std::string fuse_dir = gRootDir + "/fuse_dir";
        MountFuseDir(fuse_dir);
        std::string file = fuse_dir + "/" + gTestFile;
        cmd = std::string("touch ") + file;
        system(cmd.c_str());

        offset = 0;

        LogFileOperator logFileOp;
        ret = logFileOp.Seek(offset, SEEK_SET);
        APSARA_TEST_NOT_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_END);
        APSARA_TEST_NOT_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_CUR);
        APSARA_TEST_NOT_EQUAL(ret, 0);

        fd = logFileOp.Open(file.c_str(), true);
        APSARA_TEST_TRUE(fd >= 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);

        ret = logFileOp.Seek(offset, SEEK_SET);
        APSARA_TEST_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_END);
        APSARA_TEST_EQUAL(ret, 0);
        ret = logFileOp.Seek(offset, SEEK_CUR);
        APSARA_TEST_EQUAL(ret, 0);

        ret = logFileOp.Close();
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        UmountFuseDir(fuse_dir);
    }
#endif
}

void LogFileOperatorUnittest::TestStat() {
    int ret;
    std::string cmd;

    fsutil::PathStat ps;

    // non fuse
    {
        std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
        { std::ofstream(file, std::ios_base::binary) << ""; }

        LogFileOperator logFileOp;
        ret = logFileOp.Stat(ps);
        APSARA_TEST_TRUE(ret != 0);

        logFileOp.Open(file.c_str());
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        ret = logFileOp.Stat(ps);
        APSARA_TEST_EQUAL(ret, 0);
    }

#if defined(ENABLE_FUSE)
    // fuse
    {
        std::string fuse_dir = gRootDir + "/fuse_dir";
        MountFuseDir(fuse_dir);
        std::string file = fuse_dir + "/" + gTestFile;
        cmd = std::string("touch ") + file;
        system(cmd.c_str());

        LogFileOperator logFileOp;
        ret = logFileOp.Stat(ps);
        APSARA_TEST_TRUE(ret != 0);

        logFileOp.Open(file.c_str(), true);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        ret = logFileOp.Stat(ps);
        APSARA_TEST_EQUAL(ret, 0);

        ret = logFileOp.Close();
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        UmountFuseDir(fuse_dir);
    }
#endif
}

void LogFileOperatorUnittest::TestPread() {
    std::string cmd;
    int readBytes;
    int bytes;

    std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
    { std::ofstream(file, std::ios_base::binary) << ""; }

    std::string logData = GenerateData(1024, 9);
    FILE* pFile = fopen(file.c_str(), "wb");
    fwrite(logData.c_str(), 1, logData.size(), pFile);
    fclose(pFile);

    char* buf = new char[logData.size() + 1];
    LogFileOperator logFileOp;

    off_t offset = 0;
    readBytes = logFileOp.Pread(buf, 1, 1024, offset);
    APSARA_TEST_EQUAL(readBytes, 0);

    logFileOp.Open(file.c_str());
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);

    do {
        bytes = logFileOp.Pread(buf + readBytes, 1, logData.size() - readBytes, offset);
        readBytes += bytes;
        offset = (off_t)readBytes;
    } while (bytes);
    buf[readBytes] = '\0';
    std::string readData(buf);

    APSARA_TEST_EQUAL(readBytes, logData.size());
    APSARA_TEST_EQUAL(readData, logData);

    delete[] buf;
}

void LogFileOperatorUnittest::TestSkipHoleRead() {
#if defined(ENABLE_FUSE)
    int mainVersion = 0, subVersion = 0;
    if (UnitTestHelper::GetKernelVersion(mainVersion, subVersion)) {
        if (mainVersion < 3 || (mainVersion == 3 && subVersion < 1)) {
            LOG_INFO(sLogger, ("linux kernel version is lower than 3.1", "skip this case"));
            return;
        }
    } else {
        LOG_INFO(sLogger, ("failed to get linux kernel version", "skip this case"));
        APSARA_TEST_TRUE(false);
        return;
    }

    std::string cmd;
    size_t readBytes;
    size_t bytes;

    std::string fuse_dir = gRootDir + "/fuse_dir";
    MountFuseDir(fuse_dir);
    std::string file = fuse_dir + "/" + gTestFile;

    std::string logData = GenerateData(1024, 9);
    FILE* pFile = fopen(file.c_str(), "w");
    fwrite(logData.c_str(), 1, logData.size(), pFile);
    fclose(pFile);

    char* buf = new char[logData.size() + 1];

    LogFileOperator logFileOp;

    int64_t offset = 0;
    readBytes = logFileOp.SkipHoleRead(buf, 1, 1024, &offset);
    APSARA_TEST_TRUE(readBytes == 0);
    APSARA_TEST_TRUE(offset == 0);

    logFileOp.Open(file.c_str(), true);
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);

    do {
        bytes = logFileOp.SkipHoleRead(buf + readBytes, 1, logData.size() - readBytes, &offset);
        readBytes += bytes;
    } while (bytes);
    buf[readBytes] = '\0';
    std::string readData(buf);

    APSARA_TEST_EQUAL(readBytes, logData.size());
    APSARA_TEST_EQUAL(readData, logData);

    logFileOp.Close();
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

    delete[] buf;

    UmountFuseDir(fuse_dir);
#endif
}

void LogFileOperatorUnittest::TestTell() {
    std::string cmd;
    long int size;

    // non fuse
    {
        std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
        { std::ofstream(file, std::ios_base::binary) << ""; }

        LogFileOperator logFileOp;
        size = logFileOp.GetFileSize();
        APSARA_TEST_TRUE(size == -1);

        logFileOp.Open(file.c_str());
        APSARA_TEST_TRUE(logFileOp.IsOpen() == true);

        size = logFileOp.GetFileSize();
        APSARA_TEST_TRUE(size == 0);

        { std::ofstream(file, std::ios_base::binary) << "1\n"; }
        logFileOp.Seek(0, SEEK_END);
        size = logFileOp.GetFileSize();
        APSARA_TEST_EQUAL(size, 2);
    }

#if defined(ENABLE_FUSE)
    // fuse
    {
        std::string fuse_dir = gRootDir + "/fuse_dir";
        MountFuseDir(fuse_dir);
        std::string file = fuse_dir + "/" + gTestFile;
        cmd = std::string("touch ") + file;
        system(cmd.c_str());

        LogFileOperator logFileOp;
        size = logFileOp.GetFileSize();
        APSARA_TEST_TRUE(size == -1);

        logFileOp.Open(file.c_str(), true);
        APSARA_TEST_TRUE(logFileOp.IsOpen() == true);

        size = logFileOp.GetFileSize();
        APSARA_TEST_TRUE(size == 0);

        cmd = std::string("echo 1 > ") + file;
        system(cmd.c_str());
        logFileOp.Seek(0, SEEK_END);
        size = logFileOp.GetFileSize();
        APSARA_TEST_EQUAL(size, 2);

        logFileOp.Close();
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        UmountFuseDir(fuse_dir);
    }
#endif
}

void LogFileOperatorUnittest::TestClose() {
    int ret;
    std::string cmd;

    // non fuse
    {
        std::string file = gRootDir + PATH_SEPARATOR + gTestFile;
        { std::ofstream(file, std::ios_base::binary) << ""; }

        LogFileOperator logFileOp;
        ret = logFileOp.Close();
        APSARA_TEST_TRUE(ret != 0);

        logFileOp.Open(file.c_str());
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        ret = logFileOp.Close();
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);
    }

#if defined(ENABLE_FUSE)
    // fuse
    {
        std::string fuse_dir = gRootDir + "/fuse_dir";
        MountFuseDir(fuse_dir);
        std::string file = fuse_dir + "/" + gTestFile;
        cmd = std::string("touch ") + file;
        system(cmd.c_str());

        LogFileOperator logFileOp;
        ret = logFileOp.Close();
        APSARA_TEST_TRUE(ret != 0);

        logFileOp.Open(file.c_str(), true);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);
        ret = logFileOp.Close();
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

        UmountFuseDir(fuse_dir);
    }
#endif
}

#define FILESIZE (10 * 1024 * 1024)
#define OFFSET (8 * 1024 * 1024)
#define HEADSIZE (4 * 1024)

void LogFileOperatorUnittest::TestFuseTruncate() {
#if defined(ENABLE_FUSE)
    std::string fuse_dir = gRootDir + "/fuse_dir";
    MountFuseDir(fuse_dir);
    std::string file = fuse_dir + "/" + gTestFile;
    std::string cmd = std::string("touch ") + file;
    system(cmd.c_str());

    size_t lines = 1024 * 1024;
    std::string logData = GenerateData(lines, FILESIZE / lines - 1);
    FILE* pFile = fopen(file.c_str(), "w");
    fwrite(logData.c_str(), 1, logData.size(), pFile);
    fclose(pFile);

    LogFileOperator logFileOp;
    logFileOp.Open(file.c_str(), true);
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), true);

    long int filesize;
    ;
    logFileOp.Seek(0, SEEK_END);
    filesize = logFileOp.GetFileSize();
    APSARA_TEST_EQUAL(filesize, FILESIZE);

    MockTruncate(file.c_str(), HEADSIZE, OFFSET, FILESIZE);

    filesize = logFileOp.GetFileSize();
    APSARA_TEST_EQUAL(filesize, FILESIZE);

    pFile = fopen(file.c_str(), "a");
    fwrite(logData.c_str(), 1, 1, pFile);
    fclose(pFile);

    filesize = logFileOp.GetFileSize();
    APSARA_TEST_EQUAL(filesize, FILESIZE + 1);

    logFileOp.Close();
    APSARA_TEST_EQUAL(logFileOp.IsOpen(), false);

    UmountFuseDir(fuse_dir);
#endif
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
