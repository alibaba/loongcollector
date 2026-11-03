// Copyright 2026 iLogtail Authors
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

#include "common/Flags.h"
#include "file_server/FileServer.h"
#include "file_server/event/Event.h"
#include "file_server/polling/PollingDirFile.h"
#include "file_server/polling/PollingEventQueue.h"
#include "unittest/Unittest.h"
#include "unittest/common/DirErrorInjector.h"

DECLARE_FLAG_INT32(delete_dir_file_round);
DECLARE_FLAG_INT32(polling_max_stat_count);
DECLARE_FLAG_INT32(polling_max_stat_count_per_dir);
DECLARE_FLAG_INT32(polling_check_timeout_interval);

namespace logtail {

class PollingScanErrorUnittest : public testing::Test {
protected:
    void SetUp() override {
        mRoot = std::filesystem::path(GetProcessExecutionDir()) / "p1_polling";
        std::filesystem::create_directories(mRoot / "child");
        std::ofstream(mRoot / "a.log") << "a";
        std::ofstream(mRoot / "b.log") << "b";
        mCtx.SetConfigName("p1_polling");
        Json::Value config;
        config["Type"] = "input_file";
        config["FilePaths"].append((mRoot / "**" / "*.log").string());
        config["MaxDirSearchDepth"] = 2;
        ASSERT_TRUE(mOptions.Init(config, mCtx, "test"));
        FileServer::GetInstance()->AddFileDiscoveryConfig("p1_polling", &mOptions, &mCtx);
        mPolling.mRuningFlag = true;
        mPolling.mHoldOnFlag = false;
        mOldDeleteRounds = INT32_FLAG(delete_dir_file_round);
        INT32_FLAG(delete_dir_file_round) = 0;
        mOldGlobalLimit = INT32_FLAG(polling_max_stat_count);
        mOldDirLimit = INT32_FLAG(polling_max_stat_count_per_dir);
    }

    void TearDown() override {
        INT32_FLAG(delete_dir_file_round) = mOldDeleteRounds;
        INT32_FLAG(polling_max_stat_count) = mOldGlobalLimit;
        INT32_FLAG(polling_max_stat_count_per_dir) = mOldDirLimit;
        FileServer::GetInstance()->RemoveFileDiscoveryConfig("p1_polling");
        std::vector<Event*> events;
        PollingEventQueue::GetInstance()->PopAllEvents(events);
        for (auto* event : events) {
            delete event;
        }
        std::filesystem::remove_all(mRoot);
    }

    void nextRound() {
        ++mPolling.mCurrentRound;
        mPolling.mStatCount = 0;
        mPolling.mNewFileVec.clear();
        mPolling.mIncompleteDirs.clear();
        mPolling.mScanInterrupted = false;
    }

    void scanNormal() {
        fsutil::PathStat stat;
        ASSERT_TRUE(fsutil::PathStat::stat(mRoot.string(), stat));
        mPolling.PollingNormalConfigPath({&mOptions, &mCtx}, mRoot.string(), "", stat, 0);
        mPolling.ProtectUnscannedCache();
        mPolling.ClearUnavailableFileAndDir();
    }

