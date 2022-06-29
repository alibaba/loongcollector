/*
 * Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include <string>
#include <fstream>
#include <boost/format.hpp>
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "common/LogtailCommonFlags.h"

namespace logtail {

class FileSystemUtilUnittest : public ::testing::Test {
protected:
    static std::string mRootDir;

    // Because file system operation can't lock any files or dirs, so it
    // is important to test if the test root directory can be removed.
    // Use mTestRoot to record the test root directory of each case.
    bfs::path mTestRoot;

    void SetUp() {
        auto testCaseName = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        mTestRoot = bfs::path(mRootDir) / testCaseName;
        ASSERT_TRUE(bfs::create_directories(mTestRoot));
    }
    void TearDown() {
        // Delete the whole folder to prove that Dir will not locking any files or dirs.
        try {
            bfs::remove_all(mTestRoot);
        } catch (...) {
            ASSERT_TRUE(false);
        }
    }

public:
    static void SetUpTestCase() {
        bfs::path rootPath(GetProcessExecutionDir());
        rootPath /= "CommonUnittest";
        bfs::remove_all(rootPath);
        EXPECT_TRUE(bfs::create_directories(rootPath));
        mRootDir = rootPath.string();
    }
    static void TearDownTestCase() { EXPECT_TRUE(bfs::remove_all(mRootDir)); }
};

std::string FileSystemUtilUnittest::mRootDir;

#if defined(_MSC_VER)
TEST_F(FileSystemUtilUnittest, TestWindowsRootPath) {
    fsutil::PathStat ps;

    BOOL_FLAG(enable_root_path_collection) = false;
    EXPECT_TRUE(!fsutil::PathStat::stat("C:", ps));
    EXPECT_TRUE(fsutil::PathStat::stat("C:\\", ps));
    {
        fsutil::Dir dir("C:");
        EXPECT_TRUE(dir.Open());
    }

    BOOL_FLAG(enable_root_path_collection) = true;
    EXPECT_TRUE(fsutil::PathStat::stat("C:", ps));
    EXPECT_TRUE(fsutil::PathStat::stat("C:\\", ps));

    BOOL_FLAG(enable_root_path_collection) = false;
}
#endif

TEST_F(FileSystemUtilUnittest, TestDirNormal) {
    // Create directory and files for test.
    std::set<std::string> subFiles = {
        "f1",
        "f2",
        "f3",
        "f10",
    };
    std::set<std::string> hiddenFiles = {".hidden_f1", ".hidden_f2"};
    std::set<std::string> subDirs = {"d1", "d2", "d3", "d100"};
    std::set<std::string> hiddenDirs = {".hidden_d1", ".hidden_d2"};
    for (auto& f : subFiles)
        std::ofstream((mTestRoot / f).string());
    for (auto& f : hiddenFiles)
        std::ofstream((mTestRoot / f).string());
    for (auto& d : subDirs)
        bfs::create_directory(mTestRoot / d);
    for (auto& d : hiddenDirs)
        bfs::create_directory(mTestRoot / d);

    // Use Dir to iterate it.
    fsutil::Dir rootDir(mTestRoot.string());
    EXPECT_TRUE(rootDir.Open());

    std::set<std::string> filesFind, dirsFind;
    fsutil::Entry entry;
    while ((entry = rootDir.ReadNext())) {
        EXPECT_FALSE(entry.IsSymbolic());
        if (entry.IsDir())
            dirsFind.insert(entry.Name());
        else if (entry.IsRegFile()) {
            filesFind.insert(entry.Name());

            // Delete the first file to prove that iteration will not lock the file.
            static bool isFirstFile = true;
            if (isFirstFile) {
                EXPECT_TRUE(bfs::remove(mTestRoot / entry.Name()));
                isFirstFile = false;
            }
        } else {
            EXPECT_TRUE(false);
        }
    }
    EXPECT_EQ(subFiles, filesFind);
    EXPECT_EQ(subDirs, dirsFind);
}

#ifndef _MSC_VER
TEST_F(FileSystemUtilUnittest, TestDirSymbolic) {
    { std::ofstream((mTestRoot / "f1").string()); }
    bfs::create_directory(mTestRoot / "d1");
    std::map<std::string, std::string> symbolics = {{"s1", "f1"}, {"s2", "d1"}};
    for (auto& s : symbolics) {
        system(boost::str(boost::format("ln -s %s %s") % (mTestRoot / s.second) % (mTestRoot / s.first)).c_str());
    }

    {
        fsutil::Dir rootDir(mTestRoot.string());
        EXPECT_TRUE(rootDir.Open());
        fsutil::Entry entry;
        while ((entry = rootDir.ReadNext(false))) {
            if (symbolics.find(entry.Name()) == symbolics.end()) {
                continue;
            }

            EXPECT_TRUE(entry.IsSymbolic());
            EXPECT_TRUE(!entry.IsDir() && !entry.IsRegFile());
        }
    }
    {
        fsutil::Dir rootDir(mTestRoot.string());
        EXPECT_TRUE(rootDir.Open());
        fsutil::Entry entry;
        while ((entry = rootDir.ReadNext(true))) {
            if (symbolics.find(entry.Name()) == symbolics.end()) {
                continue;
            }

            EXPECT_TRUE(entry.IsSymbolic());
            EXPECT_TRUE(entry.IsDir() || entry.IsRegFile());
        }
    }
}
#endif

TEST_F(FileSystemUtilUnittest, TestCheckExistance) {
    std::string subFileName = "file";
    std::string subDirName = "dir";
    std::ofstream((mTestRoot / subFileName).string());
    bfs::create_directory(mTestRoot / subDirName);

    EXPECT_TRUE(CheckExistance((mTestRoot / subFileName).string()));
    EXPECT_TRUE(CheckExistance((mTestRoot / subDirName).string()));
    EXPECT_FALSE(CheckExistance((mTestRoot / "not_exist").string()));
}

TEST_F(FileSystemUtilUnittest, TestPathStat_stat) {
    auto currentTime = time(NULL);

    {
        auto filePath = ((mTestRoot / "file").string());
        { std::ofstream(filePath).write("xxx", 3); }
        fsutil::PathStat stat;
        EXPECT_TRUE(fsutil::PathStat::stat(filePath, stat));
        DevInode devInode = stat.GetDevInode();
        EXPECT_EQ(devInode, GetFileDevInode(filePath));
        EXPECT_GT(devInode.dev, 0);
        EXPECT_GT(devInode.inode, 0);
        int64_t sec = -1, nsec = -1;
        stat.GetLastWriteTime(sec, nsec);
        EXPECT_GE(sec, 0);
        EXPECT_GE(nsec, 0);
        EXPECT_EQ(stat.GetFileSize(), 3);
        EXPECT_GE(stat.GetMtime(), currentTime);
    }

    {
        auto dirPath = ((mTestRoot / "dir")).string();
        EXPECT_TRUE(bfs::create_directory(dirPath));
        fsutil::PathStat stat;
        EXPECT_TRUE(fsutil::PathStat::stat(dirPath, stat));
        DevInode devInode = stat.GetDevInode();
        EXPECT_EQ(devInode, GetFileDevInode(dirPath));
        EXPECT_GT(devInode.dev, 0);
        EXPECT_GT(devInode.inode, 0);
        int64_t sec = -1, nsec = -1;
        stat.GetLastWriteTime(sec, nsec);
        EXPECT_GE(sec, 0);
        EXPECT_GE(nsec, 0);
#if defined(__linux__)
        EXPECT_EQ(stat.GetFileSize(), 4096);
#elif defined(_MSC_VER)
        EXPECT_EQ(stat.GetFileSize(), 0);
#endif
        EXPECT_GE(stat.GetMtime(), currentTime);
    }
}

TEST_F(FileSystemUtilUnittest, TestPathStat_fstat) {
    auto currentTime = time(NULL);
    auto filePath = ((mTestRoot / "file").string());
    { std::ofstream(filePath).write("xxx", 3); }

    FILE* file = fopen(filePath.c_str(), "r");
    EXPECT_TRUE(file != NULL);
    fsutil::PathStat stat;
    EXPECT_TRUE(fsutil::PathStat::fstat(file, stat));
    DevInode devInode = stat.GetDevInode();
    EXPECT_EQ(devInode, GetFileDevInode(filePath));
    EXPECT_GT(devInode.dev, 0);
    EXPECT_GT(devInode.inode, 0);
    int64_t sec = -1, nsec = -1;
    stat.GetLastWriteTime(sec, nsec);
    EXPECT_GE(sec, 0);
    EXPECT_GE(nsec, 0);
    EXPECT_EQ(stat.GetMtime(), sec);
    EXPECT_EQ(stat.GetFileSize(), 3);
    EXPECT_GE(stat.GetMtime(), currentTime);
    fclose(file);

#if defined(_MSC_VER)
    HANDLE hFile = CreateFile(filePath.c_str(),
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    EXPECT_TRUE(hFile != INVALID_HANDLE_VALUE);
    fsutil::PathStat handleStat;
    EXPECT_TRUE(fsutil::PathStat::fstat(hFile, handleStat));
    EXPECT_EQ(stat.GetFileSize(), handleStat.GetFileSize());
    EXPECT_EQ(stat.GetMtime(), handleStat.GetMtime());
    int64_t handleStatSec = -1, handleStatNsec = -1;
    handleStat.GetLastWriteTime(handleStatSec, handleStatNsec);
    EXPECT_EQ(handleStat.GetMtime(), handleStatSec);
    CloseHandle(hFile);
#endif
}

TEST_F(FileSystemUtilUnittest, TestPathStat_GetLastWriteTime) {
    auto filePath = ((mTestRoot / "file").string());
    { std::ofstream(filePath).write("xxx", 3); }

    {
        int64_t sec = -1, nsec = -1;
        fsutil::PathStat fileStat;
        fsutil::PathStat::stat(filePath, fileStat);
        fileStat.GetLastWriteTime(sec, nsec);
        EXPECT_GE(sec, 0);
        EXPECT_GE(nsec, 0);
    }

    {
        int64_t sec = -1, nsec = -1;
        fsutil::PathStat dirStat;
        fsutil::PathStat::stat(mTestRoot.string(), dirStat);
        dirStat.GetLastWriteTime(sec, nsec);
        EXPECT_GE(sec, 0);
        EXPECT_GE(nsec, 0);
    }
}

TEST_F(FileSystemUtilUnittest, TestFileReadOnlyOpen) {
    auto filePath = ((mTestRoot / "file").string());
    const std::string fileContent{"xxx"};
    { std::ofstream(filePath) << fileContent; }

    // Open the file and delete it before closing.
    // File can still be read after deleting.
    // After closing, the file is no more existing.
    auto file = FileReadOnlyOpen(filePath.c_str(), "r");
    EXPECT_TRUE(file != NULL);
    EXPECT_TRUE(bfs::remove(filePath));
    std::vector<char> buffer(fileContent.length(), '\0');
    auto nbytes = fread(buffer.data(), sizeof(char), fileContent.length(), file);
    EXPECT_EQ(nbytes, fileContent.length());
    EXPECT_EQ(std::string(buffer.data(), nbytes), fileContent);
    fclose(file);
    EXPECT_FALSE(bfs::exists(filePath));
}

TEST_F(FileSystemUtilUnittest, TestFileWriteOnlyOpen) {
    auto filePath = (mTestRoot / "file").string();
    const std::string fileContent{"xxx"};
    const auto fileContentLen = fileContent.length();
    std::vector<char> buffer(fileContentLen, '\0');

    // Case #1: File is not existing, open it.
    // And others can also read/delete the file.
    // After writing, others can also read the new content.
    {
        auto file = FileWriteOnlyOpen(filePath.c_str(), "w");
        ASSERT_TRUE(file != NULL);
        // Write some data.
        EXPECT_EQ(fwrite(fileContent.data(), 1, fileContent.length(), file), fileContentLen);
        fflush(file);
        // Open with C++ fstream.
        std::ifstream in(filePath);
        EXPECT_TRUE(in.good());
        {
            EXPECT_TRUE(in.read(buffer.data(), fileContentLen));
            EXPECT_EQ(std::string(buffer.data(), buffer.size()), fileContent);
        }
        // Open with C file.
        FILE* cfile = fopen(filePath.c_str(), "r");
        EXPECT_TRUE(cfile != NULL);
        {
            EXPECT_EQ(fread(buffer.data(), 1, buffer.size(), cfile), fileContentLen);
            EXPECT_EQ(std::string(buffer.data(), buffer.size()), fileContent);
        }
        // Write more data.
        EXPECT_EQ(fwrite(fileContent.data(), 1, fileContent.length(), file), fileContentLen);
        fflush(file);
        // Reading more data.
        {
            EXPECT_TRUE(in.read(buffer.data(), fileContentLen));
            EXPECT_EQ(std::string(buffer.data(), buffer.size()), fileContent);
        }
        {
            EXPECT_EQ(fread(buffer.data(), 1, buffer.size(), cfile), fileContentLen);
            EXPECT_EQ(std::string(buffer.data(), buffer.size()), fileContent);
        }
        in.close();
        fclose(cfile);
        // Delete the file.
        EXPECT_TRUE(bfs::remove(filePath));
        fclose(file);
        EXPECT_FALSE(bfs::exists(filePath));
    }

    // Case #2: File is existing, open will truncate it.
    {
        { std::ofstream(filePath) << fileContent; }

        auto file = FileWriteOnlyOpen(filePath.c_str(), "w");
        ASSERT_TRUE(file != NULL);
        EXPECT_EQ(fread(buffer.data(), 1, buffer.size(), file), 0);
        fclose(file);
    }
}

TEST_F(FileSystemUtilUnittest, TestFileAppendOpen) {
    auto filePath = (mTestRoot / "file").string();
    const std::string fileContent{"xxx"};
    const auto fileContentLen = fileContent.length();

    // Case #1: File is not existing, open it and append something.
    {
        auto file = FileAppendOpen(filePath.c_str(), "a");
        ASSERT_TRUE(file != NULL);
        EXPECT_EQ(fwrite(fileContent.data(), 1, fileContentLen, file), fileContentLen);
        fflush(file);

        std::ifstream in(filePath);
        std::string content;
        in >> content;
        EXPECT_EQ(content, fileContent);

        fclose(file);
    }

    // Case #2: Append more, and delete it.
    {
        auto file = FileAppendOpen(filePath.c_str(), "a");
        ASSERT_TRUE(file != NULL);
        EXPECT_EQ(ftell(file), fileContentLen);
        EXPECT_EQ(fwrite(fileContent.data(), 1, fileContentLen, file), fileContentLen);
        EXPECT_EQ(ftell(file), fileContentLen * 2);
        fflush(file);

        FILE* in = fopen(filePath.c_str(), "r");
        EXPECT_TRUE(in != NULL);
        std::vector<char> buffer(fileContentLen * 2, '\0');
        EXPECT_EQ(fread(buffer.data(), 1, buffer.size(), in), buffer.size());
        EXPECT_EQ(std::string(buffer.data(), buffer.size()), fileContent + fileContent);
        fclose(in);
        // Delete the file.
        EXPECT_TRUE(bfs::remove(filePath));
        fclose(file);
        EXPECT_FALSE(bfs::exists(filePath));
    }

    // Case #3: Open existing file, check its cursor position.
    {
        { std::ofstream(filePath) << fileContent; }

        auto file = FileAppendOpen(filePath.c_str(), "a");
        EXPECT_EQ(ftell(file), fileContentLen);
        fclose(file);
    }
}

TEST_F(FileSystemUtilUnittest, TestFSeekAndFTell) {
    auto filePath = (mTestRoot / "file").string();

    // Write 5GB to file.
    {
        std::ofstream out(filePath);
        std::vector<char> content(1024 * 1024, 'A');
        for (int i = 0; i < 1024 * 5; ++i) {
            out.write(content.data(), content.size());
        }
    }

    auto file = FileReadOnlyOpen(filePath.c_str());
    EXPECT_TRUE(file != NULL);
    EXPECT_EQ(0, FSeek(file, 0, SEEK_END));
    EXPECT_EQ(int64_t(1024) * 1024 * 1024 * 5, FTell(file));

    EXPECT_EQ(0, FSeek(file, int64_t(4) * 1024 * 1024 * 1024 + 100, SEEK_SET));
    EXPECT_EQ(int64_t(4) * 1024 * 1024 * 1024 + 100, FTell(file));

    EXPECT_EQ(0, FSeek(file, 900, SEEK_CUR));
    EXPECT_EQ(int64_t(4) * 1024 * 1024 * 1024 + 1000, FTell(file));
    fclose(file);
}

TEST_F(FileSystemUtilUnittest, TestGetFdPath) {
    auto filePath = (mTestRoot / "file").string();

    auto file = FileWriteOnlyOpen(filePath.c_str());
    EXPECT_TRUE(file != NULL);
#if defined(_MSC_VER)
    EXPECT_EQ(filePath, GetFdPath(_fileno(file)));
#else
    EXPECT_EQ(filePath, GetFdPath(fileno(file)));
#endif
    fclose(file);
}

} // namespace logtail
