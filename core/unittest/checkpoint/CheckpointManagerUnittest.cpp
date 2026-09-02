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

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/DevInode.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "file_server/FileServer.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(checkpoint_find_max_file_count);
DECLARE_FLAG_INT32(check_point_check_interval);
DECLARE_FLAG_INT32(mem_check_point_time_out);

namespace logtail {

std::string kTestRootDir;
const std::string kMatchedConfig = "checkpoint_gc_matched_config";

class CheckpointManagerUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        kTestRootDir = (bfs::path(GetProcessExecutionDir()) / "CheckpointManagerUnittest").string();
        bfs::remove_all(kTestRootDir);
        bfs::create_directories(kTestRootDir);
        AppConfig::GetInstance()->SetLoongcollectorConfDir(kTestRootDir);

        // Register a discovery config matching kTestRootDir/*.log so that the GC's
        // config-match criterion passes for checkpoints created with kMatchedConfig.
        Json::Value inputJson(Json::objectValue);
        inputJson["Type"] = Json::Value("input_file");
        inputJson["FilePaths"] = Json::Value(Json::arrayValue);
        inputJson["FilePaths"].append(Json::Value((bfs::path(kTestRootDir) / "*.log").string()));
        sCtx.SetConfigName(kMatchedConfig);
        ASSERT_TRUE(sDiscoveryOpts.Init(inputJson, sCtx, "test"));
        FileServer::GetInstance()->AddFileDiscoveryConfig(kMatchedConfig, &sDiscoveryOpts, &sCtx);
    }

    static void TearDownTestCase() {
        FileServer::GetInstance()->RemoveFileDiscoveryConfig(kMatchedConfig);
        bfs::remove_all(kTestRootDir);
    }

    void SetUp() override {
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        AppConfig::GetInstance()->mCheckPointFilePath = (bfs::path(kTestRootDir) / "file_check_point").string();
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath);
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath + ".bak");
    }

    void TearDown() override {
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath);
        bfs::remove(AppConfig::GetInstance()->mCheckPointFilePath + ".bak");
    }

    void TestSearchFilePathByDevInodeInDirectory();
    void TestPendingSurvivesDumpRound();
    void TestPendingSurvivesTwoDumpRounds();
    void TestConsumeDeletesPending();
    void TestDumpRoundErasesOverwrittenPending();
    void TestDumpFailureStillEndsRound();
    void TestEndDumpRoundClearsDirCheckpoints();
    void TestDumpPersistsPendingAndActiveThenLoad();
    void TestLoadParseFailureKeepsTable();
    void TestGcEvictsWhenConfigNotMatched();
    void TestGcEvictsWhenFileGone();
    void TestGcFindsRotatedFileAndUpdatesRealPath();
    void TestGcEvictsRotatedFileWhenSignatureChanged();
    void TestGcEvictsWhenResidencyTimeout();
    void TestGcKeepsFreshEntryDespiteOldEventTime();
    void TestGcRespectsCheckInterval();

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

    static FileDiscoveryOptions sDiscoveryOpts;
    static CollectionPipelineContext sCtx;
};

FileDiscoveryOptions CheckpointManagerUnittest::sDiscoveryOpts;
CollectionPipelineContext CheckpointManagerUnittest::sCtx;

UNIT_TEST_CASE(CheckpointManagerUnittest, TestSearchFilePathByDevInodeInDirectory);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestPendingSurvivesDumpRound);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestPendingSurvivesTwoDumpRounds);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestConsumeDeletesPending);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDumpRoundErasesOverwrittenPending);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDumpFailureStillEndsRound);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestEndDumpRoundClearsDirCheckpoints);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestDumpPersistsPendingAndActiveThenLoad);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestLoadParseFailureKeepsTable);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcEvictsWhenConfigNotMatched);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcEvictsWhenFileGone);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcFindsRotatedFileAndUpdatesRealPath);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcEvictsRotatedFileWhenSignatureChanged);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcEvictsWhenResidencyTimeout);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcKeepsFreshEntryDespiteOldEventTime);
UNIT_TEST_CASE(CheckpointManagerUnittest, TestGcRespectsCheckInterval);

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

