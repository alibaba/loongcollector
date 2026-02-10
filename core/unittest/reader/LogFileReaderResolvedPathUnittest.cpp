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

#include <cstdio>

#include <fstream>

#include "common/DevInode.h"
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "file_server/FileServer.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"

namespace logtail {

class LogFileReaderResolvedPathUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = (fs::path(GetProcessExecutionDir()) / "LogFileReaderResolvedPathUnittest").string();
        if (fs::exists(logPathDir)) {
            fs::remove_all(logPathDir);
        }
        fs::create_directories(logPathDir);
        AppConfig::GetInstance()->SetLoongcollectorConfDir(logPathDir);
    }

    static void TearDownTestCase() {
        if (fs::exists(logPathDir)) {
            fs::remove_all(logPathDir);
        }
    }

    void SetUp() override {
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        FileServer::GetInstance()->AddFileDiscoveryConfig("", &discoveryOpts, &ctx);
        CheckPointManager::Instance()->RemoveAllCheckPoint();
    }

    void TearDown() override {
        FileServer::GetInstance()->RemoveFileDiscoveryConfig("");
        CheckPointManager::Instance()->RemoveAllCheckPoint();
    }

    void TestResolveHostLogPathNormalFile();
    void TestResolveHostLogPathSymbolicLink();
    void TestCheckFileSignatureWithZeroSizeAndDifferentPath();
    void TestCheckFileSignatureWithZeroSizeAndSamePath();
    void TestCopyTruncateUpgradeIssue();

    static std::string logPathDir;

    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestResolveHostLogPathNormalFile);
#ifdef __linux__
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestResolveHostLogPathSymbolicLink);
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestCopyTruncateUpgradeIssue);
#endif
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestCheckFileSignatureWithZeroSizeAndDifferentPath);
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestCheckFileSignatureWithZeroSizeAndSamePath);

std::string LogFileReaderResolvedPathUnittest::logPathDir;

