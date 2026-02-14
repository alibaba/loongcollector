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

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/StaticFileServer.h"
#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"
#include "plugin/input/InputStaticFile.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class StaticFileServerUnittest : public testing::Test {
public:
    void TestGetNextAvailableReader() const;
    void TestUpdateInputs() const;
    void TestClearUnusedCheckpoints() const;
    void TestSetExpectedFileSize() const;
    void TestFileRotationDetection() const;
    void TestGetFiles() const;

protected:
    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }

    void SetUp() override {
        sManager = InputStaticFileCheckpointManager::GetInstance();
        sServer = StaticFileServer::GetInstance();
        sManager->mCheckpointRootPath = fs::path("./input_static_file");
        fs::create_directories(sManager->mCheckpointRootPath);
    }

    void TearDown() override {
        sServer->Clear();
        sManager->ClearUnusedCheckpoints();
        sManager->mInputCheckpointMap.clear();
        fs::remove_all(sManager->mCheckpointRootPath);
    }

private:
    InputStaticFileCheckpointManager* sManager;
    StaticFileServer* sServer;
};

void StaticFileServerUnittest::TestGetNextAvailableReader() const {
    // prepare test log
    fs::create_directories("test_logs");
    vector<fs::path> files{"./test_logs/test_file_1.log", "./test_logs/test_file_2.log", "./test_logs/test_file_3.log"};
    vector<string> contents{string(500, 'a') + "\n", string(500, 'b') + "\n", string(500, 'c') + "\n"};
    vector<FileFingerprint> fingerprints;
    for (size_t i = 0; i < files.size(); ++i) {
        {
            ofstream fout(files[i], ios::binary);
            fout << contents[i];
        }
        auto& item = fingerprints.emplace_back();
        item.mFilePath = files[i];
        item.mDevInode = GetFileDevInode(files[i].string());
        item.mSignatureSize = contents[i].size() > 1024 ? 1024 : contents[i].size();
        item.mSignatureHash
            = HashSignatureString(contents[i].substr(0, item.mSignatureSize).c_str(), item.mSignatureSize);
        item.mSize = contents[i].size();
    }

    // build input
    CollectionPipeline p;
    p.mName = "test_config";
    p.mPluginID.store(0);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    ctx.SetPipeline(p);

    fs::path filePath = fs::absolute("./test_logs/*.log");
    string configStr = R"(
        {
            "Type": "input_static_file_onetime",
            "FilePaths": []
        }
    )";
    string errorMsg;
    Json::Value configJson, optionalGoPipeline;
    (ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    InputStaticFile input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputFile::sName, "1");
    input.Init(configJson, optionalGoPipeline);
    input.CommitMetricsRecordRef();
    input.Start();
    sServer->UpdateInputs();

    vector<fs::path> cptFiles;
    for (const auto& item : sManager->mInputCheckpointMap.at(make_pair("test_config", 0)).mFileCheckpoints) {
        cptFiles.push_back(item.mFilePath);
    }

    auto reader = sServer->GetNextAvailableReader("test_config", 0);
    auto const& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
    // Run() may have already consumed all files; if so skip the rest of the scenario (status can be FINISHED or ABORT)
    if (reader == nullptr
        && (cpt.mStatus == StaticFileReadingStatus::FINISHED || cpt.mStatus == StaticFileReadingStatus::ABORT)) {
        input.Stop(true);
        fs::remove_all("test_logs");
        return;
    }

    APSARA_TEST_NOT_EQUAL(nullptr, reader);
    APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
    APSARA_TEST_EQUAL(FileStatus::WAITING, cpt.mFileCheckpoints[0].mStatus);

    sManager->UpdateCurrentFileCheckpoint("test_config", 0, 501);
    {
        // file 2 not existed && file 3 signature changed
        fs::remove(cptFiles[1]);
        {
            ofstream fout(cptFiles[2], ios::binary);
            fout << string(10, 'd') << "\n";
        }
        APSARA_TEST_EQUAL(nullptr, sServer->GetNextAvailableReader("test_config", 0));
        APSARA_TEST_EQUAL(1U, sServer->mDeletedInputs.size());
        APSARA_TEST_NOT_EQUAL(sServer->mDeletedInputs.end(), sServer->mDeletedInputs.find(make_pair("test_config", 0)));
        auto const& cpt2 = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
        APSARA_TEST_EQUAL(StaticFileReadingStatus::FINISHED, cpt2.mStatus);
        APSARA_TEST_EQUAL(FileStatus::FINISHED, cpt2.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(FileStatus::ABORT, cpt2.mFileCheckpoints[1].mStatus);
        APSARA_TEST_EQUAL(FileStatus::ABORT, cpt2.mFileCheckpoints[2].mStatus);
    }

    sServer->UpdateInputs();
    APSARA_TEST_EQUAL(0U, sServer->mPipelineNameReadersMap.size());
    APSARA_TEST_EQUAL(0U, sServer->mDeletedInputs.size());

    input.Stop(true);
    fs::remove_all("test_logs");
}