    std::filesystem::path mRoot;
    CollectionPipelineContext mCtx;
    FileDiscoveryOptions mOptions;
    PollingDirFile mPolling;
    int32_t mOldDeleteRounds = 0;
    int32_t mOldGlobalLimit = 0;
    int32_t mOldDirLimit = 0;
};

TEST_F(PollingScanErrorUnittest, FailedSubtreePausesMissingAgeAndRecoveryStillRemovesMissingFiles) {
    const auto missingDir = (mRoot / "missing").string();
    const auto missingFile = (mRoot / "missing.log").string();
    const auto unrelated = mRoot.string() + "_sibling/missing.log";
    mPolling.mDirCacheMap[missingDir].SetCheckRound(0);
    mPolling.mFileCacheMap[missingFile].SetCheckRound(0);
    for (const auto& operation : {"open", "read", "close"}) {
        nextRound();
        mPolling.mFileCacheMap[unrelated].SetCheckRound(0);
        ScopedDirErrorInjector fault(mRoot.string(), operation, EIO, 1);
        scanNormal();
        ASSERT_EQ(1, mPolling.mDirCacheMap.count(missingDir)) << operation;
        ASSERT_EQ(1, mPolling.mFileCacheMap.count(missingFile)) << operation;
        EXPECT_EQ(mPolling.mCurrentRound, mPolling.mFileCacheMap.at(missingFile).GetLastCheckRound());
        EXPECT_EQ(0, mPolling.mFileCacheMap.count(unrelated));
    }
    nextRound();
    scanNormal();
    EXPECT_EQ(0, mPolling.mDirCacheMap.count(missingDir));
    EXPECT_EQ(0, mPolling.mFileCacheMap.count(missingFile));
    EXPECT_EQ(1, mPolling.mFileCacheMap.count((mRoot / "a.log").string()));
}

TEST_F(PollingScanErrorUnittest, WildcardScanFailuresProtectUnvisitedChildren) {
    BasePathInfo pathInfo;
    pathInfo.basePath = (mRoot / "*").string();
    pathInfo.wildcardPaths = {mRoot.string(), pathInfo.basePath};
    pathInfo.constWildcardPaths = {"", ""};
    const auto missing = (mRoot / "missing" / "a.log").string();
    mPolling.mFileCacheMap[missing].SetCheckRound(0);
    for (const auto& operation : {"open", "read", "close"}) {
        nextRound();
        ScopedDirErrorInjector fault(mRoot.string(), operation, EIO, 1);
        mPolling.PollingWildcardConfigPath({&mOptions, &mCtx}, pathInfo, mRoot.string(), 0);
        mPolling.ProtectUnscannedCache();
        mPolling.ClearUnavailableFileAndDir();
        ASSERT_EQ(1, mPolling.mFileCacheMap.count(missing)) << operation;
    }
    nextRound();
    mPolling.PollingWildcardConfigPath({&mOptions, &mCtx}, pathInfo, mRoot.string(), 0);
    mPolling.ClearUnavailableFileAndDir();
    EXPECT_EQ(0, mPolling.mFileCacheMap.count(missing));
}

TEST_F(PollingScanErrorUnittest, ChildFailureDoesNotProtectConfirmedMissingSiblings) {
    const auto child = mRoot / "child";
    std::ofstream(child / "present.log") << "present";
    const auto missingChild = (child / "missing.log").string();
    const auto missingSibling = (mRoot / "missing.log").string();
    mPolling.mFileCacheMap[missingChild].SetCheckRound(0);
    for (const auto& operation : {"open", "read", "close"}) {
        nextRound();
        mPolling.mFileCacheMap[missingSibling].SetCheckRound(0);
        ScopedDirErrorInjector fault(child.string(), operation, EIO, 1);
        scanNormal();
        ASSERT_EQ(1, mPolling.mFileCacheMap.count(missingChild)) << operation;
        EXPECT_EQ(0, mPolling.mFileCacheMap.count(missingSibling));
    }
    nextRound();
    scanNormal();
    EXPECT_EQ(0, mPolling.mFileCacheMap.count(missingChild));
}

TEST_F(PollingScanErrorUnittest, BudgetAndCancellationCannotConfirmAbsence) {
    const auto missing = (mRoot / "missing.log").string();
    const auto otherRoot = mRoot.string() + "_other/missing.log";
    for (int mode = 0; mode < 3; ++mode) {
        nextRound();
        mPolling.mFileCacheMap[missing].SetCheckRound(0);
        mPolling.mFileCacheMap[otherRoot].SetCheckRound(0);
        INT32_FLAG(polling_max_stat_count) = mode == 0 ? 0 : mOldGlobalLimit;
        INT32_FLAG(polling_max_stat_count_per_dir) = mode == 1 ? 0 : mOldDirLimit;
        mPolling.mHoldOnFlag = mode == 2;
        scanNormal();
        EXPECT_EQ(1, mPolling.mFileCacheMap.count(missing)) << mode;
        EXPECT_EQ(mode == 1 ? 0 : 1, mPolling.mFileCacheMap.count(otherRoot)) << mode;
    }
}

TEST_F(PollingScanErrorUnittest, IncompleteScanDoesNotDisableConfiguredInactivityExpiry) {
    const auto expiredDir = (mRoot / "expired").string();
    const auto expiredFile = (mRoot / "expired.log").string();
    auto& dir = mPolling.mDirCacheMap[expiredDir];
    dir.SetExceedPreservedDirDepth(true);
    dir.SetLastModifyTime(0);
    auto& file = mPolling.mFileCacheMap[expiredFile];
    file.SetExceedPreservedDirDepth(true);
    file.SetLastModifyTime(0);
    mPolling.MarkScanIncomplete(mRoot.string());
    const auto oldInterval = INT32_FLAG(polling_check_timeout_interval);
    INT32_FLAG(polling_check_timeout_interval) = 0;
    mPolling.ClearTimeoutFileAndDir();
    INT32_FLAG(polling_check_timeout_interval) = oldInterval;
    EXPECT_EQ(0, mPolling.mDirCacheMap.count(expiredDir));
    EXPECT_EQ(0, mPolling.mFileCacheMap.count(expiredFile));
}

} // namespace logtail

UNIT_TEST_MAIN
