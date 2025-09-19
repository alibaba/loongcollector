// Copyright 2025 iLogtail Authors
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

#include "common/JsonUtil.h"
#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputStaticFileCheckpointManagerUnittest : public testing::Test {
public:
    void TestUpdateCheckpointMap() const;
    void TestUpdateCheckpoint() const;
    void TestCheckpointFileNames() const;
    void TestDumpCheckpoints() const;
    void TestInvalidCheckpointFile() const;

protected:
    void SetUp() override {
        sManager = InputStaticFileCheckpointManager::GetInstance();
        sManager->mCheckpointRootPath = filesystem::path("./input_static_file");
        filesystem::create_directories(sManager->mCheckpointRootPath);
    }

    void TearDown() override {
        sManager->ClearUnusedCheckpoints();
        sManager->mInputCheckpointMap.clear();
        filesystem::remove_all(sManager->mCheckpointRootPath);
    }

private:
    InputStaticFileCheckpointManager* sManager;
};

void InputStaticFileCheckpointManagerUnittest::TestUpdateCheckpointMap() const {
    // prepare logs
    filesystem::create_directories("test_logs");
    vector<filesystem::path> files{"./test_logs/test_file_1.log", "./test_logs/test_file_2.log"};
    vector<string> contents{string(2000, 'a') + "\n", string(200, 'b') + "\n"};
    vector<FileFingerprint> fingerprints;
    for (size_t i = 0; i < files.size(); ++i) {
        {
            ofstream fout(files[i], std::ios_base::binary);
            fout << contents[i];
        }
        auto& item = fingerprints.emplace_back();
        item.mFilePath = files[i];
        item.mDevInode = GetFileDevInode(files[i].string());
        item.mSignatureSize = contents[i].size() > 1024 ? 1024 : contents[i].size();
        item.mSignatureHash
            = HashSignatureString(contents[i].substr(0, item.mSignatureSize).c_str(), item.mSignatureSize);
    }

    // prepare checkpoint files
    { ofstream fout(sManager->mCheckpointRootPath / "test_config_2@0.json", std::ios_base::binary); }
    {
        ofstream fout(sManager->mCheckpointRootPath / "test_config_3@0.json", std::ios_base::binary);
        string cptStr = R"({
        "config_name" : "test_config_3",
        "current_file_index" : 0,
        "file_count" : 1,
        "start_time" : 1739349980,
        "expire_time" : 1739349981,
        "files" : 
        [
            {
                "dev" : 2081,
                "filepath" : "./test_logs/test_file_3.log",
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

    // create config
    {
        // new config
        optional<vector<filesystem::path>> filesOpt({files[0]});
        APSARA_TEST_TRUE(sManager->CreateCheckpoint("test_config_1", 0, filesOpt));
        APSARA_TEST_EQUAL(1U, sManager->mInputCheckpointMap.size());
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL("test_config_1", cpt.GetConfigName());
        APSARA_TEST_EQUAL(0U, cpt.GetInputIndex());
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(1U, cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL(fingerprints[0].mFilePath, cpt.mFileCheckpoints[0].mFilePath);
        APSARA_TEST_EQUAL(fingerprints[0].mDevInode, cpt.mFileCheckpoints[0].mDevInode);
        APSARA_TEST_EQUAL(fingerprints[0].mSignatureHash, cpt.mFileCheckpoints[0].mSignatureHash);
        APSARA_TEST_EQUAL(fingerprints[0].mSignatureSize, cpt.mFileCheckpoints[0].mSignatureSize);
        APSARA_TEST_EQUAL(0U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(0U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::WAITING, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(0, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(0, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_1@0.json"));
    }
    {
        // old config, but changed on restart
        optional<vector<filesystem::path>> filesOpt({files[1]});
        APSARA_TEST_TRUE(sManager->CreateCheckpoint("test_config_2", 0, filesOpt));
        APSARA_TEST_EQUAL(2U, sManager->mInputCheckpointMap.size());
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_2", 0));
        APSARA_TEST_EQUAL("test_config_2", cpt.GetConfigName());
        APSARA_TEST_EQUAL(0U, cpt.GetInputIndex());
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(1U, cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL(fingerprints[1].mFilePath, cpt.mFileCheckpoints[0].mFilePath);
        APSARA_TEST_EQUAL(fingerprints[1].mDevInode, cpt.mFileCheckpoints[0].mDevInode);
        APSARA_TEST_EQUAL(fingerprints[1].mSignatureHash, cpt.mFileCheckpoints[0].mSignatureHash);
        APSARA_TEST_EQUAL(fingerprints[1].mSignatureSize, cpt.mFileCheckpoints[0].mSignatureSize);
        APSARA_TEST_EQUAL(0U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(0U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::WAITING, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(0, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(0, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_2@0.json"));
        APSARA_TEST_NOT_EQUAL(0U, std::filesystem::file_size(sManager->mCheckpointRootPath / "test_config_2@0.json"));
    }
    {
        // old config, no change
        APSARA_TEST_TRUE(sManager->CreateCheckpoint("test_config_3", 0));
        APSARA_TEST_EQUAL(3U, sManager->mInputCheckpointMap.size());
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_3", 0));
        APSARA_TEST_EQUAL("test_config_3", cpt.GetConfigName());
        APSARA_TEST_EQUAL(0U, cpt.GetInputIndex());
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(1U, cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL("./test_logs/test_file_3.log", cpt.mFileCheckpoints[0].mFilePath);
        APSARA_TEST_EQUAL(DevInode(2081, 79956083), cpt.mFileCheckpoints[0].mDevInode);
        APSARA_TEST_EQUAL(5407334769256465540U, cpt.mFileCheckpoints[0].mSignatureHash);
        APSARA_TEST_EQUAL(500U, cpt.mFileCheckpoints[0].mSignatureSize);
        APSARA_TEST_EQUAL(100U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(500U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::READING, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(1739349980, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(1739349981, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_3@0.json"));
    }
    {
        // old config, no change, but checkpoint file not existed
        APSARA_TEST_FALSE(sManager->CreateCheckpoint("test_config_4", 0));
        APSARA_TEST_EQUAL(4U, sManager->mInputCheckpointMap.size());
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_4", 0));
        APSARA_TEST_EQUAL("test_config_4", cpt.GetConfigName());
        APSARA_TEST_EQUAL(0U, cpt.GetInputIndex());
        APSARA_TEST_EQUAL(StaticFileReadingStatus::ABORT, cpt.mStatus);
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_TRUE(cpt.mFileCheckpoints.empty());
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_4@0.json"));
    }

    // delete config
    {
        // checkpoint file existed
        APSARA_TEST_TRUE(sManager->DeleteCheckpoint("test_config_1", 0));
        APSARA_TEST_EQUAL(3U, sManager->mInputCheckpointMap.size());
        APSARA_TEST_EQUAL(sManager->mInputCheckpointMap.end(),
                          sManager->mInputCheckpointMap.find(make_pair("test_config_1", 0)));
        APSARA_TEST_FALSE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_1@0.json"));
    }
    {
        // checkpoint file not existed
        filesystem::remove(sManager->mCheckpointRootPath / "test_config_2@0.json");
        APSARA_TEST_TRUE(sManager->DeleteCheckpoint("test_config_2", 0));
        APSARA_TEST_EQUAL(2U, sManager->mInputCheckpointMap.size());
        APSARA_TEST_EQUAL(sManager->mInputCheckpointMap.end(),
                          sManager->mInputCheckpointMap.find(make_pair("test_config_2", 0)));
    }
    filesystem::remove_all("test_logs");
}

void InputStaticFileCheckpointManagerUnittest::TestUpdateCheckpoint() const {
    // prepare logs
    filesystem::create_directories("test_logs");
    vector<filesystem::path> files{
        "./test_logs/test_file_1.log", "./test_logs/test_file_2.log", "./test_logs/test_file_3.log"};
    vector<string> contents{string(2000, 'a') + "\n", string(200, 'b') + "\n", string(500, 'c') + "\n"};
    vector<FileFingerprint> fingerprints;
    for (size_t i = 0; i < files.size(); ++i) {
        {
            ofstream fout(files[i], std::ios_base::binary);
            fout << contents[i];
        }
        auto& item = fingerprints.emplace_back();
        item.mFilePath = files[i];
        item.mDevInode = GetFileDevInode(files[i].string());
        item.mSignatureSize = contents[i].size() > 1024 ? 1024 : contents[i].size();
        item.mSignatureHash
            = HashSignatureString(contents[i].substr(0, item.mSignatureSize).c_str(), item.mSignatureSize);
    }

    sManager->CreateCheckpoint("test_config_1", 0, files);
    sManager->CreateCheckpoint("test_config_2", 0, optional<vector<filesystem::path>>({files[0]}));
    {
        // from waiting to reading
        APSARA_TEST_TRUE(sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 1000, 2001));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(1000U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(2001U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::READING, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_1@0.json"));
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mOffset, cptLoaded.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSize, cptLoaded.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mStatus, cptLoaded.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mStartTime, cptLoaded.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mLastUpdateTime, cptLoaded.mFileCheckpoints[0].mLastUpdateTime);
        FileFingerprint fp;
        APSARA_TEST_TRUE(sManager->GetCurrentFileFingerprint("test_config_1", 0, &fp));
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mFilePath, fp.mFilePath);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mDevInode, fp.mDevInode);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSignatureHash, fp.mSignatureHash);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSignatureSize, fp.mSignatureSize);
    }
    {
        // maintain reading
        APSARA_TEST_TRUE(sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 1500, 2001));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(0U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(1500U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(2001U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::READING, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_1@0.json"));
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(1000U, cptLoaded.mFileCheckpoints[0].mOffset);
        FileFingerprint fp;
        APSARA_TEST_TRUE(sManager->GetCurrentFileFingerprint("test_config_1", 0, &fp));
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mFilePath, fp.mFilePath);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mDevInode, fp.mDevInode);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSignatureHash, fp.mSignatureHash);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSignatureSize, fp.mSignatureSize);
    }
    {
        // from reading to finished
        APSARA_TEST_TRUE(sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 2001, 2001));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(1U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(2001U, cpt.mFileCheckpoints[0].mOffset);
        APSARA_TEST_EQUAL(2001U, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(FileStatus::FINISHED, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_TRUE(filesystem::exists(sManager->mCheckpointRootPath / "test_config_1@0.json"));
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mSize, cptLoaded.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mStatus, cptLoaded.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mStartTime, cptLoaded.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[0].mLastUpdateTime, cptLoaded.mFileCheckpoints[0].mLastUpdateTime);
        FileFingerprint fp;
        APSARA_TEST_TRUE(sManager->GetCurrentFileFingerprint("test_config_1", 0, &fp));
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[1].mFilePath, fp.mFilePath);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[1].mDevInode, fp.mDevInode);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[1].mSignatureHash, fp.mSignatureHash);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[1].mSignatureSize, fp.mSignatureSize);
    }
    {
        // from waiting to abort
        APSARA_TEST_TRUE(sManager->InvalidateCurrentFileCheckpoint("test_config_1", 0));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(2U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::RUNNING, cpt.mStatus);
        APSARA_TEST_EQUAL(FileStatus::ABORT, cpt.mFileCheckpoints[1].mStatus);
        APSARA_TEST_NOT_EQUAL(0, cpt.mFileCheckpoints[1].mLastUpdateTime);
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[1].mStatus, cptLoaded.mFileCheckpoints[1].mStatus);
        FileFingerprint fp;
        APSARA_TEST_TRUE(sManager->GetCurrentFileFingerprint("test_config_1", 0, &fp));
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[2].mFilePath, fp.mFilePath);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[2].mDevInode, fp.mDevInode);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[2].mSignatureHash, fp.mSignatureHash);
        APSARA_TEST_EQUAL(cpt.mFileCheckpoints[2].mSignatureSize, fp.mSignatureSize);
    }
    {
        // job finished with last file finished
        APSARA_TEST_TRUE(sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 501, 501));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(3U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::FINISHED, cpt.mStatus);
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(cpt.mStatus, cptLoaded.mStatus);
        FileFingerprint fp;
        APSARA_TEST_FALSE(sManager->GetCurrentFileFingerprint("test_config_1", 0, &fp));
    }
    {
        // job finished with last file abort
        APSARA_TEST_TRUE(sManager->InvalidateCurrentFileCheckpoint("test_config_2", 0));
        const auto& cpt = sManager->mInputCheckpointMap.at(make_pair("test_config_2", 0));
        APSARA_TEST_EQUAL(1U, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(StaticFileReadingStatus::FINISHED, cpt.mStatus);
        InputStaticFileCheckpoint cptLoaded;
        sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cptLoaded);
        APSARA_TEST_EQUAL(cpt.mStatus, cptLoaded.mStatus);
        FileFingerprint fp;
        APSARA_TEST_FALSE(sManager->GetCurrentFileFingerprint("test_config_2", 0, &fp));
    }
    filesystem::remove_all("test_logs");
}

void InputStaticFileCheckpointManagerUnittest::TestCheckpointFileNames() const {
    // valid checkpoint root path
    filesystem::create_directories(sManager->mCheckpointRootPath / "dir");
    { ofstream fout(sManager->mCheckpointRootPath / "unsupported_extenstion.yaml", std::ios_base::binary); }
    { ofstream fout(sManager->mCheckpointRootPath / "invalid_filename.json", std::ios_base::binary); }
    { ofstream fout(sManager->mCheckpointRootPath / "test_config@invalid_idx.json", std::ios_base::binary); }
    {
        ofstream fout(sManager->mCheckpointRootPath / "test_config@18446744073709551614000.json",
                      std::ios_base::binary);
    }
    { ofstream fout(sManager->mCheckpointRootPath / "test_config@0.json", std::ios_base::binary); }
    { ofstream fout(sManager->mCheckpointRootPath / "test_config@1.json", std::ios_base::binary); }
    sManager->GetAllCheckpointFileNames();
    APSARA_TEST_EQUAL(2U, sManager->mCheckpointFileNamesOnInit.size());
    APSARA_TEST_NOT_EQUAL(sManager->mCheckpointFileNamesOnInit.end(),
                          sManager->mCheckpointFileNamesOnInit.find(make_pair("test_config", 0)));
    APSARA_TEST_NOT_EQUAL(sManager->mCheckpointFileNamesOnInit.end(),
                          sManager->mCheckpointFileNamesOnInit.find(make_pair("test_config", 1)));

    filesystem::remove(sManager->mCheckpointRootPath / "test_config@0.json");
    sManager->ClearUnusedCheckpoints();
    APSARA_TEST_TRUE(sManager->mInputCheckpointMap.empty());
    APSARA_TEST_FALSE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@0.json"));
    APSARA_TEST_FALSE(filesystem::exists(sManager->mCheckpointRootPath / "test_config@1.json"));

    // no checkpoint root path
    filesystem::remove_all(sManager->mCheckpointRootPath);
    EXPECT_NO_THROW(sManager->GetAllCheckpointFileNames());

    // invalid checkpoint root path
    { ofstream fout(sManager->mCheckpointRootPath, std::ios_base::binary); }
    EXPECT_NO_THROW(sManager->GetAllCheckpointFileNames());
}

void InputStaticFileCheckpointManagerUnittest::TestDumpCheckpoints() const {
    filesystem::create_directories("test_logs");
    vector<filesystem::path> files{"./test_logs/test_file_1.log",
                                   "./test_logs/test_file_2.log",
                                   "./test_logs/test_file_3.log",
                                   "./test_logs/test_file_4.log"};
    {
        ofstream fout(files[0], std::ios_base::binary);
        fout << string(2000, 'a') << endl;
    }
    {
        ofstream fout(files[1], std::ios_base::binary);
        fout << string(100, 'b') << endl;
    }
    {
        ofstream fout(files[2], std::ios_base::binary);
        fout << string(500, 'c') << endl;
    }
    {
        ofstream fout(files[3], std::ios_base::binary);
        fout << string(1500, 'd') << endl;
    }
    // job_1 running: file 1 finished, file 2 abort, file 3 reading, file 4 waiting
    sManager->CreateCheckpoint("test_config_1", 0, files);
    sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 2001, 2001);
    sManager->InvalidateCurrentFileCheckpoint("test_config_1", 0);
    sManager->UpdateCurrentFileCheckpoint("test_config_1", 0, 100, 501);
    // job_2 finished
    optional<vector<filesystem::path>> filesOpt({files[0]});
    sManager->CreateCheckpoint("test_config_2", 0, filesOpt);
    sManager->UpdateCurrentFileCheckpoint("test_config_2", 0, 2001, 2001);

    sManager->DumpAllCheckpointFiles();
    {
        InputStaticFileCheckpoint cpt;
        APSARA_TEST_TRUE(sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_1@0.json", &cpt));
        const auto& expectedCpt = sManager->mInputCheckpointMap.at(make_pair("test_config_1", 0));
        APSARA_TEST_EQUAL(expectedCpt.GetConfigName(), cpt.GetConfigName());
        APSARA_TEST_EQUAL(expectedCpt.GetInputIndex(), cpt.GetInputIndex());
        APSARA_TEST_EQUAL(expectedCpt.mStatus, cpt.mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mCurrentFileIndex, cpt.mCurrentFileIndex);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints.size(), cpt.mFileCheckpoints.size());
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[0].mFilePath, cpt.mFileCheckpoints[0].mFilePath);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[0].mStatus, cpt.mFileCheckpoints[0].mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[0].mSize, cpt.mFileCheckpoints[0].mSize);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[0].mStartTime, cpt.mFileCheckpoints[0].mStartTime);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[0].mLastUpdateTime, cpt.mFileCheckpoints[0].mLastUpdateTime);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[1].mFilePath, cpt.mFileCheckpoints[1].mFilePath);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[1].mStatus, cpt.mFileCheckpoints[1].mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[1].mLastUpdateTime, cpt.mFileCheckpoints[1].mLastUpdateTime);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mFilePath, cpt.mFileCheckpoints[2].mFilePath);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mStatus, cpt.mFileCheckpoints[2].mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mDevInode, cpt.mFileCheckpoints[2].mDevInode);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mSignatureHash, cpt.mFileCheckpoints[2].mSignatureHash);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mSignatureSize, cpt.mFileCheckpoints[2].mSignatureSize);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mOffset, cpt.mFileCheckpoints[2].mOffset);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mSize, cpt.mFileCheckpoints[2].mSize);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mStartTime, cpt.mFileCheckpoints[2].mStartTime);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[2].mLastUpdateTime, cpt.mFileCheckpoints[2].mLastUpdateTime);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[3].mFilePath, cpt.mFileCheckpoints[3].mFilePath);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[3].mStatus, cpt.mFileCheckpoints[3].mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[3].mDevInode, cpt.mFileCheckpoints[3].mDevInode);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[3].mSignatureHash, cpt.mFileCheckpoints[3].mSignatureHash);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints[3].mSignatureSize, cpt.mFileCheckpoints[3].mSignatureSize);
    }
    {
        InputStaticFileCheckpoint cpt;
        APSARA_TEST_TRUE(sManager->LoadCheckpointFile(sManager->mCheckpointRootPath / "test_config_2@0.json", &cpt));
        const auto& expectedCpt = sManager->mInputCheckpointMap.at(make_pair("test_config_2", 0));
        APSARA_TEST_EQUAL(expectedCpt.GetConfigName(), cpt.GetConfigName());
        APSARA_TEST_EQUAL(expectedCpt.GetInputIndex(), cpt.GetInputIndex());
        APSARA_TEST_EQUAL(expectedCpt.mStatus, cpt.mStatus);
        APSARA_TEST_EQUAL(expectedCpt.mFileCheckpoints.size(), cpt.mFileCheckpoints.size());
    }
    filesystem::remove_all("test_logs");
}

