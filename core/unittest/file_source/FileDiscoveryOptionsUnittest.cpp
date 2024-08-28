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

#include <json/json.h>

#include "common/JsonUtil.h"
#include "file_server/FileDiscoveryOptions.h"
#include "pipeline/PipelineContext.h"
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
    PipelineContext ctx;
};

void FileDiscoveryOptionsUnittest::OnSuccessfulInit() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
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
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeFilePaths": [],
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
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeFiles": [],
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
    configStr = R"(
        {
            "FilePaths": [],
            "ExcludeDirs": [],
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
            "AllowingCollectingFilesInRootDir": true,
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

    // invlaid filepath
    filePath = filesystem::current_path();
    configStr = R"(
        {
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string() + filesystem::path::preferred_separator));
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_FALSE(config->Init(configJson, ctx, pluginType));
}

void FileDiscoveryOptionsUnittest::TestFilePaths() const {
    unique_ptr<FileDiscoveryOptions> config;
    Json::Value configJson;
    PipelineContext ctx;
    filesystem::path filePath;

    // no wildcard
    filePath = filesystem::path(".") / "test" / "*.log";
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["MaxDirSearchDepth"] = Json::Value(1);
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    APSARA_TEST_EQUAL((filesystem::current_path() / "test").string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    configJson.clear();

    // with wildcard */?
    filePath = filesystem::path(".") / "*" / "test" / "?" / "*.log";
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    configJson["MaxDirSearchDepth"] = Json::Value(1);
    config.reset(new FileDiscoveryOptions());
    APSARA_TEST_TRUE(config->Init(configJson, ctx, pluginType));
    APSARA_TEST_EQUAL(0, config->mMaxDirSearchDepth);
    APSARA_TEST_EQUAL((filesystem::current_path() / "*" / "test" / "?").string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
    APSARA_TEST_EQUAL(4U, config->GetWildcardPaths().size());
    APSARA_TEST_EQUAL(filesystem::current_path().string(), config->GetWildcardPaths()[0]);
    APSARA_TEST_EQUAL((filesystem::current_path() / "*").string(), config->GetWildcardPaths()[1]);
    APSARA_TEST_EQUAL((filesystem::current_path() / "*" / "test").string(), config->GetWildcardPaths()[2]);
    APSARA_TEST_EQUAL((filesystem::current_path() / "*" / "test" / "?").string(), config->GetWildcardPaths()[3]);
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
    APSARA_TEST_EQUAL((filesystem::current_path() / "*" / "test").string(), config->GetBasePath());
    APSARA_TEST_EQUAL("*.log", config->GetFilePattern());
}

UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, OnFailedInit)
UNIT_TEST_CASE(FileDiscoveryOptionsUnittest, TestFilePaths)

} // namespace logtail

UNIT_TEST_MAIN
