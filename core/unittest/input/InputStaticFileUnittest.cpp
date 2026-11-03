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

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "file_server/StaticFileServer.h"
#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"
#include "plugin/input/InputStaticFile.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputStaticFileUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void TestCreateInnerProcessors();
    void OnPipelineUpdate();
    void TestGetFiles();
    void TestDirectoryNameWithDot();
    void OnEnableContainerDiscovery();

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        sManager->mCheckpointRootPath = filesystem::path("./input_static_file");
    }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        p.mPluginID.store(0);
        ctx.SetPipeline(p);
        filesystem::create_directories(sManager->mCheckpointRootPath);
    }

    void TearDown() override {
        sServer->Clear();
        sManager->ClearUnusedCheckpoints();
        sManager->mInputCheckpointMap.clear();
        filesystem::remove_all(sManager->mCheckpointRootPath);
    }

private:
    static InputStaticFileCheckpointManager* sManager;
    static StaticFileServer* sServer;

    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

InputStaticFileCheckpointManager* InputStaticFileUnittest::sManager = InputStaticFileCheckpointManager::GetInstance();
StaticFileServer* InputStaticFileUnittest::sServer = StaticFileServer::GetInstance();

void InputStaticFileUnittest::OnSuccessfulInit() {
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    filePath = NormalizeNativePath(filePath.string());

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_static_file_onetime",
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputStaticFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_TRUE(input->mFileReader.mTailingAllMatchedFiles);
    APSARA_TEST_EQUAL(FileReaderOptions::InputType::InputFile, input->mFileReader.mInputType);

    // valid optional param
    AppConfig::GetInstance()->mPurageContainerMode = true;
    configStr = R"(
        {
            "Type": "input_static_file_onetime",
            "FilePaths": [],
            "EnableContainerDiscovery": true,
            "Multiline": {}
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputStaticFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
    APSARA_TEST_TRUE(input->mFileDiscovery.IsContainerDiscoveryEnabled());
    AppConfig::GetInstance()->mPurageContainerMode = false;

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_static_file_onetime",
            "FilePaths": [],
            "EnableContainerDiscovery": "true",
            "Multiline": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputStaticFile());
    ctx.SetExactlyOnceFlag(false);
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
}

void InputStaticFileUnittest::OnFailedInit() {
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    filePath = NormalizeNativePath(filePath.string());

    // file path not existed
    input.reset(new InputStaticFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // file encoding not valid
    configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "FileEncoding": "unknown"
            }
        )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputStaticFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // not in container but EnableContainerDiscovery is set
    configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "EnableContainerDiscovery": true
            }
        )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputStaticFile());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
}

void InputStaticFileUnittest::TestCreateInnerProcessors() {
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");
    filePath = NormalizeNativePath(filePath.string());
    {
        // no multiline
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\n', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
    }
    {
        // custom multiline
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "Multiline": {
                    "StartPattern": "\\d+",
                    "EndPattern": "end",
                    "IgnoringUnmatchWarning": true,
                    "UnmatchedContentTreatment": "discard"
                },
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitMultilineLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitMultilineLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL(MultilineOptions::Mode::CUSTOM, plugin->mMultiline.mMode);
        APSARA_TEST_STREQ("\\d+", plugin->mMultiline.mStartPattern.c_str());
        APSARA_TEST_STREQ("", plugin->mMultiline.mContinuePattern.c_str());
        APSARA_TEST_STREQ("end", plugin->mMultiline.mEndPattern.c_str());
        APSARA_TEST_TRUE(plugin->mMultiline.mIgnoringUnmatchWarning);
        APSARA_TEST_EQUAL(MultilineOptions::UnmatchedContentTreatment::DISCARD,
                          plugin->mMultiline.mUnmatchedContentTreatment);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
    }
    {
        // json multiline, first processor is json parser
        ctx.SetIsFirstProcessorJsonFlag(true);
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\0', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFirstProcessorJsonFlag(false);
    }
    {
        // json multiline, json mode
        ctx.SetIsFirstProcessorJsonFlag(true);
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "Multiline": {
                    "Mode": "JSON"
                },
                "AppendingLogPositionMeta": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_EQUAL(DEFAULT_CONTENT_KEY, plugin->mSourceKey);
        APSARA_TEST_EQUAL('\0', plugin->mSplitChar);
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFirstProcessorJsonFlag(false);
    }
    {
        // disable raw content: has native processor
        ctx.SetHasNativeProcessorsFlag(true);
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetHasNativeProcessorsFlag(false);
    }
    {
        // disable raw content: exactly once
        ctx.SetExactlyOnceFlag(true);
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetExactlyOnceFlag(false);
    }
    {
        // disable raw content: flushing through go pipeline
        ctx.SetIsFlushingThroughGoPipelineFlag(true);
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_FALSE(plugin->mEnableRawContent);
        ctx.SetIsFlushingThroughGoPipelineFlag(false);
    }
    {
        // enable raw content
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input->mInnerProcessors.size());
        APSARA_TEST_EQUAL(ProcessorSplitLogStringNative::sName, input->mInnerProcessors[0]->Name());
        auto plugin = static_cast<ProcessorSplitLogStringNative*>(input->mInnerProcessors[0]->mPlugin.get());
        APSARA_TEST_TRUE(plugin->mEnableRawContent);
    }
}