// A pending handoff entry (written outside any dump round) must survive a periodic
// dump, while the live reader snapshot written inside the round is erased.
void CheckpointManagerUnittest::TestPendingSurvivesDumpRound() {
    const std::string pendingPath = CreateFile("pending_survive.log");
    const std::string activePath = CreateFile("active_survive.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 10, "cfg_pending").release());

    manager->BeginDumpRound();
    manager->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 20, "cfg_active").release());
    EXPECT_TRUE(manager->DumpCheckPointToLocal());
    manager->EndDumpRound();

    CheckPointPtr pendingCpt;
    CheckPointPtr activeCpt;
    EXPECT_TRUE(manager->GetCheckPoint(pendingDev, "cfg_pending", pendingCpt));
    EXPECT_EQ(pendingCpt->mOffset, 10);
    EXPECT_FALSE(manager->GetCheckPoint(activeDev, "cfg_active", activeCpt));
    EXPECT_EQ(manager->GetAllFileCheckPoint().size(), 1UL);
}

// The mid-rebuild race needs the pending entry to survive an arbitrary number of
// periodic dumps, not just the first one.
void CheckpointManagerUnittest::TestPendingSurvivesTwoDumpRounds() {
    const std::string pendingPath = CreateFile("pending_two_rounds.log");
    const std::string activePath = CreateFile("active_two_rounds.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 10, "cfg_pending").release());

    for (int round = 0; round < 2; ++round) {
        manager->BeginDumpRound();
        manager->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 20 + round, "cfg_active").release());
        EXPECT_TRUE(manager->DumpCheckPointToLocal());
        manager->EndDumpRound();
    }

    CheckPointPtr pendingCpt;
    CheckPointPtr activeCpt;
    EXPECT_TRUE(manager->GetCheckPoint(pendingDev, "cfg_pending", pendingCpt));
    EXPECT_EQ(pendingCpt->mOffset, 10);
    EXPECT_FALSE(manager->GetCheckPoint(activeDev, "cfg_active", activeCpt));
}

// InitReader consumes a pending entry with GetCheckPoint + DeleteCheckPoint.
void CheckpointManagerUnittest::TestConsumeDeletesPending() {
    const std::string path = CreateFile("consume.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 64, "cfg_consume").release());

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, "cfg_consume", cpt));
    EXPECT_EQ(cpt->mOffset, 64);
    manager->DeleteCheckPoint(devInode, "cfg_consume");
    EXPECT_FALSE(manager->GetCheckPoint(devInode, "cfg_consume", cpt));
    EXPECT_TRUE(manager->GetAllFileCheckPoint().empty());
}

// If a key is pending but a live reader with the same key writes during the round
// (InitReader with tailExisted skipped consumption), the round erase wins: same
// behavior as the old RemoveAllCheckPoint, and the live reader will be dumped again
// next round.
void CheckpointManagerUnittest::TestDumpRoundErasesOverwrittenPending() {
    const std::string path = CreateFile("overwritten.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 5, "cfg_overwrite").release());

    manager->BeginDumpRound();
    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 50, "cfg_overwrite").release());
    manager->EndDumpRound();

    CheckPointPtr cpt;
    EXPECT_FALSE(manager->GetCheckPoint(devInode, "cfg_overwrite", cpt));
}

