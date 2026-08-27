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

#include <fstream>
#include <memory>

#include "common/DevInode.h"
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

    void SetUp() override { CheckPointManager::Instance()->ResetAllCheckPoint(); }

    void TearDown() override { CheckPointManager::Instance()->ResetAllCheckPoint(); }

    void TestSearchFilePathByDevInodeInDirectory();
    void TestAddCheckPointSyncsBackupAfterPrimaryClear();
    void TestDeleteCheckPointRemovesPrimaryAndBackup();
    void TestRemoveAllKeepsUnconsumedBackup();
    void TestBackupMissWhenFileGone();
    void TestLoadOverwritesBackup();
    void TestResetAllCheckPointClearsBackup();

private:
    static std::unique_ptr<CheckPoint> MakeCheckPoint(const std::string& fileName,
                                                      const DevInode& devInode,
                                                      int64_t offset,
                                                      const std::string& configName) {
        return std::unique_ptr<CheckPoint>(new CheckPoint(fileName,
                                                          "" /* resolvedFileName */,
                                                          offset,
                                                          0 /* signatureSize */,
                                                          0 /* signatureHash */,
                                                          devInode,
                                                          configName,
                                                          "" /* realFileName */,
                                                          false /* fileOpenFlag */,
                                                          false /* containerStopped */,
                                                          "" /* containerID */,
                                                          false /* lastForceRead */));
    }

    static std::string CreateFile(const std::string& name) {
        const std::string path = (bfs::path(kTestRootDir) / name).string();
        std::ofstream(path) << name;
        return path;
    }
};

UNIT_TEST_CASE(CheckpointManagerUnittest, TestSearchFilePathByDevInodeInDirectory);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointSyncsBackupAfterPrimaryClear);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDeleteCheckPointRemovesPrimaryAndBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestRemoveAllKeepsUnconsumedBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupMissWhenFileGone);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestLoadOverwritesBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestResetAllCheckPointClearsBackup);

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

void CheckpointManagerUnittest::TestAddCheckPointSyncsBackupAfterPrimaryClear() {
    const std::string path = CreateFile("sync_backup.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_sync";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 128, configName).release());

    EXPECT_EQ(CheckPointManager::Instance()->GetAllFileCheckPoint().size(), 1UL);
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 128);
}

void CheckpointManagerUnittest::TestDeleteCheckPointRemovesPrimaryAndBackup() {
    const std::string path = CreateFile("delete_both.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_delete";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 64, configName).release());
    CheckPointManager::Instance()->DeleteCheckPoint(devInode, configName);

    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 0UL);

    CheckPointPtr cpt;
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
}

void CheckpointManagerUnittest::TestRemoveAllKeepsUnconsumedBackup() {
    const std::string pendingPath = CreateFile("pending.log");
    const std::string activePath = CreateFile("active.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    const std::string pendingCfg = "cfg_pending";
    const std::string activeCfg = "cfg_active";

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 10, pendingCfg).release());
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 20, activeCfg).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    CheckPointPtr pendingCpt;
    CheckPointPtr activeCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(pendingDev, pendingCfg, pendingCpt));
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(activeDev, activeCfg, activeCpt));
    EXPECT_EQ(pendingCpt->mOffset, 10);
    EXPECT_EQ(activeCpt->mOffset, 20);

    CheckPointManager::Instance()->DeleteCheckPoint(pendingDev, pendingCfg);
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(pendingDev, pendingCfg, pendingCpt));
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(activeDev, activeCfg, activeCpt));
}

void CheckpointManagerUnittest::TestBackupMissWhenFileGone() {
    const std::string path = CreateFile("gone.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_gone";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 32, configName).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    bfs::remove(path);

    CheckPointPtr cpt;
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);
}

void CheckpointManagerUnittest::TestLoadOverwritesBackup() {
    const std::string keepPath = CreateFile("keep.log");
    const std::string stalePath = CreateFile("stale.log");
    const DevInode keepDev = GetFileDevInode(keepPath);
    const DevInode staleDev = GetFileDevInode(stalePath);
    const std::string keepCfg = "cfg_keep";
    const std::string staleCfg = "cfg_stale";

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(stalePath, staleDev, 1, staleCfg).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(keepPath, keepDev, 99, keepCfg).release());
    CheckPointManager::Instance()->OverwriteBackupFromPrimaryForTest();

    EXPECT_EQ(CheckPointManager::Instance()->GetAllFileCheckPoint().size(), 1UL);
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    CheckPointPtr keepCpt;
    CheckPointPtr staleCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(keepDev, keepCfg, keepCpt));
    EXPECT_EQ(keepCpt->mOffset, 99);
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(staleDev, staleCfg, staleCpt));
}

void CheckpointManagerUnittest::TestResetAllCheckPointClearsBackup() {
    const std::string path = CreateFile("reset.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_reset";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 7, configName).release());
    CheckPointManager::Instance()->ResetAllCheckPoint();

    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 0UL);

    CheckPointPtr cpt;
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
}

} // namespace logtail

UNIT_TEST_MAIN
