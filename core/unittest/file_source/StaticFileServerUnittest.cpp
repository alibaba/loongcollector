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
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
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

protected:
    static void SetUpTestCase() {
        PluginRegistry::GetInstance()->LoadPlugins();
        // sManager->mCheckpointRootPath = filesystem::path("./input_static_file");
    }

    void SetUp() override { filesystem::create_directories(sManager->mCheckpointRootPath); }

    void TearDown() override {
        // sServer->Clear();
        // sManager->ClearUnusedCheckpoints();
        // sManager->mInputCheckpointMap.clear();
        // filesystem::remove_all(sManager->mCheckpointRootPath);
    }

private:
    static InputStaticFileCheckpointManager* sManager;
    static StaticFileServer* sServer;
};

InputStaticFileCheckpointManager* StaticFileServerUnittest::sManager = InputStaticFileCheckpointManager::GetInstance();
StaticFileServer* StaticFileServerUnittest::sServer = StaticFileServer::GetInstance();

void StaticFileServerUnittest::TestGetNextAvailableReader() const {
    // // prepare test log
    // filesystem::create_directories("test_logs");
    // vector<filesystem::path> files{
    //     "./test_logs/test_file_1.log", "./test_logs/test_file_2.log", "./test_logs/test_file_3.log"};
    // vector<string> contents{string(2000, 'a') + "\n", string(200, 'b') + "\n", string(1000, 'c') + "\n"};
    // vector<FileFingerprint> fingerprints;
    // for (size_t i = 0; i < files.size(); ++i) {
    //     {
    //         ofstream fout(files[i]);
    //         fout << contents[i];
    //     }
    //     auto& item = fingerprints.emplace_back();
    //     item.mFilePath = files[i];
    //     item.mDevInode = GetFileDevInode(files[i].string());
    //     item.mSignatureSize = contents[i].size() > 1024 ? 1024 : contents[i].size();
    //     item.mSignatureHash
    //         = HashSignatureString(contents[i].substr(0, item.mSignatureSize).c_str(), item.mSignatureSize);
    // }

    // // build input
    // CollectionPipeline p;
    // p.mName = "test_config";
    // p.mPluginID.store(0);
    // CollectionPipelineContext ctx;
    // ctx.SetConfigName("test_config");
    // ctx.SetPipeline(p);

    // filesystem::path filePath = filesystem::absolute("./test_logs/*.log");
    // string configStr = R"(
    //     {
    //         "Type": "input_static_file_onetime",
    //         "FilePaths": []
    //     }
    // )";
    // string errorMsg;
    // Json::Value configJson, optionalGoPipeline;
    // (ParseJsonTable(configStr, configJson, errorMsg));
    // configJson["FilePaths"].append(Json::Value(filePath.string()));
    // InputStaticFile input;
    // input.SetContext(ctx);
    // input.CreateMetricsRecordRef(InputFile::sName, "1");
    // input.Init(configJson, optionalGoPipeline);
    // input.CommitMetricsRecordRef();
    // input.Start();

    // vector<filesystem::path> cptFiles;
    // for (const auto& item : sManager->mInputCheckpointMap.at(make_pair("test_config", 0)).mFileCheckpoints) {
    //     cptFiles.push_back(item.mFilePath);
    // }

    // sServer->UpdateInputs();

    // {
    //     // file 1 existed
    //     APSARA_TEST_NOT_EQUAL(nullptr, sServer->GetNextAvailableReader("test_config", 0));
    //     auto const& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
    //     APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
    //     APSARA_TEST_EQUAL(FileStatus::WAITING, cpt.mFileCheckpoints[0].mStatus);
    // }
    // sManager->UpdateCurrentFileCheckpoint("test_config", 0, 2001, 2001);
    // {
    //     // file 2 not existed && file 3 signature changed
    //     filesystem::remove(cptFiles[1]);
    //     {
    //         ofstream fout(cptFiles[2]);
    //         fout << string(10, 'd') << endl;
    //     }
    //     APSARA_TEST_EQUAL(nullptr, sServer->GetNextAvailableReader("test_config", 0));
    //     APSARA_TEST_EQUAL(1U, sServer->mDeletedInputs.size());
    //     APSARA_TEST_NOT_EQUAL(sServer->mDeletedInputs.end(), sServer->mDeletedInputs.find(make_pair("test_config", 0)));
    //     auto const& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config", 0));
    //     APSARA_TEST_EQUAL(StaticFileReadingStatus::FINISHED, cpt.mStatus);
    //     APSARA_TEST_EQUAL(FileStatus::FINISHED, cpt.mFileCheckpoints[0].mStatus);
    //     APSARA_TEST_EQUAL(FileStatus::ABORT, cpt.mFileCheckpoints[1].mStatus);
    //     APSARA_TEST_EQUAL(FileStatus::ABORT, cpt.mFileCheckpoints[2].mStatus);
    // }

    // sServer->UpdateInputs();
    // APSARA_TEST_EQUAL(0U, sServer->mPipelineNameReadersMap.size());
    // APSARA_TEST_EQUAL(0U, sServer->mDeletedInputs.size());

    // input.Stop(true);
    // filesystem::remove_all("test_logs");
}

