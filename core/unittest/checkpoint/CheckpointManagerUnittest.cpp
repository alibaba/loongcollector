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
    void TestAddCheckPointSameInodeDifferentCreateTime();
    void TestSameNameRebuildNoOverwrite();
    void TestGetDeleteCheckPointWithCreateTime();
    void TestCreateTimeZeroBackwardCompatible();

protected:
    void SetUp() override { CheckPointManager::Instance()->RemoveAllCheckPoint(); }
    void TearDown() override { CheckPointManager::Instance()->RemoveAllCheckPoint(); }

    // Returns an owning pointer so ownership stays explicit at the call site until it is
    // handed to AddCheckPoint (which takes ownership) via release().
    static std::unique_ptr<CheckPoint> MakeCheckPoint(const std::string& fileName,
                                                      const DevInode& devInode,
                                                      const std::string& configName,
                                                      int64_t createTime) {
        return std::make_unique<CheckPoint>(fileName,
                                            "", // resolvedFileName
                                            0, // offset
                                            0, // signatureSize
                                            0, // signatureHash
                                            devInode,
                                            configName,
                                            "", // realFileName
                                            false, // fileOpenFlag
                                            false, // containerStopped
                                            "", // containerID
                                            false, // lastForceRead
                                            createTime);
    }
};

UNIT_TEST_CASE(CheckpointManagerUnittest, TestSearchFilePathByDevInodeInDirectory);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestAddCheckPointSameInodeDifferentCreateTime);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestSameNameRebuildNoOverwrite);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGetDeleteCheckPointWithCreateTime);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestCreateTimeZeroBackwardCompatible);

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

// Two checkpoints with the same config and the same (dev, inode) but different
// createTime must both be kept in memory instead of overwriting each other.
void CheckpointManagerUnittest::TestAddCheckPointSameInodeDifferentCreateTime() {
    auto* mgr = CheckPointManager::Instance();
    const DevInode devInode(1, 100);
    const std::string configName = "config-a";

    mgr->AddCheckPoint(MakeCheckPoint("/var/log/app.log", devInode, configName, 1000).release());
    mgr->AddCheckPoint(MakeCheckPoint("/var/log/app.log", devInode, configName, 2000).release());

    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 2UL);

    CheckPointPtr cp;
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, configName, cp, 1000));
    APSARA_TEST_EQUAL(cp->mCreateTime, 1000);
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, configName, cp, 2000));
    APSARA_TEST_EQUAL(cp->mCreateTime, 2000);
}

// Same-name rebuild scenario (Discussion #2628): a file is deleted and recreated
// with the same name and the OS reuses the same inode. The new physical file has
// a different createTime, so its checkpoint must not clobber the old one.
void CheckpointManagerUnittest::TestSameNameRebuildNoOverwrite() {
    auto* mgr = CheckPointManager::Instance();
    const std::string fileName = "/var/log/rebuilt.log";
    const DevInode devInode(2, 200); // same dev + inode for both generations
    const std::string configName = "config-b";

    // Old generation: offset already advanced.
    auto oldCp = MakeCheckPoint(fileName, devInode, configName, 1111);
    oldCp->mOffset = 4096;
    mgr->AddCheckPoint(oldCp.release());

    // New generation: same name + same inode, different createTime, fresh offset.
    auto newCp = MakeCheckPoint(fileName, devInode, configName, 2222);
    newCp->mOffset = 0;
    mgr->AddCheckPoint(newCp.release());

    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 2UL);

    CheckPointPtr cp;
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, configName, cp, 1111));
    APSARA_TEST_EQUAL(cp->mOffset, 4096);
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, configName, cp, 2222));
    APSARA_TEST_EQUAL(cp->mOffset, 0);
}

// Get/Delete must hit exactly the entry matching (devInode, configName, createTime).
void CheckpointManagerUnittest::TestGetDeleteCheckPointWithCreateTime() {
    auto* mgr = CheckPointManager::Instance();
    const DevInode devInode(3, 300);
    const std::string configName = "config-c";

    mgr->AddCheckPoint(MakeCheckPoint("/var/log/c.log", devInode, configName, 1000).release());
    mgr->AddCheckPoint(MakeCheckPoint("/var/log/c.log", devInode, configName, 2000).release());
    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 2UL);

    // Delete only the createTime=1000 entry.
    mgr->DeleteCheckPoint(devInode, configName, 1000);
    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 1UL);

    CheckPointPtr cp;
    APSARA_TEST_FALSE(mgr->GetCheckPoint(devInode, configName, cp, 1000));
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, configName, cp, 2000));
    APSARA_TEST_EQUAL(cp->mCreateTime, 2000);
}

// With createTime defaulting to 0, the API behaves exactly like the legacy
// (dev, inode, configName) keying: default-createTime Get/Delete still work, and
// two different configs on the same inode remain distinct entries.
void CheckpointManagerUnittest::TestCreateTimeZeroBackwardCompatible() {
    auto* mgr = CheckPointManager::Instance();
    const DevInode devInode(4, 400);

    // Constructed via the legacy 12-arg constructor: createTime defaults to kUnsetCreateTime.
    auto legacyCp = std::make_unique<CheckPoint>(
        "/var/log/legacy.log", "", 0, 0, 0, devInode, "config-d", "", false, false, "", false);
    APSARA_TEST_EQUAL(legacyCp->mCreateTime, kUnsetCreateTime);
    mgr->AddCheckPoint(legacyCp.release());

    // Default-createTime Get/Delete hit the legacy entry.
    CheckPointPtr cp;
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, "config-d", cp));
    APSARA_TEST_EQUAL(cp->mCreateTime, kUnsetCreateTime);

    // Different config on the same inode stays a separate entry (unchanged behavior).
    mgr->AddCheckPoint(MakeCheckPoint("/var/log/legacy.log", devInode, "config-e", 0).release());
    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 2UL);

    mgr->DeleteCheckPoint(devInode, "config-d");
    APSARA_TEST_EQUAL(mgr->GetAllFileCheckPoint().size(), 1UL);
    APSARA_TEST_FALSE(mgr->GetCheckPoint(devInode, "config-d", cp));
    APSARA_TEST_TRUE(mgr->GetCheckPoint(devInode, "config-e", cp));
}

} // namespace logtail

UNIT_TEST_MAIN