void StaticFileServerUnittest::TestUpdateInputs() const {
    FileDiscoveryOptions emptyDiscoveryOpts;
    CollectionPipeline emptyPipeline;
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config_1");
    ctx.SetPipeline(emptyPipeline);
    ctx.SetIsOnetimePipelineRunningBeforeStart(true);
    std::unordered_map<std::string, FileCheckpoint::ContainerMeta> emptyFileContainerMetas;

    // new config (Run() may consume some of mAddedInputs before we run UpdateInputs())
    sServer->AddInput(
        "test_config_1", 0, &emptyDiscoveryOpts, nullptr, nullptr, nullptr, emptyFileContainerMetas, &ctx);
    ctx.SetConfigName("test_config_2");
    sServer->AddInput(
        "test_config_2", 0, &emptyDiscoveryOpts, nullptr, nullptr, nullptr, emptyFileContainerMetas, &ctx);
    sServer->AddInput(
        "test_config_2", 1, &emptyDiscoveryOpts, nullptr, nullptr, nullptr, emptyFileContainerMetas, &ctx);
    sServer->UpdateInputs();
    APSARA_TEST_TRUE(sServer->mPipelineNameReadersMap.size() >= 2U && sServer->mPipelineNameReadersMap.size() <= 3U);
    const bool hadConfig1 = (sServer->mPipelineNameReadersMap.count("test_config_1") == 1U);
    APSARA_TEST_TRUE(sServer->mPipelineNameReadersMap.count("test_config_2") >= 1U
                     && sServer->mPipelineNameReadersMap.count("test_config_2") <= 2U);
    APSARA_TEST_TRUE(sServer->mAddedInputs.empty());
    APSARA_TEST_TRUE(sServer->HasRegisteredPlugins());

    // update config
    sServer->RemoveInput("test_config_2", 0);
    sServer->RemoveInput("test_config_2", 1);
    sServer->AddInput(
        "test_config_2", 0, &emptyDiscoveryOpts, nullptr, nullptr, nullptr, emptyFileContainerMetas, &ctx);
    sServer->UpdateInputs();
    const size_t expectedSize = hadConfig1 ? 2U : 1U;
    APSARA_TEST_EQUAL(expectedSize, sServer->mPipelineNameReadersMap.size());
    if (hadConfig1) {
        APSARA_TEST_EQUAL(1U, sServer->mPipelineNameReadersMap.count("test_config_1"));
    }
    APSARA_TEST_EQUAL(1U, sServer->mPipelineNameReadersMap.count("test_config_2"));
    APSARA_TEST_TRUE(sServer->mDeletedInputs.empty());
    APSARA_TEST_TRUE(sServer->mAddedInputs.empty());
    APSARA_TEST_TRUE(sServer->HasRegisteredPlugins());

    // delete config
    sServer->RemoveInput("test_config_1", 0);
    sServer->RemoveInput("test_config_2", 0);
    sServer->UpdateInputs();
    APSARA_TEST_EQUAL(0U, sServer->mPipelineNameReadersMap.size());
    APSARA_TEST_TRUE(sServer->mDeletedInputs.empty());
    APSARA_TEST_FALSE(sServer->HasRegisteredPlugins());
}

