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

#include <memory>

#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(checkpoint_find_max_file_count);

namespace logtail {

std::string kTestRootDir;

class CheckpointManagerUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        kTestRootDir = (bfs::path(GetProcessExecutionDir()) / "CheckpointManagerUnittest").string();
        bfs::remove_all(kTestRootDir);
        bfs::create_directories(kTestRootDir);
        AppConfig::GetInstance()->SetLoongcollectorConfDir(kTestRootDir);
    }

    static void TearDownTestCase() { bfs::remove_all(kTestRootDir); }

    void TestSearchFilePathByDevInodeInDirectory();
    void TestAddCheckPointStaleFileNotOverwriteActive();
    void TestAddCheckPointActiveOverwriteStale();
    void TestAddCheckPointBothExistLastWriterWins();
    void TestAddCheckPointNeitherExistLastWriterWins();

private:
    // Ownership is transferred to AddCheckPoint via release(); unique_ptr keeps construction
    // exception-safe until that hand-off.
    static std::unique_ptr<CheckPoint> MakeCheckPoint(const std::string& fileName,
                                                      const DevInode& devInode,
                                                      int64_t offset,
                                                      const std::string& configName,
                                                      const std::string& realFileName = "") {
        return std::unique_ptr<CheckPoint>(new CheckPoint(fileName,
                                                          "" /* resolvedFileName */,
                                                          offset,
                                                          0 /* signatureSize */,
                                                          0 /* signatureHash */,
                                                          devInode,
                                                          configName,
                                                          realFileName,
                                                          false /* fileOpenFlag */,
                                                          false /* containerStopped */,
                                                          "" /* containerID */,
                                                          false /* lastForceRead */));
    }
};

UNIT_TEST_CASE(CheckpointManagerUnittest, TestSearchFilePathByDevInodeInDirectory);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointStaleFileNotOverwriteActive);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointActiveOverwriteStale);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointBothExistLastWriterWins);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointNeitherExistLastWriterWins);

void CheckpointManagerUnittest::TestSearchFilePathByDevInodeInDirectory() {
    const std::string kRotateFileName = "test.log.5";
    const std::string kFileName = "test.log";
    const std::string kFilePath = (bfs::path(kTestRootDir) / kFileName).string();
    const std::string kRotateFilePath = (bfs::path(kTestRootDir) / kRotateFileName).string();
    const std::string kTempPath = (bfs::path(kTestRootDir) / ".." / kFileName).string();
    std::ofstream(kFilePath) << "";

    fsutil::PathStat ps;
    EXPECT_TRUE(fsutil::PathStat::stat(kFilePath, ps));
    auto devInode = ps.GetDevInode();

    // Rotate file in current directory.
    bfs::rename(kFilePath, kRotateFilePath);

    // Normal search.
    {
        auto const filePath = SearchFilePathByDevInodeInDirectory(kTestRootDir, 0, devInode, nullptr);
        EXPECT_TRUE(filePath);
        EXPECT_EQ(filePath.value(), kRotateFilePath);
    }

    // Exceed limit when search.
    {
        bfs::rename(kRotateFilePath, kTempPath);
        auto bakLimit = INT32_FLAG(checkpoint_find_max_file_count);
        INT32_FLAG(checkpoint_find_max_file_count) = 2;

        for (size_t idx = 1; idx < 5; ++idx) {
            std::ofstream(kFilePath + "." + std::to_string(idx)) << "";
        }
        std::map<DevInode, SplitedFilePath> cache;
        auto const filePath = SearchFilePathByDevInodeInDirectory(kTestRootDir, 0, devInode, &cache);
        EXPECT_FALSE(filePath);
        EXPECT_EQ(cache.size(), INT32_FLAG(checkpoint_find_max_file_count) + 1);

        INT32_FLAG(checkpoint_find_max_file_count) = bakLimit;
        bfs::rename(kTempPath, kRotateFilePath);
    }

    // File is moved to sub-directory.
    const auto kSubDir = bfs::path(kTestRootDir) / "sub" / "sub";
    bfs::create_directories(kSubDir);
    const auto kSubDirFilePath = (kSubDir / kRotateFileName).string();
    bfs::rename(kRotateFilePath, kSubDirFilePath);

    // Search with depth.
    {
        auto filePath = SearchFilePathByDevInodeInDirectory(kTestRootDir, 0, devInode, nullptr);
        EXPECT_FALSE(filePath);

        filePath = SearchFilePathByDevInodeInDirectory(kTestRootDir, 2, devInode, nullptr);
        EXPECT_TRUE(filePath);
        EXPECT_EQ(filePath.value(), kSubDirFilePath);
    }
}

