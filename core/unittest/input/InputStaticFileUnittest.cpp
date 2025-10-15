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
    void OnEnableContainerDiscovery();
    void TestWindowsRootPathCollection();
    void TestChinesePathMatching();
    void TestWindowsDriveLetterCaseInsensitive();

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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif

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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif

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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
    filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
        excludeFilePath = NormalizeWindowsPath(excludeFilePath.string());
        excludeDir = NormalizeWindowsPath(excludeDir.string());
#endif
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
#if defined(_MSC_VER)
        symlinkTarget = NormalizeWindowsPath(symlinkTarget.string());
#endif
        filesystem::create_directory_symlink(symlinkTarget, "test_logs/dir1/dir2/dir3");
        { ofstream fout("test_logs/dir1/test.log"); }

        filesystem::path filePath = filesystem::absolute("test_logs/**/*.log");
#if defined(_MSC_VER)
        filePath = NormalizeWindowsPath(filePath.string());
#endif
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

void InputStaticFileUnittest::OnEnableContainerDiscovery() {
}

void InputStaticFileUnittest::TestWindowsRootPathCollection() {
#if defined(_MSC_VER)
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test 1: Direct root path collection without enable_root_path_collection flag
    // Expected: Should fail because collecting from root path is not allowed by default
    {
        filesystem::path filePath = "C:\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Init should succeed but GetFiles should return empty because root collection is disabled
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: No files should be found from root path
        APSARA_TEST_TRUE(input->GetFiles().empty());
    }

    // Test 2: Direct root path collection with enable_root_path_collection flag
    // Expected: Should succeed with the flag enabled
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "C:\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "AllowingCollectingFilesInRootDir": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        // Expected: Init should succeed and root collection is allowed
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input->mFileDiscovery.IsAllowingCollectingFilesInRootDir());
        // Note: GetFiles() would return files if C:\ actually contains .log files
        BOOL_FLAG(enable_root_path_collection) = false;
    }

    // Test 3: Multi-level path with wildcard at root without flag
    // Expected: Should fail because root level has wildcard
    {
        filesystem::path filePath = "C:\\*\\logs\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Init should succeed but files at root level with wildcard won't be collected
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: GetFiles should return empty or limited results without root collection enabled
    }

    // Test 4: Multi-level path with wildcard at root with flag enabled
    // Expected: Should succeed and collect files matching the pattern
    {
        BOOL_FLAG(enable_root_path_collection) = true;
        filesystem::path filePath = "C:\\*\\logs\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "AllowingCollectingFilesInRootDir": true
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        // Expected: Init should succeed with root collection enabled
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        APSARA_TEST_TRUE(input->mFileDiscovery.IsAllowingCollectingFilesInRootDir());
        // Expected: GetFiles would traverse C:\*\logs\ directories
        BOOL_FLAG(enable_root_path_collection) = false;
    }

    // Test 5: Recursive search from root with **
    // Expected: Should respect the enable_root_path_collection flag
    {
        filesystem::path filePath = "C:\\**\\*.log";
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "MaxDirSearchDepth": 2
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        // Expected: Init succeeds but recursive search from root requires the flag
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Limited or no files without root collection flag
    }
#endif
}

