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
#include <atomic>
#include <string>
#include <thread>
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
        // 清理状态
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();

        // 创建测试根目录
        mTestRootDir = GetProcessExecutionDir();
        if (mTestRootDir.at(mTestRootDir.size() - 1) == PATH_SEPARATOR[0]) {
            mTestRootDir.resize(mTestRootDir.size() - 1);
        }
        mTestRootDir += PATH_SEPARATOR + "ConfigManagerTest";
        bfs::remove_all(mTestRootDir);
        bfs::create_directories(mTestRootDir);
    }

    void TearDown() override {
        // 清理状态
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();
        bfs::remove_all(mTestRootDir);
    }

    // 创建简单的 FileDiscoveryConfig
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
    // 测试目标：复现 ConfigManager.cpp:573 行的悬空引用问题
    // 问题场景：在遍历 checkpoint 的 subdir 时，另一个线程删除了 checkpoint 中的元素
    // 预期行为：即使遍历过程中删除了 checkpoint，函数也应该能安全完成，不应该 crash
    // 当前有 bug 的行为：如果删除发生在遍历过程中，会导致悬空引用，可能 crash

    const string baseDir = mTestRootDir + PS + "base";
    auto* checkpointManager = CheckPointManager::Instance();

    // 1. 创建大量子目录，确保遍历需要一定时间
    vector<string> subdirs;
    const int numSubdirs = 500; // 大量子目录，让遍历时间足够长
    for (int i = 0; i < numSubdirs; ++i) {
        subdirs.push_back(baseDir + PS + "subdir" + ToString(i));
        bfs::create_directories(subdirs.back());
    }

    // 2. 设置 checkpoint：先添加 baseDir，然后添加所有子目录
    //    这样 baseDir 的 checkpoint 的 mSubDir 会包含所有子目录
    checkpointManager->AddDirCheckPoint(baseDir);
    for (const auto& subdir : subdirs) {
        checkpointManager->AddDirCheckPoint(subdir);
    }

    // 验证 checkpoint 已设置
    DirCheckPointPtr baseCheckpoint;
    APSARA_TEST_TRUE_FATAL(checkpointManager->GetDirCheckPoint(baseDir, baseCheckpoint));
    APSARA_TEST_EQUAL_FATAL(baseCheckpoint->mSubDir.size(), numSubdirs);
    LOG_INFO(sLogger, ("Checkpoint created", baseDir)("subdir count", baseCheckpoint->mSubDir.size()));

    // 3. 创建配置
    FileDiscoveryConfig config = CreateTestConfig(baseDir);

    // 4. 第一次注册，建立 inotify watch
    //    这样第二次注册时，RegisterEventHandler 会返回 true，进入 checkpoint 分支
    LOG_INFO(sLogger, ("First registration", baseDir));
    bool firstResult = ConfigManager::GetInstance()->RegisterHandlersWithinDepth(baseDir, config, 0, 10);
    LOG_INFO(sLogger, ("First registration completed", firstResult));

    // 5. 验证 checkpoint 仍然存在
    DirCheckPointPtr verifyCheckpoint;
    bool hasCheckpoint = checkpointManager->GetDirCheckPoint(baseDir, verifyCheckpoint);
    APSARA_TEST_TRUE_FATAL(hasCheckpoint);
    LOG_INFO(sLogger, ("Checkpoint exists", hasCheckpoint)("subdir count", verifyCheckpoint->mSubDir.size()));

    // 6. 创建删除线程：在遍历过程中持续删除子目录的 checkpoint
    //    目标：触发在遍历 mSubDir 时删除元素导致的悬空引用问题
    //    关键：删除线程要足够激进，确保在遍历过程中删除
    std::atomic<bool> traversalStarted(false);
    std::atomic<bool> traversalCompleted(false);
    std::atomic<int> deleteCount(0);

    // 选择要删除的目录：选择大部分目录（除了开头和结尾），最大化触发概率
    vector<int> targetIndices;
    for (int i = numSubdirs / 10; i < numSubdirs * 9 / 10; ++i) {
        targetIndices.push_back(i);
    }

    // 创建多个删除线程，持续删除，确保在遍历过程中删除
    const int numDeleteThreads = 5;
    vector<std::thread> deleteThreads;

    for (int threadId = 0; threadId < numDeleteThreads; ++threadId) {
        deleteThreads.emplace_back([&, threadId]() {
            // 等待遍历开始
            while (!traversalStarted.load()) {
                std::this_thread::yield();
            }

            // 遍历开始后，立即开始删除，持续删除直到遍历完成
            // 每个线程负责删除一部分目录
            int startIdx = (targetIndices.size() * threadId) / numDeleteThreads;
            int endIdx = (targetIndices.size() * (threadId + 1)) / numDeleteThreads;

            while (!traversalCompleted.load()) {
                // 持续删除这个线程负责的目录
                for (int i = startIdx; i < endIdx; ++i) {
                    checkpointManager->DeleteDirCheckPoint(subdirs[targetIndices[i]]);
                    deleteCount++;
                }
                // 不 sleep，立即再次删除，最大化触发概率
            }
        });
    }

    // 7. 第二次调用 RegisterHandlersWithinDepth
    //    此时会进入 checkpoint 分支（568-577行），遍历 mSubDir
    //    删除线程会在遍历过程中删除元素，触发悬空引用问题
    bool result = false;
    bool exceptionThrown = false;
    try {
        LOG_INFO(sLogger, ("Starting second RegisterHandlersWithinDepth", baseDir));

        // 标记遍历开始，让删除线程开始工作
        traversalStarted = true;

        // 开始遍历，删除线程已经在等待
        result = ConfigManager::GetInstance()->RegisterHandlersWithinDepth(baseDir, config, 0, 10);

        LOG_INFO(sLogger, ("Second RegisterHandlersWithinDepth completed", result));
    } catch (const std::exception& e) {
        exceptionThrown = true;
        LOG_ERROR(sLogger, ("Exception caught", e.what()));
    } catch (...) {
        exceptionThrown = true;
        LOG_ERROR(sLogger, ("Unknown exception caught", ""));
    }

    // 标记遍历完成，让删除线程退出
    traversalCompleted = true;

    // 等待所有删除线程完成
    for (auto& t : deleteThreads) {
        t.join();
    }

    LOG_INFO(sLogger, ("Delete count", deleteCount.load()));

    // 验证正确行为：即使遍历过程中删除了 checkpoint，函数也应该能安全完成
    // 如果代码有 bug，这里可能会抛出异常或直接 crash
    APSARA_TEST_FALSE_FATAL(exceptionThrown);

    // 清理配置
    delete config.first;
#endif
}

UNIT_TEST_CASE(ConfigManagerUnittest, TestRegisterHandlersWithinDepthDanglingRef);

} // namespace logtail

UNIT_TEST_MAIN