void InputStaticFileUnittest::OnPipelineUpdate() {
    // prepare logs
    filesystem::create_directories("test_logs");
    vector<filesystem::path> files{"./test_logs/test_file_1.log"};
    { ofstream fout(files[0]); }

    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("./test_logs/*.log");
    filePath = NormalizeNativePath(filePath.string());
    {
        // new config
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();

        APSARA_TEST_TRUE(input.Start());
        APSARA_TEST_EQUAL(&input.mFileDiscovery, sServer->GetFileDiscoveryConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mFileReader, sServer->GetFileReaderConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mMultiline, sServer->GetMultilineConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mFileTag, sServer->GetFileTagConfig("test_config", 0).first);
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
        APSARA_TEST_EQUAL(1U, cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL(filesystem::absolute(files[0]).lexically_normal(), cpt.mFileCheckpoints[0].mFilePath);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@0.json"));

        APSARA_TEST_TRUE(input.Stop(true));
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileDiscoveryConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileReaderConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetMultilineConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileTagConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(sManager->mInputCheckpointMap.end(),
                          sManager->mInputCheckpointMap.find(make_pair("test_config", 0)));
        APSARA_TEST_FALSE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@0.json"));
    }
    {
        // old config
        {
            ofstream fout(sManager->mCheckpointRootPath / "test_config@0.json");
            string cptStr = R"({
                    "config_name" : "test_config",
                    "current_file_index" : 0,
                    "file_count" : 1,
                    "start_time" : 1739349980,
                    "expire_time" : 1739349981,
                    "files" :
                    [
                        {
                            "dev" : 2081,
                            "filepath" : "./test_logs/test_file_2.log",
                            "inode" : 79956083,
                            "last_read_time" : 1739349981,
                            "offset" : 100,
                            "sig_hash" : 5407334769256465540,
                            "sig_size" : 500,
                            "size" : 500,
                            "start_time" : 1739349980,
                            "status" : "reading"
                        }
                    ],
                    "input_index" : 0,
                    "status" : "running"
                }
            )";
            fout << cptStr;
        }
        sManager->GetAllCheckpointFileNames();

        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        ctx.SetIsOnetimePipelineRunningBeforeStart(true);
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();

        APSARA_TEST_TRUE(input.Start());
        APSARA_TEST_EQUAL(&input.mFileDiscovery, sServer->GetFileDiscoveryConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mFileReader, sServer->GetFileReaderConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mMultiline, sServer->GetMultilineConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(&input.mFileTag, sServer->GetFileTagConfig("test_config", 0).first);
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
        APSARA_TEST_EQUAL(1U, cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL("./test_logs/test_file_2.log", cpt.mFileCheckpoints[0].mFilePath.string());
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@0.json"));

        APSARA_TEST_TRUE(input.Stop(true));
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileDiscoveryConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileReaderConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetMultilineConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(nullptr, sServer->GetFileTagConfig("test_config", 0).first);
        APSARA_TEST_EQUAL(sManager->mInputCheckpointMap.end(),
                          sManager->mInputCheckpointMap.find(make_pair("test_config", 0)));
        APSARA_TEST_FALSE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@0.json"));
    }
    filesystem::remove_all("test_logs");
}

void InputStaticFileUnittest::TestGetFiles() {
    unique_ptr<InputStaticFile> input;
    Json::Value optionalGoPipeline;

    // wildcard dir
    {
        // invalid base dir
        filesystem::create_directories("invalid_dir");

        filesystem::path filePath = filesystem::absolute("test_logs/*/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input.GetFiles().empty());

        filesystem::remove_all("invalid_dir");
    }
    {
        // non-existing const subdir
        filesystem::create_directories("test_logs/dir");

        filesystem::path filePath = filesystem::absolute("test_logs/*/invalid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input.GetFiles().empty());

        filesystem::remove_all("test_logs");
    }
    {
        // invalid const subdir
        filesystem::create_directories("test_logs/dir");
        { ofstream fout("test_logs/dir/invalid_dir"); }

        filesystem::path filePath = filesystem::absolute("test_logs/*/invalid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input.GetFiles().empty());

        filesystem::remove_all("test_logs");
    }
    {
        // the last subdir before ** is const
        filesystem::create_directories("test_logs/dir1/dir/valid_dir");
        filesystem::create_directories("test_logs/dir2/dir/valid_dir");
        filesystem::create_directories("test_logs/unmatched_dir");
        { ofstream fout("test_logs/invalid_dir"); }
        { ofstream fout("test_logs/dir1/dir/valid_dir/test1.log"); }
        { ofstream fout("test_logs/dir2/dir/valid_dir/test2.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/dir*/dir/valid_dir/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, input.GetFiles().size());

        filesystem::remove_all("test_logs");
    }
    {
        // the last subdir before ** is wildcard
        filesystem::create_directories("test_logs/dir1");
        filesystem::create_directories("test_logs/dir2");
        filesystem::create_directories("test_logs/unmatched_dir");
        { ofstream fout("test_logs/invalid_dir"); }
        { ofstream fout("test_logs/dir1/test1.log"); }
        { ofstream fout("test_logs/dir2/test2.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/dir*/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(2U, input.GetFiles().size());

        filesystem::remove_all("test_logs");
    }
    // recursive dir search
    {
        // non-existing base path
        filesystem::create_directories("invalid_dir");

        filesystem::path filePath = filesystem::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input.GetFiles().empty());

        filesystem::remove_all("invalid_dir");
    }
    {
        // invalid base path
        { ofstream fout("test_logs"); }

        filesystem::path filePath = filesystem::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input.GetFiles().empty());

        filesystem::remove("test_logs");
    }
    {
        // normal
        filesystem::create_directories("test_logs/dir1/dir2");
        filesystem::create_directories("test_logs/exclude_dir");
        { ofstream fout("test_logs/test0.log"); }
        { ofstream fout("test_logs/exclude_file.log"); }
        { ofstream fout("test_logs/dir1/test1.log"); }
        { ofstream fout("test_logs/dir1/unmatched_file"); }
        { ofstream fout("test_logs/dir1/exclude_filepath.log"); }
        { ofstream fout("test_logs/dir1/dir2/test2.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/**/*.log");
        filesystem::path excludeFilePath = filesystem::absolute("test_logs/dir*/exlcude_filepath.log");
        filesystem::path excludeDir = filesystem::absolute("test_logs/exclude*");
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
        APSARA_TEST_EQUAL(2U, input.GetFiles().size());

        filesystem::remove_all("test_logs");
    }
    {
        // loop caused by symlink
        filesystem::create_directories("test_logs/dir1/dir2");
        filesystem::path symlinkTarget = filesystem::absolute("test_logs/dir1");
        symlinkTarget = NormalizeNativePath(symlinkTarget.string());
        filesystem::create_directory_symlink(symlinkTarget, "test_logs/dir1/dir2/dir3");
        { ofstream fout("test_logs/dir1/test.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/**/*.log");
        filePath = NormalizeNativePath(filePath.string());
        Json::Value configJson;
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["MaxDirSearchDepth"] = Json::Value(100);
        InputStaticFile input;
        input.SetContext(ctx);
        input.CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
        input.CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(1U, input.GetFiles().size());

        filesystem::remove_all("test_logs");
    }
}

void InputStaticFileUnittest::TestDirectoryNameWithDot() {
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test 1: Directory name containing dot (e.g., "dir.test")
    // This test ensures that filename() is used instead of stem() for directory matching
    // stem() would strip everything after the dot, causing match failure
    {
        filesystem::create_directories("test_logs/app.v1/subdir");
        filesystem::create_directories("test_logs/app.v2/subdir");
        filesystem::create_directories("test_logs/notstem");
        { ofstream fout("test_logs/app.v1/subdir/test1.log"); }
        { ofstream fout("test_logs/app.v2/subdir/test2.log"); }
        { ofstream fout("test_logs/notstem/test3.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/app.*/subdir/*.log");
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find 2 files from app.v1 and app.v2 directories
        // If stem() was used, it would match "app" only and fail to match "app.v1" and "app.v2"
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(2U, files.size());

        filesystem::remove_all("test_logs");
    }

    // Test 2: Directory with multiple dots using ** pattern
    {
        filesystem::create_directories("test_logs/release.2024.01.15/logs");
        filesystem::create_directories("test_logs/release.2024.01.16/logs");
        { ofstream fout("test_logs/release.2024.01.15/logs/app.log"); }
        { ofstream fout("test_logs/release.2024.01.16/logs/app.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/release.*/logs/**/*.log");
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "MaxDirSearchDepth": 10
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find 2 files from both release directories
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(2U, files.size());

        filesystem::remove_all("test_logs");
    }

    // Test 3: Exact directory name with dot
    {
        filesystem::create_directories("test_logs/node_modules.backup/lib");
        { ofstream fout("test_logs/node_modules.backup/lib/test.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/node_modules.backup/lib/*.log");
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find 1 file
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());

        filesystem::remove_all("test_logs");
    }
}

void InputStaticFileUnittest::OnEnableContainerDiscovery() {
}

UNIT_TEST_CASE(InputStaticFileUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputStaticFileUnittest, OnFailedInit)
UNIT_TEST_CASE(InputStaticFileUnittest, TestCreateInnerProcessors)
UNIT_TEST_CASE(InputStaticFileUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputStaticFileUnittest, TestGetFiles)
UNIT_TEST_CASE(InputStaticFileUnittest, TestDirectoryNameWithDot)
UNIT_TEST_CASE(InputStaticFileUnittest, OnEnableContainerDiscovery)

} // namespace logtail

UNIT_TEST_MAIN
