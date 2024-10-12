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

#include <sys/stat.h>
#include <random>
#include <map>
#include "unittest/Unittest.h"
#include "checkpoint/AdhocCheckpointManager.h"
#include "common/FileSystemUtil.h"
#include "AppConfig.h"

namespace logtail {

std::string kTestRootDir;
AdhocCheckpointManager* mAdhocCheckpointManager;
std::map<std::pair<std::string, std::string>, AdhocFileKey> fileKeyMap;

class AdhocCheckpointManagerUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        kTestRootDir = (bfs::path(GetProcessExecutionDir()) / "AdhocCheckpointManagerUnittest").string();
        bfs::remove_all(kTestRootDir);
        bfs::create_directories(kTestRootDir);
        AppConfig::GetInstance()->SetLoongcollectorConfDir(kTestRootDir);
        mAdhocCheckpointManager = AdhocCheckpointManager::GetInstance();
    }

    static void TearDownTestCase() { bfs::remove_all(kTestRootDir); }

    /*
    create 2 job:
        test_job_A
            test_file_A_1, test_file_A_2, test_file_A_3
        test_job_B
            test_file_B_1, test_file_B_2
    */
    void TestAdhocCheckpointManagerStart();
    /*
    update status of files:
        test_job_A
            test_file_A_1 -> finished
            test_file_A_2 -> loading
            test_file_A_3 -> waiting
        test_job_B
            test_file_B_1 -> lost
            test_file_B_2 -> finished
    */
    void TestAdhocCheckpointManagerRun();
    /*
    check checkpoint files:
        /tmp/logtail_adhoc_checkpoint/test_job_A
        /tmp/logtail_adhoc_checkpoint/test_job_B
    wait 10ms
    update offset of test_file_A_2(should not dump)
    wait 5s(auto dump)
    check checkpoint files:
        /tmp/logtail_adhoc_checkpoint/test_job_A
        /tmp/logtail_adhoc_checkpoint/test_job_B
    */
    void TestAdhocCheckpointManagerDump();
    /*
    delete checkpoint files:
        /tmp/logtail_adhoc_checkpoint/test_job_A
        /tmp/logtail_adhoc_checkpoint/test_job_B
    check checkpoint files(should not exist):
        /tmp/logtail_adhoc_checkpoint/test_job_A
        /tmp/logtail_adhoc_checkpoint/test_job_B
    */
    void TestAdhocCheckpointManagerStop();

private:
    void AddJob(const std::string& jobName, const std::vector<std::string>& jobFiles);
    void MockFileLog(const std::string& jobName, const std::string& fileName);
    AdhocFileKey* GetAdhocFileKey(const std::string& jobName, const std::string& fileName);
    long GetFileUpdateTime(const std::string& filePath);

    std::string jobName1 = "test_job_A";
    std::vector<std::string> jobFiles1 = {"test_file_A_1", "test_file_A_2", "test_file_A_3"};
    std::string jobName2 = "test_job_B";
    std::vector<std::string> jobFiles2 = {"test_file_B_1", "test_file_B_2"};
};

UNIT_TEST_CASE(AdhocCheckpointManagerUnittest, TestAdhocCheckpointManagerStart);
UNIT_TEST_CASE(AdhocCheckpointManagerUnittest, TestAdhocCheckpointManagerRun);
UNIT_TEST_CASE(AdhocCheckpointManagerUnittest, TestAdhocCheckpointManagerDump);
UNIT_TEST_CASE(AdhocCheckpointManagerUnittest, TestAdhocCheckpointManagerStop);

