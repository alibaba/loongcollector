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
#include <iterator>
#include <memory>

#include "app_config/AppConfig.h"
#include "common/DevInode.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
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

    void SetUp() override {
        CheckPointManager::Instance()->ResetAllCheckPoint();
        AppConfig::GetInstance()->mCheckPointFilePath = (bfs::path(kTestRootDir) / "file_check_point").string();
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath);
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath + ".bak");
    }

    void TearDown() override {
        CheckPointManager::Instance()->ResetAllCheckPoint();
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath);
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath + ".bak");
    }

    void TestSearchFilePathByDevInodeInDirectory();
    void TestAddCheckPointSyncsBackupAfterPrimaryClear();
    void TestDeleteCheckPointRemovesPrimaryAndBackup();
    void TestRemoveAllKeepsUnconsumedBackup();
    void TestBackupMissWhenFileGone();
    void TestLoadOverwritesBackup();
    void TestResetAllCheckPointClearsBackup();
    void TestLoadCheckPointOverwritesBackup();
    void TestLoadCheckPointParseFailureKeepsBackup();
    void TestDumpKeepsPendingAcrossSecondClear();
    void TestDumpFailureStillClearsPrimaryKeepsBackup();
    void TestBackupHitViaRealFileName();
    void TestBackupHitViaResolvedFileName();
    void TestBackupHitViaInodeSearch();
    void TestBackupMissWhenSignatureChanged();
    void TestPruneRemovesGoneBackup();

private:
    static std::unique_ptr<CheckPoint> MakeCheckPoint(const std::string& fileName,
                                                      const DevInode& devInode,
                                                      int64_t offset,
                                                      const std::string& configName,
                                                      const std::string& realFileName = "",
                                                      const std::string& resolvedFileName = "",
                                                      uint32_t signatureSize = 0,
                                                      uint64_t signatureHash = 0) {
        return std::unique_ptr<CheckPoint>(new CheckPoint(fileName,
                                                          resolvedFileName,
                                                          offset,
                                                          signatureSize,
                                                          signatureHash,
                                                          devInode,
                                                          configName,
                                                          realFileName,
                                                          false /* fileOpenFlag */,
                                                          false /* containerStopped */,
                                                          "" /* containerID */,
                                                          false /* lastForceRead */));
    }

    static std::string CreateFile(const std::string& name, const std::string& content = "") {
        const std::string path = (bfs::path(kTestRootDir) / name).string();
        std::ofstream(path) << (content.empty() ? name : content);
        return path;
    }

    static void FillSignature(const std::string& path, uint32_t& signatureSize, uint64_t& signatureHash) {
        std::ifstream in(path);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        signatureSize = static_cast<uint32_t>(content.size());
        signatureHash = static_cast<uint64_t>(HashSignatureString(content.c_str(), content.size()));
    }

    static void SimulateDumpClear() {
        CheckPointManager::Instance()->PruneInvalidBackupCheckPoints();
        CheckPointManager::Instance()->RemoveAllCheckPoint();
    }
};

UNIT_TEST_CASE(CheckpointManagerUnittest, TestSearchFilePathByDevInodeInDirectory);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointSyncsBackupAfterPrimaryClear);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDeleteCheckPointRemovesPrimaryAndBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestRemoveAllKeepsUnconsumedBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupMissWhenFileGone);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestLoadOverwritesBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestResetAllCheckPointClearsBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestLoadCheckPointOverwritesBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestLoadCheckPointParseFailureKeepsBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDumpKeepsPendingAcrossSecondClear);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDumpFailureStillClearsPrimaryKeepsBackup);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupHitViaRealFileName);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupHitViaResolvedFileName);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupHitViaInodeSearch);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestBackupMissWhenSignatureChanged);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestPruneRemovesGoneBackup);

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
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 0UL);
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

void CheckpointManagerUnittest::TestLoadCheckPointOverwritesBackup() {
    const std::string keepPath = CreateFile("load_keep.log");
    const std::string stalePath = CreateFile("load_stale.log");
    const DevInode keepDev = GetFileDevInode(keepPath);
    const DevInode staleDev = GetFileDevInode(stalePath);
    const std::string keepCfg = "cfg_load_keep";
    const std::string staleCfg = "cfg_load_stale";

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(keepPath, keepDev, 99, keepCfg).release());
    EXPECT_TRUE(CheckPointManager::Instance()->DumpCheckPointToLocal());
    CheckPointManager::Instance()->ResetAllCheckPoint();

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(stalePath, staleDev, 1, staleCfg).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointManager::Instance()->LoadCheckPoint();
    EXPECT_EQ(CheckPointManager::Instance()->GetAllFileCheckPoint().size(), 1UL);
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointManager::Instance()->RemoveAllCheckPoint();
    CheckPointPtr keepCpt;
    CheckPointPtr staleCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(keepDev, keepCfg, keepCpt));
    EXPECT_EQ(keepCpt->mOffset, 99);
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(staleDev, staleCfg, staleCpt));
}

void CheckpointManagerUnittest::TestLoadCheckPointParseFailureKeepsBackup() {
    const std::string path = CreateFile("load_invalid.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_load_invalid";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 42, configName).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    std::ofstream(AppConfig::GetInstance()->mCheckPointFilePath) << "{not-json";
    CheckPointManager::Instance()->LoadCheckPoint();

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 42);
    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
}