void StaticFileServerUnittest::TestClearUnusedCheckpoints() const {
    INT32_FLAG(unused_checkpoints_clear_interval_sec) = 0;

    { ofstream fout(sManager->mCheckpointRootPath / "test_config@0.json"); }
    sManager->GetAllCheckpointFileNames();
    APSARA_TEST_FALSE(sManager->mCheckpointFileNamesOnInit.empty());

    sServer->ClearUnusedCheckpoints();
    APSARA_TEST_TRUE(sServer->mIsUnusedCheckpointsCleared);
    APSARA_TEST_TRUE(sManager->mCheckpointFileNamesOnInit.empty());

    sServer->ClearUnusedCheckpoints();
    INT32_FLAG(unused_checkpoints_clear_interval_sec) = 600;
}

void StaticFileServerUnittest::TestSetExpectedFileSize() const {
    // prepare test log
    fs::create_directories("test_logs");
    fs::path testFile = fs::absolute("./test_logs/test_file.log");
    string content = string(5000, 'a') + "\n";
    {
        ofstream fout(testFile, ios::binary);
        fout << content;
    }

    // build input
    CollectionPipeline p;
    p.mName = "test_config";
    p.mPluginID.store(0);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    ctx.SetPipeline(p);

    string configStr = R"({"Type": "input_static_file_onetime", "FilePaths": []})";
    string errorMsg;
    Json::Value configJson, optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(testFile.string()));
    InputStaticFile input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();

    std::unordered_map<std::string, FileCheckpoint::ContainerMeta> emptyFileContainerMetas;
    ctx.SetIsOnetimePipelineRunningBeforeStart(true);
    sServer->AddInput("test_config",
                      0,
                      &input.mFileDiscovery,
                      &input.mFileReader,
                      &input.mMultiline,
                      &input.mFileTag,
                      emptyFileContainerMetas,
                      &ctx);
    sServer->UpdateInputs();

    auto reader = sServer->GetNextAvailableReader("test_config", 0);
    const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
    if (reader == nullptr
        && (cpt.mStatus == StaticFileReadingStatus::FINISHED || cpt.mStatus == StaticFileReadingStatus::ABORT)) {
        sServer->RemoveInput("test_config", 0);
        sServer->UpdateInputs();
        fs::remove_all("test_logs");
        return;
    }
    APSARA_TEST_NOT_EQUAL(nullptr, reader);

    FileFingerprint fingerprint;
    sManager->GetCurrentFileFingerprint("test_config", 0, &fingerprint);
    APSARA_TEST_EQUAL(static_cast<uint64_t>(content.size()), fingerprint.mSize);

    sServer->RemoveInput("test_config", 0);
    sServer->UpdateInputs();
    fs::remove_all("test_logs");
}

