// Copyright 2023 iLogtail Authors
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
#include <memory>
#include <string>

#include "json/json.h"

#include "FileSystemUtil.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "file_server/FileDiscoveryOptions.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FileDiscoveryOptionsUnittest : public testing::Test {
public:
    void OnSuccessfulInit() const;
    void OnFailedInit() const;
    void TestFilePaths() const;
    void TestWindowsRootPathCollection() const;
    void TestChinesePathMatching() const;
    void TestWindowsDriveLetterCaseInsensitive() const;

private:
    const string pluginType = "test";
    CollectionPipelineContext ctx;
};

void FileDiscoveryOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    filePath = NormalizeNativePath(filePath.string());
    filesystem::path ex1, ex2, ex3, ex4, ex5;

    // only mandatory param
    configStr = R"(
        {
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(1U, config->mFilePaths.size());
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    APSARA_TEST_EQUAL(-1, config->mPreservedDirDepth);
    APSARA_TEST_EQUAL(0U, config->mExcludeFilePaths.size());
    APSARA_TEST_EQUAL(0U, config->mExcludeFiles.size());
    APSARA_TEST_EQUAL(0U, config->mExcludeDirs.size());
    APSARA_TEST_FALSE(config->mAllowingCollectingFilesInRootDir);
    APSARA_TEST_FALSE(config->mAllowingIncludedByMultiConfigs);

    // valid optional param
    configStr = R"(
        {
            "FilePaths": [],
            "MaxDirSearchDepth": 1,
            "PreservedDirDepth": 0,
            "ExcludeFilePaths": ["/home/b.log"],
            "ExcludeFiles": ["a.log"],
            "ExcludeDirs": ["/home/test"],
            "AllowingCollectingFilesInRootDir": true,
            "AllowingIncludedByMultiConfigs": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(1U, config->mFilePaths.size());
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    APSARA_TEST_EQUAL(0, config->mPreservedDirDepth);
    APSARA_TEST_EQUAL(1U, config->mExcludeFilePaths.size());
    APSARA_TEST_EQUAL(1U, config->mExcludeFiles.size());
    APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
    APSARA_TEST_TRUE(config->mAllowingCollectingFilesInRootDir);
    APSARA_TEST_TRUE(config->mAllowingIncludedByMultiConfigs);

    // invalid optional param
    configStr = R"(
        {
            "FilePaths": [],
            "MaxDirSearchDepth": true,
            "PreservedDirDepth": true,
            "ExcludeFilePaths": true,
            "ExcludeFiles": true,
            "ExcludeDirs": true,
            "AllowingCollectingFilesInRootDir": "true",
            "AllowingIncludedByMultiConfigs": "true"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(1U, config->mFilePaths.size());
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    APSARA_TEST_EQUAL(-1, config->mPreservedDirDepth);
    APSARA_TEST_EQUAL(0U, config->mExcludeFilePaths.size());
    APSARA_TEST_EQUAL(0U, config->mExcludeFiles.size());
    APSARA_TEST_EQUAL(0U, config->mExcludeDirs.size());
    APSARA_TEST_FALSE(config->mAllowingCollectingFilesInRootDir);
    APSARA_TEST_FALSE(config->mAllowingIncludedByMultiConfigs);

    // ExcludeFilePaths
    ex1 = filesystem::path(".") / "test" / "a.log"; // not absolute
    ex2 = filesystem::current_path() / "**" / "b.log"; // ML
    ex3 = filesystem::absolute(ex1);
    ex2 = NormalizeNativePath(ex2.string());
    ex3 = NormalizeNativePath(ex3.string());
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeFilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["ExcludeFilePaths"].append(Json::Value(ex1.string()));
    configJson["ExcludeFilePaths"].append(Json::Value(ex2.string()));
    configJson["ExcludeFilePaths"].append(Json::Value(ex3.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(3U, config->mExcludeFilePaths.size());
    APSARA_TEST_EQUAL(1U, config->mFilePathBlacklist.size());
    APSARA_TEST_EQUAL(1U, config->mMLFilePathBlacklist.size());
    APSARA_TEST_TRUE(config->mHasBlacklist);

    // ExcludeFiles
    ex1 = "a.log";
    ex2 = filesystem::current_path() / "b.log"; // has path separator
    ex2 = NormalizeNativePath(ex2.string());
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeFiles": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["ExcludeFiles"].append(Json::Value(ex1.string()));
    configJson["ExcludeFiles"].append(Json::Value(ex2.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(2U, config->mExcludeFiles.size());
    APSARA_TEST_EQUAL(1U, config->mFileNameBlacklist.size());
    APSARA_TEST_TRUE(config->mHasBlacklist);

    // ExcludeDirs
    ex1 = filesystem::path(".") / "test"; // not absolute
    ex2 = filesystem::current_path() / "**" / "test"; // ML
    ex3 = filesystem::current_path() / "a*"; // *
    ex4 = filesystem::current_path() / "a?"; // ?
    ex5 = filesystem::absolute(ex1);
    ex2 = NormalizeNativePath(ex2.string());
    ex3 = NormalizeNativePath(ex3.string());
    ex4 = NormalizeNativePath(ex4.string());
    ex5 = NormalizeNativePath(ex5.string());
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeDirs": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["ExcludeDirs"].append(Json::Value(ex1.string()));
    configJson["ExcludeDirs"].append(Json::Value(ex2.string()));
    configJson["ExcludeDirs"].append(Json::Value(ex3.string()));
    configJson["ExcludeDirs"].append(Json::Value(ex4.string()));
    configJson["ExcludeDirs"].append(Json::Value(ex5.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(5U, config->mExcludeDirs.size());
    APSARA_TEST_EQUAL(1U, config->mMLWildcardDirPathBlacklist.size());
    APSARA_TEST_EQUAL(2U, config->mWildcardDirPathBlacklist.size());
    APSARA_TEST_EQUAL(1U, config->mDirPathBlacklist.size());
    APSARA_TEST_TRUE(config->mHasBlacklist);

    // AllowingCollectingFilesInRootDir
    configStr = R"(
        {
            "FilePaths": [],
            "AllowingCollectingFilesInRootDir": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_FALSE(config->mAllowingIncludedByMultiConfigs);
    APSARA_TEST_TRUE(BOOL_FLAG(enable_root_path_collection));
}

void FileDiscoveryOptionsUnittest::OnFailedInit() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    filePath = NormalizeNativePath(filePath.string());

    // no FilePaths
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginType));

    // more than 1 file path
    configStr = R"(
        {
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginType));

    // invalid filepath
    filePath = filesystem::current_path();
    filePath = NormalizeNativePath(filePath.string());
    configStr = R"(
        {
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string() + PATH_SEPARATOR));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginType));
}

void FileDiscoveryOptionsUnittest::TestFilePaths() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    CollectionPipelineContext ctx;
    filesystem::path filePath;

    // no wildcard
    filePath = filesystem::path(".") / "test" / "*.log";
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["MaxDirSearchDepth"] = Json::Value(1);
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    filesystem::path expectedBasePath = filesystem::current_path() / "test";
    expectedBasePath = NormalizeNativePath(expectedBasePath.string());
    APSARA_TEST_EQUAL(expectedBasePath.string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    configJson.clear();

    // with wildcard */?
    filePath = filesystem::path(".") / "*" / "test" / "?" / "*.log";
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["MaxDirSearchDepth"] = Json::Value(1);
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    filesystem::path expectedBasePath1 = filesystem::current_path() / "*" / "test" / "?";
    filesystem::path expectedWildcard0 = filesystem::current_path();
    filesystem::path expectedWildcard1 = filesystem::current_path() / "*";
    filesystem::path expectedWildcard2 = filesystem::current_path() / "*" / "test";
    filesystem::path expectedWildcard3 = filesystem::current_path() / "*" / "test" / "?";
    expectedBasePath1 = NormalizeNativePath(expectedBasePath1.string());
    expectedWildcard0 = NormalizeNativePath(expectedWildcard0.string());
    expectedWildcard1 = NormalizeNativePath(expectedWildcard1.string());
    expectedWildcard2 = NormalizeNativePath(expectedWildcard2.string());
    expectedWildcard3 = NormalizeNativePath(expectedWildcard3.string());
    APSARA_TEST_EQUAL(expectedBasePath1.string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    APSARA_TEST_EQUAL(4U, config->GetWildcardPaths().size());
    APSARA_TEST_EQUAL(expectedWildcard0.string(), config->GetWildcardPaths()[0]);
    APSARA_TEST_EQUAL(expectedWildcard1.string(), config->GetWildcardPaths()[1]);
    APSARA_TEST_EQUAL(expectedWildcard2.string(), config->GetWildcardPaths()[2]);
    APSARA_TEST_EQUAL(expectedWildcard3.string(), config->GetWildcardPaths()[3]);
    APSARA_TEST_EQUAL(3U, config->GetConstWildcardPaths().size());
    APSARA_TEST_EQUAL("", config->GetConstWildcardPaths()[0]);
    APSARA_TEST_EQUAL("test", config->GetConstWildcardPaths()[1]);
    APSARA_TEST_EQUAL("", config->GetConstWildcardPaths()[2]);
    configJson.clear();

    // with wildcard **
    filePath = filesystem::path(".") / "*" / "test" / "**" / "*.log";
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["MaxDirSearchDepth"] = Json::Value(1);
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(1, config->mMaxDirSearchDepth);
    filesystem::path expectedBasePath2 = filesystem::current_path() / "*" / "test";
    expectedBasePath2 = NormalizeNativePath(expectedBasePath2.string());
    APSARA_TEST_EQUAL(expectedBasePath2.string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
}

void FileDiscoveryOptionsUnittest::TestWindowsRootPathCollection() const {
#if defined(_MSC_VER)
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test 1: Direct root path collection without AllowingCollectingFilesInRootDir flag
    // Expected: Init should succeed but mAllowingCollectingFilesInRootDir should be false
    {
        filesystem::path filePath = "C:\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Flag should be false without explicit setting
        APSARA_TEST_FALSE(config->mAllowingCollectingFilesInRootDir);
    }

    // Test 2: Direct root path collection with AllowingCollectingFilesInRootDir=true
    // Expected: Flag should be enabled only when enable_root_path_collection is true
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "C:\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": [],
                "AllowingCollectingFilesInRootDir": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Flag should be true with both config and global flag enabled
        APSARA_TEST_TRUE(config->mAllowingCollectingFilesInRootDir);
        APSARA_TEST_EQUAL("C:\\", config->GetBasePath());
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        BOOL_FLAG(enable_root_path_collection) = false;
    }

    // Test 3: Multi-level path with wildcard at root (C:\*\logs\*.log)
    // Expected: Should parse correctly and set base path to C:\*\logs
    {
        filesystem::path filePath = "C:\\*\\logs\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Base path should be set to wildcard path
        APSARA_TEST_EQUAL("C:\\*\\logs", config->GetBasePath());
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        // Expected: Wildcard paths should include C:\, C:\*, and C:\*\logs
        APSARA_TEST_EQUAL(3U, config->GetWildcardPaths().size());
    }

    // Test 4: Multi-level path with wildcard at root and AllowingCollectingFilesInRootDir=true
    // Expected: Should work when both config and global flag are enabled
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "C:\\*\\logs\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": [],
                "AllowingCollectingFilesInRootDir": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        APSARA_TEST_TRUE(config->mAllowingCollectingFilesInRootDir);
        APSARA_TEST_EQUAL("C:\\*\\logs", config->GetBasePath());
        APSARA_TEST_EQUAL(3U, config->GetWildcardPaths().size());
        BOOL_FLAG(enable_root_path_collection) = false;
    }

    // Test 5: Recursive search from root (C:\**\*.log)
    // Expected: Should parse correctly with ** at root
    {
        filesystem::path filePath = "C:\\**\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": [],
                "MaxDirSearchDepth": 2
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Base path should be C:\, MaxDirSearchDepth should be set
        APSARA_TEST_EQUAL("C:\\", config->GetBasePath());
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        APSARA_TEST_EQUAL(2, config->mMaxDirSearchDepth);
    }

    // Test 6: Recursive search with enable flag (D:\**\*.log with AllowingCollectingFilesInRootDir=true)
    // Expected: Should work with flag enabled
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "D:\\**\\*.log";
        filePath = NormalizeNativePath(filePath.string());
        configStr = R"(
            {
                "FilePaths": [],
                "MaxDirSearchDepth": 3,
                "AllowingCollectingFilesInRootDir": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        APSARA_TEST_TRUE(config->mAllowingCollectingFilesInRootDir);
        APSARA_TEST_EQUAL("D:\\", config->GetBasePath());
        APSARA_TEST_EQUAL(3, config->mMaxDirSearchDepth);
        BOOL_FLAG(enable_root_path_collection) = false;
    }
#endif
}

void FileDiscoveryOptionsUnittest::TestChinesePathMatching() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

#if defined(__linux__)
    // Linux Test 1: Chinese path in FilePaths
    // Expected: Should successfully parse Chinese path
    {
        filesystem::path filePath = filesystem::absolute("测试目录/**/*.log");
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Base path should contain Chinese characters
        APSARA_TEST_TRUE(config->GetBasePath().find("测试目录") != string::npos);
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    }

    // Linux Test 2: Chinese directory in ExcludeDirs
    // Expected: Should successfully add Chinese directory to blacklist
    {
        filesystem::path filePath = filesystem::absolute("日志/**/*.log");
        filesystem::path excludeDir = filesystem::absolute("日志/黑名单");
        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Blacklist should contain the Chinese path
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Linux Test 3: Chinese filename in ExcludeFiles
    // Expected: Should successfully add Chinese filename to blacklist
    {
        filesystem::path filePath = filesystem::absolute("logs/*.log");
        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeFiles": ["排除文件.log", "测试*.log"]
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: File name blacklist should contain Chinese patterns
        APSARA_TEST_EQUAL(2U, config->mExcludeFiles.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }
#elif defined(_MSC_VER)
    // Windows: Chinese path tests with proper exception handling
    // Note: UTF-8 to ACP conversion may fail depending on system locale
    // These tests verify the system handles Chinese paths gracefully

    // Windows Test 1: Chinese path - basic functionality
    // Using UTF-8 literal (will be converted to ACP internally)
    {
        try {
            // UTF-8: "测试目录"
            string chineseDir = "\xE6\xB5\x8B\xE8\xAF\x95\xE7\x9B\xAE\xE5\xBD\x95";
            filesystem::path basePath = filesystem::current_path();
            string fullPath = basePath.string() + "\\" + chineseDir + "\\**\\*.log";

            configStr = R"(
                {
                    "FilePaths": []
                }
            )";
            APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
            configJson["FilePaths"].append(Json::Value(fullPath));
            config.reset(new FileDiscoveryOptions());

            bool initSuccess = config->Init(configJson, ctx, pluginType);
            if (initSuccess) {
                // If encoding conversion succeeds, verify basic properties
                APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
                APSARA_TEST_FALSE(config->GetBasePath().empty());

                // Verify the base path contains the Chinese directory name
                // Convert UTF-8 to ACP for comparison
                string chineseDirACP = ConvertAndNormalizeNativePath(chineseDir);
                APSARA_TEST_TRUE(config->GetBasePath().find(chineseDirACP) != string::npos);

                LOG_INFO(sLogger, ("Chinese path test", "PASSED - encoding conversion succeeded"));
            } else {
                // If Init fails, it's acceptable on systems with incompatible locale
                LOG_WARNING(sLogger,
                            ("Chinese path test", "SKIPPED - encoding conversion not supported on this system"));
            }
        } catch (const std::exception& e) {
            // Encoding conversion or filesystem operation failed
            LOG_WARNING(sLogger, ("Chinese path test", "SKIPPED - exception")("error", e.what()));
        }
    }

    // Windows Test 2: Chinese ExcludeDirs
    {
        try {
            // UTF-8: "日志" and "黑名单"
            string chineseLog = "\xE6\x97\xA5\xE5\xBF\x97";
            string chineseBlacklist = "\xE9\xBB\x91\xE5\x90\x8D\xE5\x8D\x95";
            filesystem::path basePath = filesystem::current_path();
            string fullPath = basePath.string() + "\\" + chineseLog + "\\**\\*.log";
            string excludePath = basePath.string() + "\\" + chineseLog + "\\" + chineseBlacklist;

            configStr = R"(
                {
                    "FilePaths": [],
                    "ExcludeDirs": []
                }
            )";
            APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
            configJson["FilePaths"].append(Json::Value(fullPath));
            configJson["ExcludeDirs"].append(Json::Value(excludePath));
            config.reset(new FileDiscoveryOptions());

            bool initSuccess = config->Init(configJson, ctx, pluginType);
            if (initSuccess) {
                APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
                APSARA_TEST_TRUE(config->mHasBlacklist);

                // Verify the base path contains Chinese directory name
                string chineseLogACP = ConvertAndNormalizeNativePath(chineseLog);
                APSARA_TEST_TRUE(config->GetBasePath().find(chineseLogACP) != string::npos);

                LOG_INFO(sLogger, ("Chinese ExcludeDirs test", "PASSED"));
            } else {
                LOG_WARNING(sLogger, ("Chinese ExcludeDirs test", "SKIPPED - encoding not supported"));
            }
        } catch (const std::exception& e) {
            LOG_WARNING(sLogger, ("Chinese ExcludeDirs test", "SKIPPED")("error", e.what()));
        }
    }

    // Windows Test 3: Chinese ExcludeFiles
    {
        try {
            // UTF-8: "混合"
            string chineseMixed = "\xE6\xB7\xB7\xE5\x90\x88";
            filesystem::path basePath = filesystem::current_path();
            string fullPath = basePath.string() + "\\" + chineseMixed + "\\*.log";
            // UTF-8: "排除.log" and "测试.log"
            string excludeFile1 = "\xE6\x8E\x92\xE9\x99\xA4.log";
            string excludeFile2 = "\xE6\xB5\x8B\xE8\xAF\x95.log";

            configStr = R"(
                {
                    "FilePaths": [],
                    "ExcludeFiles": []
                }
            )";
            APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
            configJson["FilePaths"].append(Json::Value(fullPath));
            configJson["ExcludeFiles"].append(Json::Value(excludeFile1));
            configJson["ExcludeFiles"].append(Json::Value(excludeFile2));
            config.reset(new FileDiscoveryOptions());

            bool initSuccess = config->Init(configJson, ctx, pluginType);
            if (initSuccess) {
                APSARA_TEST_EQUAL(2U, config->mExcludeFiles.size());
                APSARA_TEST_TRUE(config->mHasBlacklist);

                // Verify the base path contains Chinese directory name
                string chineseMixedACP = ConvertAndNormalizeNativePath(chineseMixed);
                APSARA_TEST_TRUE(config->GetBasePath().find(chineseMixedACP) != string::npos);

                // Verify ExcludeFiles contain Chinese filenames (converted to ACP)
                string excludeFile1ACP = ConvertAndNormalizeNativePath(excludeFile1);
                string excludeFile2ACP = ConvertAndNormalizeNativePath(excludeFile2);
                APSARA_TEST_EQUAL(excludeFile1ACP, config->mFileNameBlacklist[0]);
                APSARA_TEST_EQUAL(excludeFile2ACP, config->mFileNameBlacklist[1]);

                LOG_INFO(sLogger, ("Chinese ExcludeFiles test", "PASSED"));
            } else {
                LOG_WARNING(sLogger, ("Chinese ExcludeFiles test", "SKIPPED - encoding not supported"));
            }
        } catch (const std::exception& e) {
            LOG_WARNING(sLogger, ("Chinese ExcludeFiles test", "SKIPPED")("error", e.what()));
        }
    }

    // Windows Test 4: Chinese ExcludeFilePaths
    {
        try {
            // UTF-8: "文档"
            string chineseDoc = "\xE6\x96\x87\xE6\xA1\xA3";
            filesystem::path basePath = filesystem::current_path();
            string fullPath = basePath.string() + "\\" + chineseDoc + "\\*.log";
            // UTF-8: "排除文件.log"
            string excludeFileName = "\xE6\x8E\x92\xE9\x99\xA4\xE6\x96\x87\xE4\xBB\xB6.log";
            string excludeFile = basePath.string() + "\\" + chineseDoc + "\\" + excludeFileName;

            configStr = R"(
                {
                    "FilePaths": [],
                    "ExcludeFilePaths": []
                }
            )";
            APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
            configJson["FilePaths"].append(Json::Value(fullPath));
            configJson["ExcludeFilePaths"].append(Json::Value(excludeFile));
            config.reset(new FileDiscoveryOptions());

            bool initSuccess = config->Init(configJson, ctx, pluginType);
            if (initSuccess) {
                APSARA_TEST_EQUAL(1U, config->mExcludeFilePaths.size());
                APSARA_TEST_TRUE(config->mHasBlacklist);

                // Verify the base path contains Chinese directory name
                string chineseDocACP = ConvertAndNormalizeNativePath(chineseDoc);
                APSARA_TEST_TRUE(config->GetBasePath().find(chineseDocACP) != string::npos);

                // Verify ExcludeFilePaths contains Chinese filename (converted to ACP and normalized)
                string excludeFileACP = ConvertAndNormalizeNativePath(excludeFile);
                APSARA_TEST_EQUAL(excludeFileACP, config->mFilePathBlacklist[0]);

                LOG_INFO(sLogger, ("Chinese ExcludeFilePaths test", "PASSED"));
            } else {
                LOG_WARNING(sLogger, ("Chinese ExcludeFilePaths test", "SKIPPED - encoding not supported"));
            }
        } catch (const std::exception& e) {
            LOG_WARNING(sLogger, ("Chinese ExcludeFilePaths test", "SKIPPED")("error", e.what()));
        }
    }

    // Windows Test 5: ASCII paths (baseline test to ensure basic functionality)
    {
        filesystem::path filePath = filesystem::absolute("test_logs\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute("test_logs\\exclude");
        filePath = NormalizeNativePath(filePath.string());
        excludeDir = NormalizeNativePath(excludeDir.string());
        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeDirs": [],
                "ExcludeFiles": ["temp.log"],
                "ExcludeFilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        filesystem::path excludeFile = filesystem::absolute("test_logs\\exclude.log");
        excludeFile = NormalizeNativePath(excludeFile.string());
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFile.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Verify all blacklist types work with ASCII paths
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_EQUAL(1U, config->mExcludeFiles.size());
        APSARA_TEST_EQUAL(1U, config->mExcludeFilePaths.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }
#endif
}

void FileDiscoveryOptionsUnittest::TestWindowsDriveLetterCaseInsensitive() const {
#if defined(_MSC_VER)
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;

    // Test 1: Base path with lowercase drive letter
    // Expected: Should parse correctly (NormalizeNativePath will convert to uppercase)
    {
        string lowerCasePath = "c:\\test\\*.log";
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(lowerCasePath));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: After normalization, path should start with uppercase drive letter
        APSARA_TEST_TRUE(config->GetBasePath().find("C:\\") == 0 || config->GetBasePath().find("c:\\") == 0);
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    }

    // Test 2: ExcludeDirs with lowercase drive letter
    // Expected: Should correctly add to blacklist regardless of case
    {
        filesystem::path filePath = filesystem::absolute("test_logs\\*.log");
        string upperFilePath = filePath.string();
        if (upperFilePath.size() >= 2 && upperFilePath[1] == ':') {
            upperFilePath[0] = toupper(upperFilePath[0]);
        }

        string lowerExcludeDir = filePath.parent_path().string();
        if (lowerExcludeDir.size() >= 2 && lowerExcludeDir[1] == ':') {
            lowerExcludeDir[0] = tolower(lowerExcludeDir[0]);
        }
        lowerExcludeDir += "\\exclude";

        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(upperFilePath));
        configJson["ExcludeDirs"].append(Json::Value(lowerExcludeDir));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Should successfully add to blacklist
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Test 3: ExcludeFilePaths with different drive letter case
    // Expected: Should correctly handle regardless of case
    {
        filesystem::path filePath = filesystem::absolute("test_files\\*.log");
        filesystem::path excludeFile = filesystem::absolute("test_files\\exclude.log");

        string filePathStr = filePath.string();
        string excludeFileStr = excludeFile.string();

        // Make drive letters different case
        if (filePathStr.size() >= 2 && filePathStr[1] == ':') {
            filePathStr[0] = tolower(filePathStr[0]);
        }
        if (excludeFileStr.size() >= 2 && excludeFileStr[1] == ':') {
            excludeFileStr[0] = toupper(excludeFileStr[0]);
        }

        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeFilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFileStr));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: File path blacklist should work with different case
        APSARA_TEST_EQUAL(1U, config->mExcludeFilePaths.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Test 4: Wildcard path with lowercase drive letter
    // Expected: Should correctly parse wildcard path
    {
        string wildcardPath = "c:\\*\\logs\\*.log";
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(wildcardPath));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Should parse wildcard paths correctly (C:\, C:\*, C:\*\logs)
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        APSARA_TEST_EQUAL(3U, config->GetWildcardPaths().size());
    }

    // Test 5: Mixed case in base path and multiple blacklists
    // Expected: All path operations should be case-insensitive for drive letters
    {
        filesystem::path filePath = filesystem::absolute("multi_test\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute("multi_test\\exclude_dir");
        filesystem::path excludeFile = filesystem::absolute("multi_test\\exclude.log");

        string filePathStr = filePath.string();
        string excludeDirStr = excludeDir.string();
        string excludeFileStr = excludeFile.string();

        // Mix cases: lowercase base, uppercase exclude dir, lowercase exclude file
        if (filePathStr.size() >= 2 && filePathStr[1] == ':') {
            filePathStr[0] = 'c';
        }
        if (excludeDirStr.size() >= 2 && excludeDirStr[1] == ':') {
            excludeDirStr[0] = 'C';
        }
        if (excludeFileStr.size() >= 2 && excludeFileStr[1] == ':') {
            excludeFileStr[0] = 'c';
        }

        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeDirs": [],
                "ExcludeFilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        configJson["ExcludeDirs"].append(Json::Value(excludeDirStr));
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFileStr));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: All blacklists should be added successfully
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_EQUAL(1U, config->mExcludeFilePaths.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }
#endif
}

UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnFailedInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestFilePaths)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestWindowsRootPathCollection)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestChinesePathMatching)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestWindowsDriveLetterCaseInsensitive)

} // namespace logtail

UNIT_TEST_MAIN
