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
#if defined(__linux__)
#include <unistd.h>
#endif

#include <string>
#include <vector>

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "file_server/ConfigManager.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "logger/Logger.h"

using namespace std;
using namespace logtail;

DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_STRING(user_log_config);

namespace logtail {

class ConfigManagerUnittest : public ::testing::Test {
private:
    const std::string& PS = PATH_SEPARATOR;
    string mTestRootDir;

public:
    void TestRegisterHandlersWithinDepthDanglingRef();

protected:
    void SetUp() override {
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();

        mTestRootDir = GetProcessExecutionDir();
        if (mTestRootDir.at(mTestRootDir.size() - 1) == PATH_SEPARATOR[0]) {
            mTestRootDir.resize(mTestRootDir.size() - 1);
        }
        mTestRootDir += PATH_SEPARATOR + "ConfigManagerTest";
        fs::remove_all(mTestRootDir);
        fs::create_directories(mTestRootDir);
    }

    void TearDown() override {
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();
        fs::remove_all(mTestRootDir);
    }

    FileDiscoveryConfig CreateTestConfig(const string& basePath) {
        static CollectionPipelineContext ctx;

        FileDiscoveryOptions* discoveryOpts = new FileDiscoveryOptions();
        discoveryOpts->mBasePathInfos.push_back(BasePathInfo(basePath, "*.log"));
        discoveryOpts->mPreservedDirDepth = 0;
        discoveryOpts->mMaxDirSearchDepth = 10;

        return make_pair(discoveryOpts, &ctx);
    }
};

void ConfigManagerUnittest::TestRegisterHandlersWithinDepthDanglingRef() {
    LOG_INFO(sLogger, ("TestRegisterHandlersWithinDepthDanglingRef() begin", time(NULL)));

#if defined(__linux__)
    const string baseDir = mTestRootDir + PS + "base";
    auto* checkpointManager = CheckPointManager::Instance();

    // 1. 创建嵌套目录结构：baseDir/subdir0/subdir1/.../subdirN
    const int numSubdirs = 200;
    vector<string> subdirs;
    for (int i = 0; i < numSubdirs; ++i) {
        subdirs.push_back(baseDir + PS + "subdir" + ToString(i));
        for (int j = 0; j < 5; ++j) {
            fs::create_directories(subdirs.back() + PS + "nested" + ToString(j));
        }
    }

    // 2. 设置 checkpoint：先添加 baseDir，然后添加所有子目录
    checkpointManager->AddDirCheckPoint(baseDir);
    for (const auto& subdir : subdirs) {
        checkpointManager->AddDirCheckPoint(subdir);
    }
    DirCheckPointPtr baseCheckpoint;
    APSARA_TEST_TRUE_FATAL(checkpointManager->GetDirCheckPoint(baseDir, baseCheckpoint));
    APSARA_TEST_EQUAL_FATAL(baseCheckpoint->mSubDir.size(), numSubdirs);
    LOG_INFO(sLogger, ("Checkpoint created", baseDir)("subdir count", baseCheckpoint->mSubDir.size()));

    // 3. 创建配置
    FileDiscoveryConfig config = CreateTestConfig(baseDir);

    // 4. 第一次注册，建立 inotify watch，这样第二次注册时，RegisterEventHandler 会返回 true，进入 checkpoint 分支
    LOG_INFO(sLogger, ("First registration", baseDir));
    bool firstResult = ConfigManager::GetInstance()->RegisterHandlersWithinDepth(baseDir, config, 0, 10);
    LOG_INFO(sLogger, ("First registration completed", firstResult));

    // 5. 验证 checkpoint 仍然存在
    DirCheckPointPtr verifyCheckpoint;
    bool hasCheckpoint = checkpointManager->GetDirCheckPoint(baseDir, verifyCheckpoint);
    APSARA_TEST_TRUE_FATAL(hasCheckpoint);
    LOG_INFO(sLogger, ("Checkpoint exists", hasCheckpoint)("subdir count", verifyCheckpoint->mSubDir.size()));

    // 6. 删除并重建大部分子目录，改变它们的 inode
    LOG_INFO(sLogger, ("Deleting and recreating subdirs to change inode", ""));
    // 删除大部分目录（除了开头和结尾），确保在遍历过程中一定会遇到，触发 inode 变化检测和删除
    int startIdx = numSubdirs / 10;
    int endIdx = numSubdirs * 9 / 10;
    for (int i = startIdx; i < endIdx; ++i) {
        fs::remove_all(subdirs[i]);
        fs::create_directories(subdirs[i]);
        for (int j = 0; j < 5; ++j) {
            fs::create_directories(subdirs[i] + PS + "nested" + ToString(j));
        }
    }
    LOG_INFO(sLogger, ("Subdirs recreated with new inode", "")("count", endIdx - startIdx));

    // 7. 第二次调用 RegisterHandlersWithinDepth
    bool result = false;
    bool exceptionThrown = false;

    try {
        LOG_INFO(sLogger, ("Starting second RegisterHandlersWithinDepth", baseDir));
        LOG_INFO(sLogger, ("Will traverse", baseDir)("subdir count", verifyCheckpoint->mSubDir.size()));

        // 关键：遍历过程中，当遇到已重建的目录时，会触发 DeleteDirCheckPoint，这会删除父层 mSubDir 中的元素
        result = ConfigManager::GetInstance()->RegisterHandlersWithinDepth(baseDir, config, 0, 10);
        LOG_INFO(sLogger, ("Second RegisterHandlersWithinDepth completed", result));
    } catch (const std::exception& e) {
        exceptionThrown = true;
        LOG_ERROR(sLogger, ("Exception caught", e.what()));
    } catch (...) {
        exceptionThrown = true;
        LOG_ERROR(sLogger, ("Unknown exception caught", ""));
    }

    APSARA_TEST_FALSE_FATAL(exceptionThrown);

    delete config.first;
#endif
}

UNIT_TEST_CASE(ConfigManagerUnittest, TestRegisterHandlersWithinDepthDanglingRef);

} // namespace logtail

UNIT_TEST_MAIN