void AdhocCheckpointManagerUnittest::TestAdhocCheckpointManagerStart() {
    // logtail start, AdhocCheckpointManager start
    mAdhocCheckpointManager->LoadAdhocCheckpoint();

    // job start
    // job 1
    AddJob(jobName1, jobFiles1);
    AdhocJobCheckpointPtr jobCheckpoint1 = mAdhocCheckpointManager->GetAdhocJobCheckpoint(jobName1);
    EXPECT_EQ(jobCheckpoint1->GetJobName(), jobName1);
    // job 2
    AddJob(jobName2, jobFiles2);
    AdhocJobCheckpointPtr jobCheckpoint2 = mAdhocCheckpointManager->GetAdhocJobCheckpoint(jobName2);
    EXPECT_EQ(jobCheckpoint2->GetJobName(), jobName2);
    // job 3
    std::string jobName3 = jobName1;
    std::vector<std::string> jobFiles3 = {"test_file_C_1"};
    AddJob(jobName3, jobFiles3);
    AdhocJobCheckpointPtr jobCheckpoint3
        = mAdhocCheckpointManager->GetAdhocJobCheckpoint(jobName3); // return jobCheckpoint1
    EXPECT_EQ(jobCheckpoint3->GetJobName(), jobName3);
    AdhocFileKey* fileKeyA1 = GetAdhocFileKey(jobName1, jobFiles1[0]);
    AdhocFileCheckpointPtr fileCheckpointA1 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName3, fileKeyA1);
    EXPECT_EQ(nullptr != fileCheckpointA1, true);
}

void AdhocCheckpointManagerUnittest::TestAdhocCheckpointManagerRun() {
    // Get file checkpoint
    AdhocFileKey* fileKeyA1 = GetAdhocFileKey(jobName1, jobFiles1[0]);
    AdhocFileCheckpointPtr fileCheckpointA1 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName1, fileKeyA1);
    EXPECT_EQ(nullptr != fileCheckpointA1, true);
    AdhocFileKey* fileKeyA2 = GetAdhocFileKey(jobName1, jobFiles1[1]);
    AdhocFileCheckpointPtr fileCheckpointA2 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName1, fileKeyA2);
    EXPECT_EQ(nullptr != fileCheckpointA2, false);

    // Read test_file_A_1
    fileCheckpointA1->mOffset = fileCheckpointA1->mSize;
    mAdhocCheckpointManager->UpdateAdhocFileCheckpoint(jobName1, fileKeyA1, fileCheckpointA1);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), true);
    EXPECT_EQ(fileCheckpointA1->mStatus == STATUS_FINISHED, true);

    // Read test_file_A_2
    fileCheckpointA2 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName1, fileKeyA2);
    EXPECT_EQ(nullptr != fileCheckpointA2, true);
    fileCheckpointA2->mOffset = fileCheckpointA2->mSize / 3;
    mAdhocCheckpointManager->UpdateAdhocFileCheckpoint(jobName1, fileKeyA2, fileCheckpointA2);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), true);
    EXPECT_EQ(fileCheckpointA2->mStatus == STATUS_LOADING, true);

    // Read test_file_B_1
    AdhocFileKey* fileKeyB1 = GetAdhocFileKey(jobName2, jobFiles2[0]);
    AdhocFileCheckpointPtr fileCheckpointB1 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName2, fileKeyB1);
    EXPECT_EQ(nullptr != fileCheckpointB1, true);
    fileCheckpointB1->mOffset = -1;
    mAdhocCheckpointManager->UpdateAdhocFileCheckpoint(jobName2, fileKeyB1, fileCheckpointB1);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName2)), true);
    EXPECT_EQ(fileCheckpointB1->mStatus == STATUS_LOST, true);

    // Read test_file_B_2
    AdhocFileKey* fileKeyB2 = GetAdhocFileKey(jobName2, jobFiles2[1]);
    AdhocFileCheckpointPtr fileCheckpointB2 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName2, fileKeyB2);
    EXPECT_EQ(nullptr != fileCheckpointB2, true);
    fileCheckpointB2->mOffset = fileCheckpointB2->mSize;
    mAdhocCheckpointManager->UpdateAdhocFileCheckpoint(jobName2, fileKeyB2, fileCheckpointB2);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName2)), true);
    EXPECT_EQ(fileCheckpointB2->mStatus == STATUS_FINISHED, true);
}