void CheckpointManagerUnittest::TestDumpKeepsPendingAcrossSecondClear() {
    const std::string pendingPath = CreateFile("dump_pending.log");
    const std::string activePath = CreateFile("dump_active.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    const std::string pendingCfg = "cfg_dump_pending";
    const std::string activeCfg = "cfg_dump_active";

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 10, pendingCfg).release());
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 20, activeCfg).release());
    EXPECT_TRUE(CheckPointManager::Instance()->DumpCheckPointToLocal());
    SimulateDumpClear();

    CheckPointPtr pendingCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(pendingDev, pendingCfg, pendingCpt));
    EXPECT_EQ(pendingCpt->mOffset, 10);

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 21, activeCfg).release());
    EXPECT_TRUE(CheckPointManager::Instance()->DumpCheckPointToLocal());
    SimulateDumpClear();

    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(pendingDev, pendingCfg, pendingCpt));
    EXPECT_EQ(pendingCpt->mOffset, 10);
    CheckPointPtr activeCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(activeDev, activeCfg, activeCpt));
    EXPECT_EQ(activeCpt->mOffset, 21);
}

void CheckpointManagerUnittest::TestDumpFailureStillClearsPrimaryKeepsBackup() {
    const std::string path = CreateFile("dump_fail.log");
    const DevInode devInode = GetFileDevInode(path);
    const std::string configName = "cfg_dump_fail";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(path, devInode, 55, configName).release());

    const std::string notDir = (bfs::path(kTestRootDir) / "not_a_dir").string();
    std::ofstream(notDir) << "file";
    const std::string oldPath = AppConfig::GetInstance()->mCheckPointFilePath;
    AppConfig::GetInstance()->mCheckPointFilePath = (bfs::path(notDir) / "file_check_point").string();
    EXPECT_FALSE(CheckPointManager::Instance()->DumpCheckPointToLocal());
    AppConfig::GetInstance()->mCheckPointFilePath = oldPath;

    SimulateDumpClear();
    EXPECT_TRUE(CheckPointManager::Instance()->GetAllFileCheckPoint().empty());
    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 55);
}

void CheckpointManagerUnittest::TestBackupHitViaRealFileName() {
    const std::string logicalPath = CreateFile("real_logical.log");
    const DevInode oldDev = GetFileDevInode(logicalPath);
    const std::string rotatedPath = (bfs::path(kTestRootDir) / "real_logical.log.1").string();
    bfs::rename(logicalPath, rotatedPath);
    CreateFile("real_logical.log", "new-inode-content");

    const std::string configName = "cfg_real";
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(logicalPath, oldDev, 88, configName, rotatedPath).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(oldDev, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 88);
}

void CheckpointManagerUnittest::TestBackupHitViaResolvedFileName() {
    const std::string logicalPath = CreateFile("resolved_logical.log");
    const DevInode oldDev = GetFileDevInode(logicalPath);
    const std::string resolvedPath = (bfs::path(kTestRootDir) / "resolved_logical.log.resolved").string();
    bfs::rename(logicalPath, resolvedPath);
    CreateFile("resolved_logical.log", "new-inode-content");

    const std::string configName = "cfg_resolved";
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(logicalPath, oldDev, 77, configName, "", resolvedPath).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(oldDev, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 77);
}

void CheckpointManagerUnittest::TestBackupHitViaInodeSearch() {
    const std::string logicalPath = CreateFile("search_logical.log");
    const DevInode oldDev = GetFileDevInode(logicalPath);
    const std::string rotatedPath = (bfs::path(kTestRootDir) / "search_logical.log.1").string();
    bfs::rename(logicalPath, rotatedPath);
    CreateFile("search_logical.log", "new-inode-content");

    const std::string configName = "cfg_search";
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(logicalPath, oldDev, 66, configName).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    CheckPointPtr cpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(oldDev, configName, cpt));
    EXPECT_EQ(cpt->mOffset, 66);
    EXPECT_EQ(cpt->mRealFileName, rotatedPath);
}

void CheckpointManagerUnittest::TestBackupMissWhenSignatureChanged() {
    const std::string content = "old-signature-body";
    const std::string path = CreateFile("sig_change.log", content);
    const DevInode devInode = GetFileDevInode(path);
    uint32_t signatureSize = 0;
    uint64_t signatureHash = 0;
    FillSignature(path, signatureSize, signatureHash);

    const std::string configName = "cfg_sig";
    CheckPointManager::Instance()->AddCheckPoint(
        MakeCheckPoint(path, devInode, 33, configName, "", "", signatureSize, signatureHash).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();

    std::ofstream(path, std::ios::trunc) << "new-signature-body-xxx";
    CheckPointPtr cpt;
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(devInode, configName, cpt));
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 0UL);
}

void CheckpointManagerUnittest::TestPruneRemovesGoneBackup() {
    const std::string keepPath = CreateFile("prune_keep.log");
    const std::string gonePath = CreateFile("prune_gone.log");
    const DevInode keepDev = GetFileDevInode(keepPath);
    const DevInode goneDev = GetFileDevInode(gonePath);
    const std::string keepCfg = "cfg_prune_keep";
    const std::string goneCfg = "cfg_prune_gone";

    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(keepPath, keepDev, 11, keepCfg).release());
    CheckPointManager::Instance()->AddCheckPoint(MakeCheckPoint(gonePath, goneDev, 12, goneCfg).release());
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    bfs::remove(gonePath);

    CheckPointManager::Instance()->PruneInvalidBackupCheckPoints();
    EXPECT_EQ(CheckPointManager::Instance()->GetBackupFileCheckPointCount(), 1UL);

    CheckPointPtr keepCpt;
    CheckPointPtr goneCpt;
    EXPECT_TRUE(CheckPointManager::Instance()->GetCheckPoint(keepDev, keepCfg, keepCpt));
    EXPECT_FALSE(CheckPointManager::Instance()->GetCheckPoint(goneDev, goneCfg, goneCpt));
}

} // namespace logtail

UNIT_TEST_MAIN
