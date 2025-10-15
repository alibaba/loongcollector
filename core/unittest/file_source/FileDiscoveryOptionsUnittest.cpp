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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
    ex2 = NormalizeWindowsPath(ex2.string());
    ex3 = NormalizeWindowsPath(ex3.string());
#endif
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
#if defined(_MSC_VER)
    ex2 = NormalizeWindowsPath(ex2.string());
#endif
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
#if defined(_MSC_VER)
    ex2 = NormalizeWindowsPath(ex2.string());
    ex3 = NormalizeWindowsPath(ex3.string());
    ex4 = NormalizeWindowsPath(ex4.string());
    ex5 = NormalizeWindowsPath(ex5.string());
#endif
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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif

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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
    expectedBasePath = NormalizeWindowsPath(expectedBasePath.string());
#endif
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
#if defined(_MSC_VER)
    expectedBasePath1 = NormalizeWindowsPath(expectedBasePath1.string());
    expectedWildcard0 = NormalizeWindowsPath(expectedWildcard0.string());
    expectedWildcard1 = NormalizeWindowsPath(expectedWildcard1.string());
    expectedWildcard2 = NormalizeWindowsPath(expectedWildcard2.string());
    expectedWildcard3 = NormalizeWindowsPath(expectedWildcard3.string());
#endif
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
#if defined(_MSC_VER)
    expectedBasePath2 = NormalizeWindowsPath(expectedBasePath2.string());
#endif
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
        filePath = NormalizeWindowsPath(filePath.string());
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
        filePath = NormalizeWindowsPath(filePath.string());
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
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Wildcard paths should include C: and C:\*
        APSARA_TEST_EQUAL(2U, config->GetWildcardPaths().size());
    }

    // Test 4: Multi-level path with wildcard at root and AllowingCollectingFilesInRootDir=true
    // Expected: Should work when both config and global flag are enabled
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "C:\\*\\logs\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
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
        APSARA_TEST_EQUAL(2U, config->GetWildcardPaths().size());
        BOOL_FLAG(enable_root_path_collection) = false;
    }

    // Test 5: Recursive search from root (C:\**\*.log)
    // Expected: Should parse correctly with ** at root
    {
        filesystem::path filePath = "C:\\**\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Base path should be C:, MaxDirSearchDepth should be set
        APSARA_TEST_EQUAL("C:", config->GetBasePath());
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        APSARA_TEST_EQUAL(2, config->mMaxDirSearchDepth);
    }

    // Test 6: Recursive search with enable flag (D:\**\*.log with AllowingCollectingFilesInRootDir=true)
    // Expected: Should work with flag enabled
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "D:\\**\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
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
        APSARA_TEST_EQUAL("D:", config->GetBasePath());
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
    // Windows Test 1: Chinese path with UTF-8 encoding
    // Expected: Should successfully parse Chinese path
    {
        // "\346\265\213\350\257\225\347\233\256\345\275\225" = "测试目录"
        string chineseDir = "\346\265\213\350\257\225\347\233\256\345\275\225";
        filesystem::path filePath = filesystem::absolute(chineseDir + "\\**\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Base path should contain Chinese characters (in UTF-8)
        APSARA_TEST_TRUE(config->GetBasePath().find(chineseDir) != string::npos);
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    }

    // Windows Test 2: UTF-8 escaped Chinese path
    // Expected: Should handle UTF-8 encoded Chinese characters
    {
        // "\346\265\213\350\257\225\347\233\256\345\275\225" = "测试目录"
        string utf8Path = "\346\265\213\350\257\225\347\233\256\345\275\225"; // "测试目录"
        filesystem::path filePath = filesystem::absolute(utf8Path + "\\**\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Should successfully parse UTF-8 encoded path
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    }

    // Windows Test 3: Chinese ExcludeDirs with UTF-8 encoding
    // Expected: Should add Chinese directory to blacklist
    {
        // "\346\227\245\345\277\227" = "日志", "\351\273\221\345\220\215\345\215\225" = "黑名单"
        string chineseLog = "\346\227\245\345\277\227";
        string chineseBlacklist = "\351\273\221\345\220\215\345\215\225";
        filesystem::path filePath = filesystem::absolute(chineseLog + "\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute(chineseLog + "\\" + chineseBlacklist);
        filePath = NormalizeWindowsPath(filePath.string());
        excludeDir = NormalizeWindowsPath(excludeDir.string());
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
        // Expected: Blacklist should contain Chinese path
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Windows Test 4: UTF-8 escaped Chinese in ExcludeDirs
    // Expected: Should handle UTF-8 encoded blacklist path
    {
        // "\346\265\213\350\257\225-\351\273\221\345\220\215\345\215\225" = "测试-黑名单"
        string blacklistDir = "logs\\\346\265\213\350\257\225-\351\273\221\345\220\215\345\215\225";
        filesystem::path filePath = filesystem::absolute("logs\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute(blacklistDir);
        filePath = NormalizeWindowsPath(filePath.string());
        excludeDir = NormalizeWindowsPath(excludeDir.string());
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
        // Expected: Should successfully add UTF-8 encoded blacklist
        APSARA_TEST_EQUAL(1U, config->mExcludeDirs.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Windows Test 5: Mixed Chinese and UTF-8 in ExcludeFiles
    // Expected: Should handle both native and UTF-8 encoded filenames
    {
        // "\346\267\267\345\220\210" = "混合", "\346\216\222\351\231\244" = "排除"
        string chineseMixed = "\346\267\267\345\220\210";
        string chineseExclude = "\346\216\222\351\231\244";
        filesystem::path filePath = filesystem::absolute(chineseMixed + "\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeFiles": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeFiles"].append(Json::Value(chineseExclude + ".log"));
        configJson["ExcludeFiles"].append(Json::Value("\346\265\213\350\257\225.log"));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: Should handle both Chinese and UTF-8 filenames
        APSARA_TEST_EQUAL(2U, config->mExcludeFiles.size());
        APSARA_TEST_TRUE(config->mHasBlacklist);
    }

    // Windows Test 6: Chinese ExcludeFilePaths with UTF-8 encoding
    // Expected: Should add Chinese file path to blacklist
    {
        // "\346\226\207\346\241\243" = "文档", "\346\216\222\351\231\244\346\226\207\344\273\266" = "排除文件"
        string chineseDoc = "\346\226\207\346\241\243";
        string chineseExcludeFile = "\346\216\222\351\231\244\346\226\207\344\273\266";
        filesystem::path filePath = filesystem::absolute(chineseDoc + "\\*.log");
        filesystem::path excludeFile = filesystem::absolute(chineseDoc + "\\" + chineseExcludeFile + ".log");
        filePath = NormalizeWindowsPath(filePath.string());
        excludeFile = NormalizeWindowsPath(excludeFile.string());
        configStr = R"(
            {
                "FilePaths": [],
                "ExcludeFilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFile.string()));
        config.reset(new FileDiscoveryOptions());
        APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
        // Expected: File path blacklist should contain Chinese path
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
    // Expected: Should parse correctly (NormalizeWindowsPath will convert to uppercase)
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
        // Expected: Should parse wildcard paths correctly
        APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
        APSARA_TEST_EQUAL(2U, config->GetWildcardPaths().size());
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