void AdhocCheckpointManagerUnittest::TestAdhocCheckpointManagerDump() {
    // check dump when file status change
    AdhocFileKey* fileKeyA2 = GetAdhocFileKey(jobName1, jobFiles1[1]);
    AdhocFileCheckpointPtr fileCheckpointA2 = mAdhocCheckpointManager->GetAdhocFileCheckpoint(jobName1, fileKeyA2);
    EXPECT_EQ(nullptr != fileCheckpointA2, true);
    fileCheckpointA2->mOffset = fileCheckpointA2->mSize / 3 * 2;

    usleep(1 * 1000 * 1000);
    mAdhocCheckpointManager->UpdateAdhocFileCheckpoint(jobName1, fileKeyA2, fileCheckpointA2);
    EXPECT_EQ(fileCheckpointA2->mStatus == STATUS_LOADING, true);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), true);
    long time1 = GetFileUpdateTime(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1));

    // check dump when auto dump per 5s
    usleep(5 * 1000 * 1000);
    mAdhocCheckpointManager->DumpAdhocCheckpoint();
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), true);
    long time2 = GetFileUpdateTime(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1));
    EXPECT_EQ(time1 != time2, true);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName2)), true);
}

void AdhocCheckpointManagerUnittest::TestAdhocCheckpointManagerStop() {
    // test delete
    mAdhocCheckpointManager->DeleteAdhocJobCheckpoint(jobName2);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), true);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName2)), false);

    usleep(5 * 1000 * 1000);
    mAdhocCheckpointManager->DeleteAdhocJobCheckpoint(jobName1);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName1)), false);
    EXPECT_EQ(CheckExistance(mAdhocCheckpointManager->GetJobCheckpointPath(jobName2)), false);
}

void AdhocCheckpointManagerUnittest::AddJob(const std::string& jobName, const std::vector<std::string>& jobFiles) {
    std::vector<AdhocFileCheckpointPtr> fileCheckpointList;
    for (std::string fileName : jobFiles) {
        std::string jobDataPath = GetProcessExecutionDir() + jobName;
        std::string filePath = PathJoin(jobDataPath, fileName);

        // mock file
        Mkdir(jobDataPath);
        std::ofstream file(filePath);
        EXPECT_EQ(!file, false);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(10, 100);

        int randomLogCount = dist(gen);
        for (int i = 0; i < randomLogCount; ++i) {
            int randomNumber = dist(gen);
            std::time_t now = std::time(nullptr);
            std::string log = std::to_string(randomNumber) + " - " + std::ctime(&now);
            file << log;
        }

        file.close();

        // create file checkpoint
        AdhocFileCheckpointPtr fileCheckpoint = mAdhocCheckpointManager->CreateAdhocFileCheckpoint(jobName, filePath);
        fileCheckpointList.push_back(fileCheckpoint);
        AdhocFileKey fileKey(fileCheckpoint->mDevInode, fileCheckpoint->mSignatureSize, fileCheckpoint->mSignatureHash);
        fileKeyMap[std::make_pair(jobName, fileName)] = fileKey;
    }
    AdhocJobCheckpointPtr jobCheckpoint
        = AdhocCheckpointManager::GetInstance()->CreateAdhocJobCheckpoint(jobName, fileCheckpointList);
    EXPECT_EQ(nullptr != jobCheckpoint, true);
}

AdhocFileKey* AdhocCheckpointManagerUnittest::GetAdhocFileKey(const std::string& jobName, const std::string& fileName) {
    return &fileKeyMap[std::make_pair(jobName, fileName)];
}

long AdhocCheckpointManagerUnittest::GetFileUpdateTime(const std::string& filePath) {
    struct stat statbuf;
    stat(filePath.c_str(), &statbuf);
    return statbuf.st_mtim.tv_nsec;
}

} // namespace logtail

UNIT_TEST_MAIN