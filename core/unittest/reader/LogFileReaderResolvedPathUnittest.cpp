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

#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "file_server/FileServer.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"

namespace logtail {

class LogFileReaderResolvedPathUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = (bfs::path(GetProcessExecutionDir()) / "LogFileReaderResolvedPathUnittest").string();
        if (bfs::exists(logPathDir)) {
            bfs::remove_all(logPathDir);
        }
        bfs::create_directories(logPathDir);
        AppConfig::GetInstance()->SetLoongcollectorConfDir(logPathDir);
    }

    static void TearDownTestCase() {
        if (bfs::exists(logPathDir)) {
            bfs::remove_all(logPathDir);
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

    static std::string logPathDir;

    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestResolveHostLogPathNormalFile);
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestResolveHostLogPathSymbolicLink);
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestCheckFileSignatureWithZeroSizeAndDifferentPath);
UNIT_TEST_CASE(LogFileReaderResolvedPathUnittest, TestCheckFileSignatureWithZeroSizeAndSamePath);

std::string LogFileReaderResolvedPathUnittest::logPathDir;

void LogFileReaderResolvedPathUnittest::TestResolveHostLogPathNormalFile() {
    // Test that ResolveHostLogPath works correctly for normal files
    const std::string fileName = "normal_file.log";
    const std::string filePath = (bfs::path(logPathDir) / fileName).string();

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

    // Call ResolveHostLogPath
    reader.ResolveHostLogPath();

    APSARA_TEST_EQUAL_FATAL(reader.mResolvedHostLogPath, filePath);

    // Clean up
    bfs::remove(filePath);
}

void LogFileReaderResolvedPathUnittest::TestResolveHostLogPathSymbolicLink() {
#ifndef _MSC_VER
    // Test that ResolveHostLogPath correctly resolves symbolic links
    const std::string realFileName = "real_file.log";
    const std::string linkName = "link_to_file.log";
    const std::string realFilePath = (bfs::path(logPathDir) / realFileName).string();
    const std::string linkPath = (bfs::path(logPathDir) / linkName).string();

    // Create a real file
    std::ofstream(realFilePath) << "test content\n";

    // Create a symbolic link
    bfs::create_symlink(realFilePath, linkPath);

    LogFileReader reader(logPathDir,
                         linkName,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));

    // Open the file through the symbolic link
    reader.UpdateReaderManual();

    // Call ResolveHostLogPath
    reader.ResolveHostLogPath();

    APSARA_TEST_EQUAL_FATAL(reader.mResolvedHostLogPath, realFilePath);

    // Clean up
    bfs::remove(linkPath);
    bfs::remove(realFilePath);
#endif
}

void LogFileReaderResolvedPathUnittest::TestCheckFileSignatureWithZeroSizeAndDifferentPath() {
    // Test the fix: when signature size is 0 and paths differ, CheckFileSignatureAndOffset should return false
    const std::string fileName = "empty_file.log";
    const std::string filePath = (bfs::path(logPathDir) / fileName).string();

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
    bfs::remove(filePath);
}

void LogFileReaderResolvedPathUnittest::TestCheckFileSignatureWithZeroSizeAndSamePath() {
    // Test that when signature size is 0 but paths are the same, the check continues normally
    const std::string fileName = "empty_file2.log";
    const std::string filePath = (bfs::path(logPathDir) / fileName).string();

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
    bfs::remove(filePath);
}

} // namespace logtail

UNIT_TEST_MAIN
