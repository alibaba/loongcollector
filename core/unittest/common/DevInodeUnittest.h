/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "unittest/Unittest.h"
#include <string>
#include "common/RuntimeUtil.h"
#include "common/DevInode.h"

namespace logtail {

class DevInodeUnittest : public ::testing::Test {
protected:
    static std::string mRootDir;

public:
    static void SetUpTestCase() {
        bfs::path rootPath(GetProcessExecutionDir());
        rootPath /= "DevInodeUnittestDir";
        mRootDir = rootPath.string();
        EXPECT_TRUE(bfs::create_directories(rootPath));
    }

    static void TearDownTestCase() { EXPECT_TRUE(bfs::remove_all(bfs::path(mRootDir))); }
};

std::string DevInodeUnittest::mRootDir;

TEST_F(DevInodeUnittest, TestGetFileDevInode) {
    bfs::path testRoot(mRootDir + "/TestGetFileDevInode");
    ASSERT_TRUE(bfs::create_directories(testRoot));
    std::string subFileName = "file";
    std::string subDirName = "dir";
    std::ofstream((testRoot / subFileName).string());
    bfs::create_directory(testRoot / subDirName);

    auto fileDevInode = GetFileDevInode((testRoot / subFileName).string());
    EXPECT_TRUE(fileDevInode.IsValid());
    auto dirDevInode = GetFileDevInode((testRoot / subDirName).string());
    EXPECT_TRUE(dirDevInode.IsValid());
}

} // namespace logtail
