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

#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(force_release_deleted_file_fd_timeout);

using namespace std;

namespace logtail {

class DeletedFileUnittest : public testing::Test {
public:
    void TestShouldForceReleaseDeletedFileFdDeleted();
    void TestShouldForceReleaseDeletedFileFdStopped();

protected:
    void SetUp() override {
        reader.reset(new LogFileReader(hostLogPathDir,
                                       hostLogPathFile,
                                       DevInode(),
                                       make_pair(&readerOpts, &ctx),
                                       make_pair(&multilineOpts, &ctx)));
        }

    void TearDown() override { INT32_FLAG(force_release_deleted_file_fd_timeout) = -1; }

private:
    LogFileReaderPtr reader;
    FileReaderOptions readerOpts;
    MultilineOptions multilineOpts;
    PipelineContext ctx;
    string hostLogPathDir = ".";
    string hostLogPathFile = "DeletedFileUnittest.txt";
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

UNIT_TEST_CASE(DeletedFileUnittest, TestShouldForceReleaseDeletedFileFdDeleted)
UNIT_TEST_CASE(DeletedFileUnittest, TestShouldForceReleaseDeletedFileFdStopped)

} // namespace logtail

UNIT_TEST_MAIN