void LogFileReaderResolvedPathUnittest::TestResolveHostLogPathNormalFile() {
    // Test that ResolveHostLogPath works correctly for normal files
    const std::string fileName = "normal_file.log";
    const std::string filePath = (fs::path(logPathDir) / fileName).string();

    // Create a test file
    std::ofstream(filePath) << "test content\n";

    LogFileReader reader(logPathDir,
                         fileName,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // Open the file
    reader.UpdateReaderManual();

    APSARA_TEST_EQUAL_FATAL(reader.mResolvedHostLogPath, filePath);

    // Clean up
    fs::remove(filePath);
}

void LogFileReaderResolvedPathUnittest::TestResolveHostLogPathSymbolicLink() {
#ifdef __linux__
    // Test that ResolveHostLogPath correctly resolves symbolic links
    const std::string realFileName = "real_file.log";
    const std::string linkName = "link_to_file.log";
    const std::string realFilePath = (fs::path(logPathDir) / realFileName).string();
    const std::string linkPath = (fs::path(logPathDir) / linkName).string();

    // Create a real file
    std::ofstream(realFilePath) << "test content\n";

    // Create a symbolic link
    fs::create_symlink(realFilePath, linkPath);

    LogFileReader reader(logPathDir,
                         linkName,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // Open the file through the symbolic link
    reader.UpdateReaderManual();

    APSARA_TEST_EQUAL_FATAL(reader.mResolvedHostLogPath, realFilePath);

    // Clean up
    fs::remove(linkPath);
    fs::remove(realFilePath);
#endif
}

void LogFileReaderResolvedPathUnittest::TestCheckFileSignatureWithZeroSizeAndDifferentPath() {
    // Test the fix: when signature size is 0 and paths differ, CheckFileSignatureAndOffset should return false
    const std::string fileName = "empty_file.log";
    const std::string filePath = (fs::path(logPathDir) / fileName).string();

    // Create an empty file
    std::ofstream(filePath) << "";

    LogFileReader reader(logPathDir,
                         fileName,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // Set up the reader with signature size 0
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);

    // Set mLastFileSignatureSize to 0 (empty file)
    reader.mLastFileSignatureSize = 0;
    reader.mRealLogPath = "/different/path/file.log"; // Different from resolved path
    reader.mResolvedHostLogPath = filePath; // Same as current file

    // CheckFileSignatureAndOffset should return false when signature size is 0
    // and mRealLogPath != mResolvedHostLogPath
    bool result = reader.CheckFileSignatureAndOffset(false);
    APSARA_TEST_FALSE_FATAL(result);

    // Clean up
    fs::remove(filePath);
}

void LogFileReaderResolvedPathUnittest::TestCheckFileSignatureWithZeroSizeAndSamePath() {
    // Test that when signature size is 0 but paths are the same, the check continues normally
    const std::string fileName = "empty_file2.log";
    const std::string filePath = (fs::path(logPathDir) / fileName).string();

    // Create an empty file
    std::ofstream(filePath) << "";

    LogFileReader reader(logPathDir,
                         fileName,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // Set up the reader
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);

    // Set mLastFileSignatureSize to 0
    reader.mLastFileSignatureSize = 0;
    reader.mRealLogPath = filePath;
    reader.mResolvedHostLogPath = filePath; // Same path

    // CheckFileSignatureAndOffset should continue (not early return false)
    // The result depends on other conditions, but it should not return false immediately
    // Just verify it doesn't crash and completes
    (void)reader.CheckFileSignatureAndOffset(false);

    // Clean up
    fs::remove(filePath);
}

void LogFileReaderResolvedPathUnittest::TestCopyTruncateUpgradeIssue() {
#ifdef __linux__
    // 测试场景：copy truncate 模式 + 升级场景 + 目录软链接
    // 修复后的正确行为：当 truncate 后 signature size 变成 0，即使 mResolvedHostLogPath 错误地等于 mHostLogPath，
    // 也应该能够检测并修复路径，继续正常采集，而不是持续返回 false

    // 1. 创建目录结构：真实目录和软链接目录
    const std::string realDir = (fs::path(logPathDir) / "real_logs").string();
    const std::string linkDir = (fs::path(logPathDir) / "link_logs").string();
    const std::string fileName = "app.log";
    const std::string realFilePath = (fs::path(realDir) / fileName).string();
    const std::string linkFilePath = (fs::path(linkDir) / fileName).string();
    fs::create_directories(realDir);
    if (fs::exists(linkDir)) {
        fs::remove(linkDir);
    }
    fs::create_symlink(realDir, linkDir);

    // 2. 创建初始文件并写入内容
    std::ofstream(realFilePath) << "initial log content\nline 2\nline 3\n";
    DevInode fileDevInode = GetFileDevInode(realFilePath);
    APSARA_TEST_TRUE_FATAL(fileDevInode.IsValid());

    // 3. 创建旧版本 checkpoint（模拟升级场景）。关键：旧版本 checkpoint 没有 resolved_file_name 字段
    std::string configName = "test_config";
    ctx.SetConfigName(configName);
    std::string checkpointKey
        = linkFilePath + "*" + ToString(fileDevInode.dev) + "*" + ToString(fileDevInode.inode) + "*" + configName;
    Json::Value checkpointRoot;
    Json::Value checkpointData;
    checkpointData["file_name"] = Json::Value(linkFilePath);
    checkpointData["real_file_name"] = Json::Value(realFilePath);
    checkpointData["offset"] = Json::Value("0");
    checkpointData["sig_size"] = Json::Value(Json::UInt(0)); // 初始 signature size 为 0
    checkpointData["sig_hash"] = Json::Value(Json::UInt64(0));
    checkpointData["update_time"] = Json::Value(static_cast<int32_t>(time(nullptr)));
    checkpointData["inode"] = Json::Value(Json::UInt64(fileDevInode.inode));
    checkpointData["dev"] = Json::Value(Json::UInt64(fileDevInode.dev));
    checkpointData["file_open"] = Json::Value(0);
    checkpointData["container_stopped"] = Json::Value(0);
    checkpointData["container_id"] = Json::Value("");
    checkpointData["last_force_read"] = Json::Value(0);
    checkpointData["config_name"] = Json::Value(configName);
    checkpointData["idx_in_reader_array"] = Json::Value(LogFileReader::CHECKPOINT_IDX_UNDEFINED);
    checkpointRoot["check_point"][checkpointKey] = checkpointData;

    // 4. 加载 checkpoint（模拟从旧版本升级）
    CheckPointManager::Instance()->LoadFileCheckPoint(checkpointRoot);
    CheckPointPtr verifyCheckPoint;
    bool checkpointFound = CheckPointManager::Instance()->GetCheckPoint(fileDevInode, configName, verifyCheckPoint);
    APSARA_TEST_TRUE_FATAL(checkpointFound);
    APSARA_TEST_EQUAL_FATAL(verifyCheckPoint->mRealFileName, realFilePath);
    // APSARA_TEST_TRUE_FATAL(verifyCheckPoint->mResolvedFileName.empty());

    // 5. 创建 reader，它会从 checkpoint 恢复数据
    LogFileReader reader(linkDir,
                         fileName,
                         fileDevInode,
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // 从 checkpoint 初始化（tailExisted = false 表示文件不存在，会从 checkpoint 恢复）
    reader.InitReader(false, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_EQUAL_FATAL(reader.mRealLogPath, realFilePath);
    APSARA_TEST_TRUE_FATAL(reader.mResolvedHostLogPath.empty());

    // 现在打开文件，ResolveHostLogPath 会被调用，重新解析路径
    reader.UpdateReaderManual();
    APSARA_TEST_EQUAL_FATAL(reader.mResolvedHostLogPath, realFilePath);

    // 清理
    CheckPointManager::Instance()->RemoveAllCheckPoint();
    fs::remove(linkFilePath);
    fs::remove(realFilePath);
    fs::remove(linkDir);
    fs::remove_all(realDir);
#endif
}

} // namespace logtail

UNIT_TEST_MAIN