// Fake rotation: a new file B reuses the inode of a deleted file A. Both readers dump a
// checkpoint under the same (dev, inode, config) key. The stale checkpoint of the deleted
// file A must not overwrite the active checkpoint of the still-existing file B.
void CheckpointManagerUnittest::TestAddCheckPointStaleFileNotOverwriteActive() {
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    const std::string configName = "test-config";
    const std::string activeFile = (bfs::path(kTestRootDir) / "active.log").string();
    const std::string deletedFile = (bfs::path(kTestRootDir) / "deleted.log").string();
    std::ofstream(activeFile) << "active";
    // deletedFile is never created on disk -> its checkpoint is stale.

    fsutil::PathStat ps;
    EXPECT_TRUE(fsutil::PathStat::stat(activeFile, ps));
    const DevInode reusedDevInode = ps.GetDevInode();

    // Active reader dumps first, then the stale reader of the deleted file (same inode).
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(activeFile, reusedDevInode, 1000, configName).release());
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(deletedFile, reusedDevInode, 0, configName).release());

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(reusedDevInode, configName, cpt));
    EXPECT_EQ(cpt->mFileName, activeFile);
    EXPECT_EQ(cpt->mOffset, 1000);
    EXPECT_EQ(CheckPointManager::Instance()->GetAllFileCheckPoint().size(), 1UL);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    bfs::remove(activeFile);
}

// Order independence: even when the stale checkpoint is added first, the later active
// checkpoint of the still-existing file must win.
void CheckpointManagerUnittest::TestAddCheckPointActiveOverwriteStale() {
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    const std::string configName = "test-config";
    const std::string activeFile = (bfs::path(kTestRootDir) / "active2.log").string();
    const std::string deletedFile = (bfs::path(kTestRootDir) / "deleted2.log").string();
    std::ofstream(activeFile) << "active";

    fsutil::PathStat ps;
    EXPECT_TRUE(fsutil::PathStat::stat(activeFile, ps));
    const DevInode reusedDevInode = ps.GetDevInode();

    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(deletedFile, reusedDevInode, 0, configName).release());
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(activeFile, reusedDevInode, 2000, configName).release());

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(reusedDevInode, configName, cpt));
    EXPECT_EQ(cpt->mFileName, activeFile);
    EXPECT_EQ(cpt->mOffset, 2000);
    EXPECT_EQ(CheckPointManager::Instance()->GetAllFileCheckPoint().size(), 1UL);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    bfs::remove(activeFile);
}

// Both files exist (e.g. hard links sharing an inode): legacy last-writer-wins is preserved.
void CheckpointManagerUnittest::TestAddCheckPointBothExistLastWriterWins() {
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    const std::string configName = "test-config";
    const std::string file1 = (bfs::path(kTestRootDir) / "link1.log").string();
    const std::string file2 = (bfs::path(kTestRootDir) / "link2.log").string();
    std::ofstream(file1) << "data";
    try {
        bfs::create_hard_link(file1, file2);
    } catch (const bfs::filesystem_error& e) {
        GTEST_SKIP() << "hard link not supported in this environment: " << e.what();
    }

    fsutil::PathStat ps;
    EXPECT_TRUE(fsutil::PathStat::stat(file1, ps));
    const DevInode devInode = ps.GetDevInode();

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(file1, devInode, 100, configName).release());
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(file2, devInode, 200, configName).release());

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(cpt->mFileName, file2);
    EXPECT_EQ(cpt->mOffset, 200);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    bfs::remove(file1);
    bfs::remove(file2);
}

// Neither file exists (both stale): fall back to legacy last-writer-wins, no state dropped.
void CheckpointManagerUnittest::TestAddCheckPointNeitherExistLastWriterWins() {
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    const std::string configName = "test-config";
    const std::string gone1 = (bfs::path(kTestRootDir) / "gone1.log").string();
    const std::string gone2 = (bfs::path(kTestRootDir) / "gone2.log").string();
    const DevInode devInode(12345, 67890); // does not match any real file

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(gone1, devInode, 100, configName).release());
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(gone2, devInode, 200, configName).release());

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(cpt->mFileName, gone2);
    EXPECT_EQ(cpt->mOffset, 200);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
}

} // namespace logtail

UNIT_TEST_MAIN