void StaticFileServerUnittest::TestUpdateInputs() const {
    // // new config
    // sServer->AddInput("test_config_1", 0, nullopt, nullptr, nullptr, nullptr, nullptr, nullptr);
    // sServer->AddInput("test_config_2", 0, nullopt, nullptr, nullptr, nullptr, nullptr, nullptr);
    // sServer->AddInput("test_config_2", 1, nullopt, nullptr, nullptr, nullptr, nullptr, nullptr);
    // sServer->UpdateInputs();
    // APSARA_TEST_EQUAL(3U, sServer->mPipelineNameReadersMap.size());
    // APSARA_TEST_EQUAL(1U, sServer->mPipelineNameReadersMap.count("test_config_1"));
    // APSARA_TEST_EQUAL(2U, sServer->mPipelineNameReadersMap.count("test_config_2"));
    // APSARA_TEST_TRUE(sServer->mAddedInputs.empty());
    // APSARA_TEST_TRUE(sServer->HasRegisteredPlugins());

    // // update config
    // sServer->RemoveInput("test_config_2", 0);
    // sServer->RemoveInput("test_config_2", 1);
    // sServer->AddInput("test_config_2", 0, nullopt, nullptr, nullptr, nullptr, nullptr, nullptr);
    // sServer->UpdateInputs();
    // APSARA_TEST_EQUAL(2U, sServer->mPipelineNameReadersMap.size());
    // APSARA_TEST_EQUAL(1U, sServer->mPipelineNameReadersMap.count("test_config_1"));
    // APSARA_TEST_EQUAL(1U, sServer->mPipelineNameReadersMap.count("test_config_2"));
    // APSARA_TEST_TRUE(sServer->mDeletedInputs.empty());
    // APSARA_TEST_TRUE(sServer->mAddedInputs.empty());
    // APSARA_TEST_TRUE(sServer->HasRegisteredPlugins());

    // // delete config
    // sServer->RemoveInput("test_config_1", 0);
    // sServer->RemoveInput("test_config_2", 0);
    // sServer->UpdateInputs();
    // APSARA_TEST_EQUAL(0U, sServer->mPipelineNameReadersMap.size());
    // APSARA_TEST_TRUE(sServer->mDeletedInputs.empty());
    // APSARA_TEST_FALSE(sServer->HasRegisteredPlugins());
}

void StaticFileServerUnittest::TestClearUnusedCheckpoints() const {
    // INT32_FLAG(unused_checkpoints_clear_interval_sec) = 0;

    // { ofstream fout(sManager->mCheckpointRootPath / "test_config@0.json"); }
    // sManager->GetAllCheckpointFileNames();
    // APSARA_TEST_FALSE(sManager->mCheckpointFileNamesOnInit.empty());

    // sServer->ClearUnusedCheckpoints();
    // APSARA_TEST_TRUE(sServer->mIsUnusedCheckpointsCleared);
    // APSARA_TEST_TRUE(sManager->mCheckpointFileNamesOnInit.empty());

    // sServer->ClearUnusedCheckpoints();
    // INT32_FLAG(unused_checkpoints_clear_interval_sec) = 600;
}

UNIT_TEST_CASE(StaticFileServerUnittest, TestGetNextAvailableReader)
UNIT_TEST_CASE(StaticFileServerUnittest, TestUpdateInputs)
UNIT_TEST_CASE(StaticFileServerUnittest, TestClearUnusedCheckpoints)

} // namespace logtail

UNIT_TEST_MAIN
