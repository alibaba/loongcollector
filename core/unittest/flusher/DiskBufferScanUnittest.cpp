// Copyright 2026 iLogtail Authors
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
#include <fstream>

#include "app_config/AppConfig.h"
#include "plugin/flusher/sls/DiskBufferWriter.h"
#include "unittest/Unittest.h"
#include "unittest/common/DirErrorInjector.h"

namespace logtail {

class DiskBufferScanUnittest : public testing::Test {
protected:
    void SetUp() override {
        mRoot = std::filesystem::path(GetProcessExecutionDir()) / "p1_disk_buffer";
        std::filesystem::create_directories(mRoot);
        mWriter.SetBufferFilePath(mRoot.string());
        mOldLimit = AppConfig::GetInstance()->mNumOfBufferFile;
        AppConfig::GetInstance()->mNumOfBufferFile = 1;
        for (int i = 1; i <= 3; ++i) {
            std::ofstream(mRoot / (GetSendBufferFileNamePrefix() + std::to_string(i))) << "unsent";
        }
    }
    void TearDown() override {
        AppConfig::GetInstance()->mNumOfBufferFile = mOldLimit;
        std::filesystem::remove_all(mRoot);
    }

    std::filesystem::path mRoot;
    DiskBufferWriter mWriter;
    int32_t mOldLimit = 0;
};

TEST_F(DiskBufferScanUnittest, FailedScanCannotDeleteUnsentBuffersOrReplaceOutput) {
    for (const auto& operation : {"open", "read", "close"}) {
        ScopedDirErrorInjector fault(mRoot.string() + PATH_SEPARATOR, operation, EIO, 1);
        std::vector<std::string> previous{"previous snapshot"};
        EXPECT_FALSE(mWriter.LoadFileToSend(time(nullptr), previous)) << operation;
        EXPECT_EQ((std::vector<std::string>{"previous snapshot"}), previous);
        EXPECT_FALSE(mWriter.CreateNewFile()) << operation;
        EXPECT_EQ(3, std::distance(std::filesystem::directory_iterator(mRoot), std::filesystem::directory_iterator()));
        EXPECT_EQ(0, mWriter.mBufferDivideTime);
    }
    EXPECT_TRUE(mWriter.CreateNewFile());
    EXPECT_EQ(1, std::distance(std::filesystem::directory_iterator(mRoot), std::filesystem::directory_iterator()));
    EXPECT_TRUE(std::filesystem::exists(mRoot / (GetSendBufferFileNamePrefix() + "3")));
}

TEST_F(DiskBufferScanUnittest, CompleteEmptyScanIsSuccessful) {
    std::filesystem::remove_all(mRoot);
    std::filesystem::create_directories(mRoot);
    std::vector<std::string> files{"old"};
    EXPECT_TRUE(mWriter.LoadFileToSend(time(nullptr), files));
    EXPECT_TRUE(files.empty());
}

} // namespace logtail

UNIT_TEST_MAIN