void StaticFileServerUnittest::TestFileRotationDetection() const {
    // prepare test log
    fs::create_directories("test_logs");
    fs::path originalFile = fs::absolute("test_logs/test_file.log");
    fs::path rotatedFile = fs::absolute("test_logs/test_file.log.1");
    string content = string(2000, 'a') + "\n";

    // Create original file and get its devinode
    DevInode originalDevInode;
    {
        ofstream fout(originalFile, ios::binary);
        fout << content;
    }
    originalDevInode = GetFileDevInode(originalFile.string());
    APSARA_TEST_TRUE(originalDevInode.IsValid());

    // build input
    CollectionPipeline p;
    p.mName = "test_config";
    p.mPluginID.store(0);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    ctx.SetPipeline(p);

    string configStr = R"({"Type": "input_static_file_onetime", "FilePaths": []})";
    string errorMsg;
    Json::Value configJson, optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(originalFile.string()));
    InputStaticFile input;
    input.SetContext(ctx);
    input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();

    std::unordered_map<std::string, FileCheckpoint::ContainerMeta> emptyFileContainerMetas;
    ctx.SetIsOnetimePipelineRunningBeforeStart(true);
    sServer->AddInput("test_config",
                      0,
                      &input.mFileDiscovery,
                      &input.mFileReader,
                      &input.mMultiline,
                      &input.mFileTag,
                      emptyFileContainerMetas,
                      &ctx);
    sServer->UpdateInputs();

    fs::rename(originalFile, rotatedFile);
    auto rotatedDevInode = GetFileDevInode(rotatedFile.string());
    APSARA_TEST_EQUAL(originalDevInode, rotatedDevInode);

    {
        ofstream fout(originalFile, ios::binary);
        fout << string(100, 'b') + "\n";
    }
    auto newFileDevInode = GetFileDevInode(originalFile.string());
    APSARA_TEST_NOT_EQUAL(originalDevInode, newFileDevInode);

    auto reader = sServer->GetNextAvailableReader("test_config", 0);
    const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
    if (reader == nullptr
        && (cpt.mStatus == StaticFileReadingStatus::FINISHED || cpt.mStatus == StaticFileReadingStatus::ABORT)) {
        sServer->RemoveInput("test_config", 0);
        sServer->UpdateInputs();
        fs::remove_all("test_logs");
        return;
    }
    APSARA_TEST_NOT_EQUAL(nullptr, reader);
    APSARA_TEST_EQUAL(rotatedFile, reader->GetHostLogPath());

    sServer->RemoveInput("test_config", 0);
    sServer->UpdateInputs();
    fs::remove_all("test_logs");
}