// Dump-to-local failure must not change the clearing semantics: the round's
// snapshots are erased (readers still hold the state), pending entries stay.
void CheckpointManagerUnittest::TestDumpFailureStillEndsRound() {
    const std::string pendingPath = CreateFile("dump_fail_pending.log");
    const std::string activePath = CreateFile("dump_fail_active.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 10, "cfg_pending").release());

    const std::string notDir = (bfs::path(kTestRootDir) / "not_a_dir").string();
    std::ofstream(notDir) << "file";
    const std::string oldPath = AppConfig::GetInstance()->mCheckPointFilePath;
    AppConfig::GetInstance()->mCheckPointFilePath = (bfs::path(notDir) / "file_check_point").string();

    manager->BeginDumpRound();
    manager->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 20, "cfg_active").release());
    EXPECT_FALSE(manager->DumpCheckPointToLocal());
    manager->EndDumpRound();

    AppConfig::GetInstance()->mCheckPointFilePath = oldPath;

    CheckPointPtr pendingCpt;
    CheckPointPtr activeCpt;
    EXPECT_TRUE(manager->GetCheckPoint(pendingDev, "cfg_pending", pendingCpt));
    EXPECT_FALSE(manager->GetCheckPoint(activeDev, "cfg_active", activeCpt));
}

// Dir checkpoints are staging-only and must be cleared every round, same as the old
// RemoveAllCheckPoint behavior.
void CheckpointManagerUnittest::TestEndDumpRoundClearsDirCheckpoints() {
    auto* manager = CheckPointManager::Instance();
    const std::string subDir = (bfs::path(kTestRootDir) / "watched_sub").string();
    manager->AddDirCheckPoint(subDir);

    DirCheckPointPtr dirCpt;
    EXPECT_TRUE(manager->GetDirCheckPoint(kTestRootDir, dirCpt));

    manager->BeginDumpRound();
    manager->EndDumpRound();
    EXPECT_FALSE(manager->GetDirCheckPoint(kTestRootDir, dirCpt));
}

// The single table is the single source of truth for serialization: a dump taken
// mid-rebuild persists both live snapshots and pending entries, and a restart can
// load both back.
void CheckpointManagerUnittest::TestDumpPersistsPendingAndActiveThenLoad() {
    const std::string pendingPath = CreateFile("persist_pending.log");
    const std::string activePath = CreateFile("persist_active.log");
    const DevInode pendingDev = GetFileDevInode(pendingPath);
    const DevInode activeDev = GetFileDevInode(activePath);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(pendingPath, pendingDev, 11, "cfg_pending").release());

    manager->BeginDumpRound();
    manager->AddCheckPoint(MakeCheckPoint(activePath, activeDev, 22, "cfg_active").release());
    EXPECT_TRUE(manager->DumpCheckPointToLocal());
    manager->EndDumpRound();

    manager->RemoveAllCheckPoint();
    manager->LoadCheckPoint();

    CheckPointPtr pendingCpt;
    CheckPointPtr activeCpt;
    EXPECT_TRUE(manager->GetCheckPoint(pendingDev, "cfg_pending", pendingCpt));
    EXPECT_EQ(pendingCpt->mOffset, 11);
    EXPECT_TRUE(manager->GetCheckPoint(activeDev, "cfg_active", activeCpt));
    EXPECT_EQ(activeCpt->mOffset, 22);
}

void CheckpointManagerUnittest::TestLoadParseFailureKeepsTable() {
    const std::string path = CreateFile("load_invalid.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 42, "cfg_invalid").release());
    std::ofstream(AppConfig::GetInstance()->mCheckPointFilePath) << "{not-json";
    manager->LoadCheckPoint();

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, "cfg_invalid", cpt));
    EXPECT_EQ(cpt->mOffset, 42);
}

