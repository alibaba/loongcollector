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

UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnFailedInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestFilePaths)

} // namespace logtail

UNIT_TEST_MAIN
