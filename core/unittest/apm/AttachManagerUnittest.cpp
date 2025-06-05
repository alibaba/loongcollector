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

#include "apm/AttachManager.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

namespace logtail::apm {

namespace fs = std::filesystem;

class AttachManagerUnittest : public testing::Test {
public:
    void SetUp() override {
        mTestCwdDir = std::filesystem::temp_directory_path() / "loongcollector_test" / "cwd";
        std::filesystem::create_directories(mTestCwdDir);
    }

    void TearDown() override {
        // 清理临时测试目录
        std::filesystem::remove_all(mTestCwdDir);
    }
    void TestInit() {}
    void TestPrepareRuntimeInfo() {
        mMgr.prepareRuntimeConfig(mTestCwdDir,
                                  "/opt/.arms/apm-java-agent/AliyunJavaAgent/aliyun-java-agent.jar",
                                  "test-license-key",
                                  "test-app-name");
        fs::path rc = mTestCwdDir / ".arms.rc";
        APSARA_TEST_TRUE(fs::exists(rc));
        std::ifstream rcFile(rc);
        std::string content((std::istreambuf_iterator<char>(rcFile)), std::istreambuf_iterator<char>());
        LOG_INFO(sLogger, ("content", content));
        APSARA_TEST_TRUE(content.find("test-license-key") != std::string::npos);
        APSARA_TEST_TRUE(content.find("test-app-name") != std::string::npos);
        APSARA_TEST_TRUE(content.find("/opt/.arms/apm-java-agent/AliyunJavaAgent/aliyun-java-agent.jar")
                         != std::string::npos);
    }

private:
    AttachManager mMgr;
    std::filesystem::path mTestCwdDir;
};

UNIT_TEST_CASE(AttachManagerUnittest, TestPrepareRuntimeInfo);

} // namespace logtail::apm


UNIT_TEST_MAIN