void CheckpointManagerUnittest::TestGcEvictsWhenConfigNotMatched() {
    const std::string path = CreateFile("gc_config_gone.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 1, "cfg_unregistered").release());

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    INT32_FLAG(check_point_check_interval) = -1;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;

    CheckPointPtr cpt;
    EXPECT_FALSE(manager->GetCheckPoint(devInode, "cfg_unregistered", cpt));
}

void CheckpointManagerUnittest::TestGcEvictsWhenFileGone() {
    const std::string path = CreateFile("gc_file_gone.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 1, kMatchedConfig).release());
    bfs::remove(path);

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    INT32_FLAG(check_point_check_interval) = -1;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;

    CheckPointPtr cpt;
    EXPECT_FALSE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
}

void CheckpointManagerUnittest::TestGcFindsRotatedFileAndUpdatesRealPath() {
    const std::string fileName = "gc_rotated.log";
    const std::string path = CreateFile(fileName);
    const std::string rotatedPath = path + ".1";
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    auto checkPoint = MakeCheckPoint(path, devInode, 1, kMatchedConfig);
    checkPoint->mSignatureSize = static_cast<uint32_t>(fileName.size());
    checkPoint->mSignatureHash = static_cast<uint64_t>(HashSignatureString(fileName.data(), fileName.size()));
    manager->AddCheckPoint(checkPoint.release());
    bfs::rename(path, rotatedPath);
    CreateFile(fileName);
    EXPECT_NE(GetFileDevInode(path), devInode);

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    INT32_FLAG(check_point_check_interval) = -1;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
    EXPECT_EQ(cpt->mRealFileName, rotatedPath);
}

void CheckpointManagerUnittest::TestGcEvictsRotatedFileWhenSignatureChanged() {
    const std::string fileName = "gc_rotated_signature.log";
    const std::string path = CreateFile(fileName);
    const std::string rotatedPath = path + ".1";
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    auto checkPoint = MakeCheckPoint(path, devInode, 1, kMatchedConfig);
    checkPoint->mSignatureSize = static_cast<uint32_t>(fileName.size());
    checkPoint->mSignatureHash = static_cast<uint64_t>(HashSignatureString(fileName.data(), fileName.size()));
    manager->AddCheckPoint(checkPoint.release());
    bfs::rename(path, rotatedPath);
    std::ofstream(rotatedPath) << "changed signature";

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    INT32_FLAG(check_point_check_interval) = -1;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;

    CheckPointPtr cpt;
    EXPECT_FALSE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
}

void CheckpointManagerUnittest::TestGcEvictsWhenResidencyTimeout() {
    const std::string path = CreateFile("gc_residency.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 1, kMatchedConfig).release());

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
    cpt->mMemInsertTime = (int32_t)time(NULL) - 100;

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    auto bakTimeout = INT32_FLAG(mem_check_point_time_out);
    INT32_FLAG(check_point_check_interval) = -1;
    INT32_FLAG(mem_check_point_time_out) = 10;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;
    INT32_FLAG(mem_check_point_time_out) = bakTimeout;

    EXPECT_FALSE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
}

// The timeout criterion must use the in-memory residency clock, not the file's last
// event time: an idle file refreshed into the table must not be evicted while its
// rebuild event is still queued.
void CheckpointManagerUnittest::TestGcKeepsFreshEntryDespiteOldEventTime() {
    const std::string path = CreateFile("gc_idle_file.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    auto checkPoint = MakeCheckPoint(path, devInode, 1, kMatchedConfig);
    checkPoint->mLastUpdateTime = 1; // file idle for decades in business time
    manager->AddCheckPoint(checkPoint.release());

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    auto bakTimeout = INT32_FLAG(mem_check_point_time_out);
    INT32_FLAG(check_point_check_interval) = -1;
    INT32_FLAG(mem_check_point_time_out) = 3600;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;
    INT32_FLAG(mem_check_point_time_out) = bakTimeout;

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, kMatchedConfig, cpt));
    EXPECT_EQ(cpt->mOffset, 1);
}

void CheckpointManagerUnittest::TestGcRespectsCheckInterval() {
    const std::string path = CreateFile("gc_gate.log");
    const DevInode devInode = GetFileDevInode(path);
    auto* manager = CheckPointManager::Instance();

    // Entry would be evicted (config never registered), but the gate blocks the run.
    manager->AddCheckPoint(MakeCheckPoint(path, devInode, 1, "cfg_unregistered").release());

    auto bakInterval = INT32_FLAG(check_point_check_interval);
    INT32_FLAG(check_point_check_interval) = INT32_MAX;
    manager->CheckTimeoutCheckPoint();
    INT32_FLAG(check_point_check_interval) = bakInterval;

    CheckPointPtr cpt;
    EXPECT_TRUE(manager->GetCheckPoint(devInode, "cfg_unregistered", cpt));
}

} // namespace logtail

UNIT_TEST_MAIN