void InputStaticFileCheckpointManagerUnittest::TestInvalidCheckpointFile() const {
    filesystem::path cptPath = sManager->mCheckpointRootPath / "test_config@0.json";
    InputStaticFileCheckpoint cpt;
    // no file
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    // not regular file
    filesystem::create_directories(cptPath);
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    filesystem::remove_all(cptPath);
    // empty file
    { ofstream fout(cptPath, std::ios_base::binary); }
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    // invalid json
    {
        ofstream fout(cptPath, std::ios_base::binary);
        fout << "{]";
    }
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    // not json object
    {
        ofstream fout(cptPath, std::ios_base::binary);
        fout << "[]";
    }
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));

    string validCptStr = R"({
            "config_name" : "test_config_1",
            "current_file_index" : 2,
            "file_count" : 4,
            "start_time" : 1739349980,
            "expire_time" : 1739349981,
            "files" : 
            [
                {
                    "filepath" : "./test_logs/test_file_1.log",
                    "finish_time" : 1739349980,
                    "size" : 2000,
                    "start_time" : 1739349980,
                    "status" : "finished"
                },
                {
                    "abort_time" : 1739349980,
                    "filepath" : "./test_logs/test_file_2.log",
                    "status" : "abort"
                },
                {
                    "dev" : 2081,
                    "filepath" : "./test_logs/test_file_3.log",
                    "inode" : 79956083,
                    "last_read_time" : 1739349980,
                    "offset" : 100,
                    "sig_hash" : 5407334769256465540,
                    "sig_size" : 501,
                    "size" : 500,
                    "start_time" : 1739349980,
                    "status" : "reading"
                },
                {
                    "dev" : 2081,
                    "filepath" : "./test_logs/test_file_4.log",
                    "inode" : 79956086,
                    "sig_hash" : 2508160163735440748,
                    "sig_size" : 1024,
                    "status" : "waiting"
                }
            ],
            "input_index" : 0,
            "status" : "running"
        }
    )";
    string errorMsg;
    Json::Value validCptJson;
    ParseJsonTable(validCptStr, validCptJson, errorMsg);
    {
        // invalid job key
        vector<string> keys{"config_name", "input_index", "status", "current_file_index", "file_count", "files"};
        for (const auto& key : keys) {
            auto copy = validCptJson;
            copy.removeMember(key);
            {
                ofstream fout(cptPath, std::ios_base::binary);
                fout << copy.toStyledString();
            }
            APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        }
        {
            auto copy = validCptJson;
            copy["status"] = "unknown";
            ofstream fout(cptPath, std::ios_base::binary);
            fout << copy.toStyledString();
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        {
            auto copy = validCptJson;
            copy["files"] = Json::objectValue;
            ofstream fout(cptPath, std::ios_base::binary);
            fout << copy.toStyledString();
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        {
            auto copy = validCptJson;
            copy["files"] = Json::arrayValue;
            ofstream fout(cptPath, std::ios_base::binary);
            fout << copy.toStyledString();
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        {
            auto copy = validCptJson;
            copy["files"][0] = Json::arrayValue;
            ofstream fout(cptPath, std::ios_base::binary);
            fout << copy.toStyledString();
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    }
    {
        // common invalid file key
        {
            vector<string> keys{"filepath", "status"};
            for (const auto& key : keys) {
                auto copy = validCptJson;
                copy["files"][0].removeMember(key);
                {
                    ofstream fout(cptPath, std::ios_base::binary);
                    fout << copy.toStyledString();
                }
                APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
            }
        }
        {
            auto copy = validCptJson;
            copy["files"][0]["status"] = "unknown";
            {
                ofstream fout(cptPath, std::ios_base::binary);
                fout << copy.toStyledString();
            }
        }
        APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    }
    {
        // invalid finished file key
        vector<string> keys{"size", "start_time", "finish_time"};
        for (const auto& key : keys) {
            auto copy = validCptJson;
            copy["files"][0].removeMember(key);
            {
                ofstream fout(cptPath, std::ios_base::binary);
                fout << copy.toStyledString();
            }
            APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        }
    }
    {
        // invalid abort file key
        auto copy = validCptJson;
        copy["files"][1].removeMember("abort_time");
        ofstream fout(cptPath, std::ios_base::binary);
        fout << copy.toStyledString();
    }
    APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
    {
        // invalid reading file key
        vector<string> keys{"dev", "inode", "sig_hash", "sig_size", "size", "offset", "start_time", "last_read_time"};
        for (const auto& key : keys) {
            auto copy = validCptJson;
            copy["files"][2].removeMember(key);
            {
                ofstream fout(cptPath, std::ios_base::binary);
                fout << copy.toStyledString();
            }
            APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        }
    }
    {
        // invalid waiting file key
        vector<string> keys{"dev", "inode", "sig_hash", "sig_size"};
        for (const auto& key : keys) {
            auto copy = validCptJson;
            copy["files"][3].removeMember(key);
            {
                ofstream fout(cptPath, std::ios_base::binary);
                fout << copy.toStyledString();
            }
            APSARA_TEST_FALSE(sManager->LoadCheckpointFile(cptPath, &cpt));
        }
    }
}

UNIT_TEST_CASE(InputStaticFileCheckpointManagerUnittest, TestUpdateCheckpointMap)
UNIT_TEST_CASE(InputStaticFileCheckpointManagerUnittest, TestUpdateCheckpoint)
UNIT_TEST_CASE(InputStaticFileCheckpointManagerUnittest, TestCheckpointFileNames)
UNIT_TEST_CASE(InputStaticFileCheckpointManagerUnittest, TestDumpCheckpoints)
UNIT_TEST_CASE(InputStaticFileCheckpointManagerUnittest, TestInvalidCheckpointFile)

} // namespace logtail

UNIT_TEST_MAIN
