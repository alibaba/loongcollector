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

#include "unittest/Unittest.h"
#include <fstream>
#include <json/json.h>
#include "reader/CommonRegLogFileReader.h"
#include "util.h"

DECLARE_FLAG_INT32(force_release_deleted_file_fd_timeout);

namespace logtail {

class DeletedFileUnittest : public ::testing::Test {
public:
    void SetUp() override {
        hostLogPathDir = ".";
        hostLogPathFile = "DeletedFileUnittest.txt";
        reader.reset(new CommonRegLogFileReader(
            "testProject", "testLogstore", hostLogPathDir, hostLogPathFile, 0, "%Y-%m-%d %H:%M:%S", ""));
    }
    void TearDown() override { INT32_FLAG(force_release_deleted_file_fd_timeout) = -1; }
    void TestShouldForceReleaseDeletedFileFdDeleted();
    void TestShouldForceReleaseDeletedFileFdStopped();
    LogFileReaderPtr reader;
    std::string hostLogPathDir;
    std::string hostLogPathFile;
};

void DeletedFileUnittest::TestShouldForceReleaseDeletedFileFdDeleted() {
    APSARA_TEST_FALSE(reader->ShouldForceReleaseDeletedFileFd());
    INT32_FLAG(force_release_deleted_file_fd_timeout) = 0;
    APSARA_TEST_FALSE(reader->ShouldForceReleaseDeletedFileFd());
    reader->SetFileDeleted(true);
    APSARA_TEST_TRUE(reader->ShouldForceReleaseDeletedFileFd());
}

void DeletedFileUnittest::TestShouldForceReleaseDeletedFileFdStopped() {
    INT32_FLAG(force_release_deleted_file_fd_timeout) = 0;
    reader->SetContainerStopped();
    APSARA_TEST_TRUE(reader->ShouldForceReleaseDeletedFileFd());
}

UNIT_TEST_CASE(DeletedFileUnittest, TestShouldForceReleaseDeletedFileFdDeleted);
UNIT_TEST_CASE(DeletedFileUnittest, TestShouldForceReleaseDeletedFileFdStopped);


} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}