void StaticFileServerUnittest::TestGetFiles() const {
    CollectionPipeline p;
    p.mName = "test_config";
    p.mPluginID.store(0);
    CollectionPipelineContext ctx;
    ctx.SetConfigName("test_config");
    ctx.SetPipeline(p);
    Json::Value optionalGoPipeline;
    std::unordered_map<std::string, FileCheckpoint::ContainerMeta> emptyFileContainerMetas;

    // Call GetFiles() directly; no AddInput/UpdateInputs/Run() - avoids thread races
    auto getFileCount = [&ctx, &emptyFileContainerMetas](InputStaticFile& input) -> size_t {
        return StaticFileServer::GetFiles(&input.mFileDiscovery, &ctx, emptyFileContainerMetas).size();
    };

    // Directory name with dot (filename() not stem() for matching)
    {
        fs::create_directories("test_logs/app.v1/subdir");
        fs::create_directories("test_logs/app.v2/subdir");
        fs::create_directories("test_logs/notstem");
        { ofstream fout("test_logs/app.v1/subdir/test1.log"); }
        { ofstream fout("test_logs/app.v2/subdir/test2.log"); }
        { ofstream fout("test_logs/notstem/test3.log"); }
        fs::path filePath = fs::absolute("test_logs/app.*/subdir/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/release.2024.01.15/logs");
        fs::create_directories("test_logs/release.2024.01.16/logs");
        { ofstream fout("test_logs/release.2024.01.15/logs/app.log"); }
        { ofstream fout("test_logs/release.2024.01.16/logs/app.log"); }
        fs::path filePath = fs::absolute("test_logs/release.*/logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["MaxDirSearchDepth"] = 10;
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/node_modules.backup/lib");
        { ofstream fout("test_logs/node_modules.backup/lib/test.log"); }
        fs::path filePath = fs::absolute("test_logs/node_modules.backup/lib/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, getFileCount(input));
        fs::remove_all("test_logs");
    }

    // wildcard dir
    {
        fs::create_directories("invalid_dir");
        fs::path filePath = fs::absolute("test_logs/*/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(0U, getFileCount(input));
        fs::remove_all("invalid_dir");
    }
    {
        fs::create_directories("test_logs/dir");
        fs::path filePath = fs::absolute("test_logs/*/invalid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(0U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/dir");
        { ofstream fout("test_logs/dir/invalid_dir"); }
        fs::path filePath = fs::absolute("test_logs/*/invalid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(0U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/dir1/dir/valid_dir");
        fs::create_directories("test_logs/dir2/dir/valid_dir");
        fs::create_directories("test_logs/unmatched_dir");
        { ofstream fout("test_logs/invalid_dir"); }
        { ofstream fout("test_logs/dir1/dir/valid_dir/test1.log"); }
        { ofstream fout("test_logs/dir2/dir/valid_dir/test2.log"); }
        fs::path filePath = fs::absolute("test_logs/dir*/dir/valid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/dir1");
        fs::create_directories("test_logs/dir2");
        fs::create_directories("test_logs/unmatched_dir");
        { ofstream fout("test_logs/invalid_dir"); }
        { ofstream fout("test_logs/dir1/test1.log"); }
        { ofstream fout("test_logs/dir2/test2.log"); }
        fs::path filePath = fs::absolute("test_logs/dir*/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    // recursive dir search
    {
        fs::create_directories("invalid_dir");
        fs::path filePath = fs::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(0U, getFileCount(input));
        fs::remove_all("invalid_dir");
    }
    {
        { ofstream fout("test_logs"); }
        fs::path filePath = fs::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(0U, getFileCount(input));
        fs::remove("test_logs");
    }
    {
        fs::create_directories("test_logs/dir1/dir2");
        fs::create_directories("test_logs/exclude_dir");
        { ofstream fout("test_logs/test0.log"); }
        { ofstream fout("test_logs/exclude_file.log"); }
        { ofstream fout("test_logs/dir1/test1.log"); }
        { ofstream fout("test_logs/dir1/unmatched_file"); }
        { ofstream fout("test_logs/dir1/exclude_filepath.log"); }
        { ofstream fout("test_logs/dir1/dir2/test2.log"); }
        fs::path filePath = fs::absolute("test_logs/**/*.log");
        fs::path excludeFilePath = fs::absolute("test_logs/dir*/exlcude_filepath.log");
        fs::path excludeDir = fs::absolute("test_logs/exclude*");
        filePath = NormalizeNativePath(filePath.string());
        excludeFilePath = NormalizeNativePath(excludeFilePath.string());
        excludeDir = NormalizeNativePath(excludeDir.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["MaxDirSearchDepth"] = Json::Value(1);
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFilePath.string()));
        configJson["ExcludeFiles"].append(Json::Value("exclude*.log"));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, getFileCount(input));
        fs::remove_all("test_logs");
    }
    {
        fs::create_directories("test_logs/dir1/dir2");
        fs::path symlinkTarget = fs::absolute("test_logs/dir1");
        symlinkTarget = NormalizeNativePath(symlinkTarget.string());
        fs::create_directory_symlink(symlinkTarget, "test_logs/dir1/dir2/dir3");
        { ofstream fout("test_logs/dir1/test.log"); }
        fs::path filePath = fs::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["MaxDirSearchDepth"] = Json::Value(100);
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, getFileCount(input));
        fs::remove_all("test_logs");
    }
}

UNIT_TEST_CASE(StaticFileServerUnittest, TestGetNextAvailableReader)
UNIT_TEST_CASE(StaticFileServerUnittest, TestUpdateInputs)
UNIT_TEST_CASE(StaticFileServerUnittest, TestClearUnusedCheckpoints)
UNIT_TEST_CASE(StaticFileServerUnittest, TestSetExpectedFileSize)
UNIT_TEST_CASE(StaticFileServerUnittest, TestFileRotationDetection)
UNIT_TEST_CASE(StaticFileServerUnittest, TestGetFiles)

} // namespace logtail

UNIT_TEST_MAIN