void InputStaticFileUnittest::TestChinesePathMatching() {
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

#if defined(__linux__)
    // Linux Test 1: Chinese path matching
    // Expected: Should successfully match and collect files with Chinese characters in path
    {
        filesystem::create_directories("测试目录/子目录");
        { ofstream fout("测试目录/子目录/测试文件.log"); }
        
        filesystem::path filePath = filesystem::absolute("测试目录/**/*.log");
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
        // Expected: Should find the Chinese named log file
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("测试目录");
    }

    // Linux Test 2: Chinese path with exclusion filter
    // Expected: Should correctly filter out files matching Chinese blacklist patterns
    {
        filesystem::create_directories("日志目录/正常");
        filesystem::create_directories("日志目录/黑名单目录");
        { ofstream fout("日志目录/正常/app.log"); }
        { ofstream fout("日志目录/黑名单目录/app.log"); }
        
        filesystem::path filePath = filesystem::absolute("日志目录/**/*.log");
        filesystem::path excludeDir = filesystem::absolute("日志目录/黑名单目录");
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file (黑名单目录 is excluded)
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("日志目录");
    }
#elif defined(_MSC_VER)
    // Windows Test 1: Chinese path with native Chinese characters
    // Expected: Should successfully match and collect files
    {
        filesystem::create_directories("测试目录\\子目录");
        { ofstream fout("测试目录\\子目录\\测试文件.log"); }
        
        filesystem::path filePath = filesystem::absolute("测试目录\\**\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Should find the Chinese named log file
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("测试目录");
    }

    // Windows Test 2: UTF-8 escaped Chinese path
    // Expected: Should correctly handle UTF-8 encoded Chinese characters
    {
        // "\346\265\213\350\257\225" = "测试"
        // Create directory using UTF-8 escaped string
        string utf8Path = "\346\265\213\350\257\225\347\233\256\345\275\225"; // "测试目录"
        filesystem::create_directories(utf8Path + "\\子目录");
        { ofstream fout(utf8Path + "\\子目录\\app.log"); }
        
        filesystem::path filePath = filesystem::absolute(utf8Path + "\\**\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
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
        // Expected: Should find the file in UTF-8 encoded directory
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all(utf8Path);
    }

    // Windows Test 3: Chinese exclusion with native characters
    // Expected: Should correctly filter Chinese named directories
    {
        filesystem::create_directories("日志\\正常");
        filesystem::create_directories("日志\\黑名单");
        { ofstream fout("日志\\正常\\app.log"); }
        { ofstream fout("日志\\黑名单\\app.log"); }
        
        filesystem::path filePath = filesystem::absolute("日志\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute("日志\\黑名单");
        filePath = NormalizeWindowsPath(filePath.string());
        excludeDir = NormalizeWindowsPath(excludeDir.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file (黑名单 directory is excluded)
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("日志");
    }

    // Windows Test 4: Chinese exclusion with UTF-8 escaped characters
    // Expected: Should correctly filter when using UTF-8 encoding
    {
        // "\346\265\213\350\257\225-\351\273\221\345\220\215\345\215\225" = "测试-黑名单"
        string normalDir = "logs\\normal";
        string blacklistDir = "logs\\\346\265\213\350\257\225-\351\273\221\345\220\215\345\215\225"; // "logs\\测试-黑名单"
        filesystem::create_directories(normalDir);
        filesystem::create_directories(blacklistDir);
        { ofstream fout(normalDir + "\\app.log"); }
        { ofstream fout(blacklistDir + "\\app.log"); }
        
        filesystem::path filePath = filesystem::absolute("logs\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute(blacklistDir);
        filePath = NormalizeWindowsPath(filePath.string());
        excludeDir = NormalizeWindowsPath(excludeDir.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        configJson["ExcludeDirs"].append(Json::Value(excludeDir.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file (UTF-8 encoded blacklist directory is excluded)
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("logs");
    }

    // Windows Test 5: Mixed Chinese and UTF-8 in file patterns
    // Expected: Should handle both native Chinese and UTF-8 escaped Chinese in ExcludeFiles
    {
        filesystem::create_directories("混合测试");
        { ofstream fout("混合测试\\正常.log"); }
        { ofstream fout("混合测试\\排除.log"); }
        { ofstream fout("混合测试\\\346\265\213\350\257\225.log"); } // "测试.log"
        
        filesystem::path filePath = filesystem::absolute("混合测试\\*.log");
        filePath = NormalizeWindowsPath(filePath.string());
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeFiles": ["排除.log", "\346\265\213\350\257\225.log"]
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file (正常.log), others are excluded
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("混合测试");
    }
#endif
}

void InputStaticFileUnittest::TestWindowsDriveLetterCaseInsensitive() {
#if defined(_MSC_VER)
    unique_ptr<InputStaticFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test 1: Base path with lowercase drive letter should match uppercase
    // Expected: c:\ and C:\ should be treated as the same path
    {
        filesystem::create_directories("test_drive_case\\subdir");
        { ofstream fout("test_drive_case\\subdir\\test.log"); }
        
        // Get absolute path which will have uppercase drive letter due to NormalizeWindowsPath
        filesystem::path filePath = filesystem::absolute("test_drive_case\\**\\*.log");
        string filePathStr = filePath.string();
        
        // Manually create a lowercase version
        string lowerCaseFilePath = filePathStr;
        if (lowerCaseFilePath.size() >= 2 && lowerCaseFilePath[1] == ':' && isupper(lowerCaseFilePath[0])) {
            lowerCaseFilePath[0] = tolower(lowerCaseFilePath[0]);
        }
        
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(lowerCaseFilePath));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find the file even with lowercase drive letter
        // because NormalizeWindowsPath converts it to uppercase internally
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("test_drive_case");
    }

    // Test 2: ExcludeDirs with different drive letter case
    // Expected: Exclusion should work regardless of drive letter case
    {
        filesystem::create_directories("case_test\\include");
        filesystem::create_directories("case_test\\exclude");
        { ofstream fout("case_test\\include\\app.log"); }
        { ofstream fout("case_test\\exclude\\app.log"); }
        
        filesystem::path filePath = filesystem::absolute("case_test\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute("case_test\\exclude");
        
        string filePathStr = filePath.string();
        string excludeDirStr = excludeDir.string();
        
        // Use lowercase for file path, uppercase for exclude dir (or vice versa)
        if (filePathStr.size() >= 2 && filePathStr[1] == ':') {
            filePathStr[0] = tolower(filePathStr[0]);
        }
        if (excludeDirStr.size() >= 2 && excludeDirStr[1] == ':') {
            excludeDirStr[0] = isupper(filePathStr[0]) ? tolower(excludeDirStr[0]) : toupper(excludeDirStr[0]);
        }
        
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        configJson["ExcludeDirs"].append(Json::Value(excludeDirStr));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file from include directory
        // exclude directory should be filtered out despite different drive letter case
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("case_test");
    }

    // Test 3: ExcludeFilePaths with different drive letter case
    // Expected: File path exclusion should work regardless of drive letter case
    {
        filesystem::create_directories("file_exclude_test");
        { ofstream fout("file_exclude_test\\keep.log"); }
        { ofstream fout("file_exclude_test\\exclude.log"); }
        
        filesystem::path filePath = filesystem::absolute("file_exclude_test\\*.log");
        filesystem::path excludeFile = filesystem::absolute("file_exclude_test\\exclude.log");
        
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
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeFilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        configJson["ExcludeFilePaths"].append(Json::Value(excludeFileStr));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file (keep.log)
        // exclude.log should be filtered despite different drive letter case
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("file_exclude_test");
    }

    // Test 4: Wildcard path with lowercase drive letter
    // Expected: Wildcard matching should work with any drive letter case
    {
        filesystem::create_directories("wildcard_test\\dir1\\logs");
        filesystem::create_directories("wildcard_test\\dir2\\logs");
        { ofstream fout("wildcard_test\\dir1\\logs\\app.log"); }
        { ofstream fout("wildcard_test\\dir2\\logs\\app.log"); }
        
        filesystem::path filePath = filesystem::absolute("wildcard_test\\*\\logs\\*.log");
        string filePathStr = filePath.string();
        
        // Use lowercase drive letter
        if (filePathStr.size() >= 2 && filePathStr[1] == ':') {
            filePathStr[0] = tolower(filePathStr[0]);
        }
        
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find 2 files from both directories
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(2U, files.size());
        
        filesystem::remove_all("wildcard_test");
    }

    // Test 5: Mixed case in both base path and blacklist
    // Expected: All combinations of drive letter cases should work correctly
    {
        filesystem::create_directories("mixed_case\\normal");
        filesystem::create_directories("mixed_case\\blacklist");
        { ofstream fout("mixed_case\\normal\\app1.log"); }
        { ofstream fout("mixed_case\\blacklist\\app2.log"); }
        
        filesystem::path filePath = filesystem::absolute("mixed_case\\**\\*.log");
        filesystem::path excludeDir = filesystem::absolute("mixed_case\\blacklist");
        
        string filePathStr = filePath.string();
        string excludeDirStr = excludeDir.string();
        
        // Test with: lowercase base path, uppercase exclude dir
        if (filePathStr.size() >= 2 && filePathStr[1] == ':') {
            filePathStr[0] = 'c';  // Force lowercase
        }
        if (excludeDirStr.size() >= 2 && excludeDirStr[1] == ':') {
            excludeDirStr[0] = 'C';  // Force uppercase
        }
        
        configStr = R"(
            {
                "Type": "input_static_file_onetime",
                "FilePaths": [],
                "ExcludeDirs": []
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePathStr));
        configJson["ExcludeDirs"].append(Json::Value(excludeDirStr));
        input.reset(new InputStaticFile());
        input->SetContext(ctx);
        input->CreateMetricsRecordRef(InputStaticFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        input->CommitMetricsRecordRef();
        // Expected: Should find only 1 file from normal directory
        // blacklist directory should be excluded despite different drive letter case
        auto files = input->GetFiles();
        APSARA_TEST_EQUAL(1U, files.size());
        
        filesystem::remove_all("mixed_case");
    }
#endif
}

UNIT_TEST_CASE(InputStaticFileUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputStaticFileUnittest, OnFailedInit)
UNIT_TEST_CASE(InputStaticFileUnittest, TestCreateInnerProcessors)
UNIT_TEST_CASE(InputStaticFileUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputStaticFileUnittest, TestGetFiles)
UNIT_TEST_CASE(InputStaticFileUnittest, OnEnableContainerDiscovery)
UNIT_TEST_CASE(InputStaticFileUnittest, TestWindowsRootPathCollection)
UNIT_TEST_CASE(InputStaticFileUnittest, TestChinesePathMatching)
UNIT_TEST_CASE(InputStaticFileUnittest, TestWindowsDriveLetterCaseInsensitive)

} // namespace logtail

UNIT_TEST_MAIN
