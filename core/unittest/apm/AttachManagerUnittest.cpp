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

    void TestDoAttachTwice() {
        AttachManager mgr;
        auto config = std::make_shared<AttachConfig>();
        config->mLicenseKey = "test-license-key";
        config->mAppName = "test-app-name";
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestCwdDir.string();
        std::string agentPath = "/tmp/agent.jar";
        auto context = std::make_shared<AttachContext>(config);
        APSARA_TEST_TRUE(mMgr.DoAttach(rule, agentPath, context));
        APSARA_TEST_TRUE(mMgr.DoAttach(rule, agentPath, context));
        APSARA_TEST_TRUE(fs::exists(mTestCwdDir / ".arms.rc"));
        std::ifstream rcFile(mTestCwdDir / ".arms.rc");
        std::string content((std::istreambuf_iterator<char>(rcFile)), std::istreambuf_iterator<char>());
        LOG_INFO(sLogger, ("content", content));
        APSARA_TEST_TRUE(content.find("test-license-key") != std::string::npos);
        APSARA_TEST_TRUE(content.find("test-app-name") != std::string::npos);
        APSARA_TEST_TRUE(content.find("/tmp/agent.jar") != std::string::npos);
    }

    void TestDoDetachNonExist() {
        AttachManager mgr;
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestCwdDir.string();
        // 未创建rc文件直接detach
        APSARA_TEST_TRUE(mgr.DoDetach(rule));
    }
    void TestCheckAttachStatus() {
        AttachManager mgr;
        int fakePid = 12345;
        APSARA_TEST_FALSE(mgr.CheckAttachStatus(mTestCwdDir, fakePid));
        // 创建pid文件并写入pid
        std::ofstream ofs(mTestCwdDir / ".arms.pid");
        ofs << fakePid;
        ofs.close();
        APSARA_TEST_TRUE(mgr.CheckAttachStatus(mTestCwdDir, fakePid));
    }
    void TestAttachNoPermission() {
        AttachManager mgr;
        auto config = std::make_shared<AttachConfig>();
        config->mLicenseKey = "key";
        config->mAppName = "app";
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestCwdDir.string();
        auto context = std::make_shared<AttachContext>(config);
        std::string agentPath = "/tmp/agent.jar";
        // 创建只读目录
        LOG_INFO(sLogger, ("step readonly", ""));
        fs::permissions(mTestCwdDir, fs::perms::owner_read, fs::perm_options::replace);

        auto p = fs::status(mTestCwdDir).permissions();
        std::cout << "Permissions: " << static_cast<int>(p) << '\n';
        // 或者使用如下方式打印：
        std::cout << "Owner read: " << ((p & fs::perms::owner_read) != fs::perms::none) << '\n';
        std::cout << "Owner write: " << ((p & fs::perms::owner_write) != fs::perms::none) << '\n';
        std::cout << "Owner exec: " << ((p & fs::perms::owner_exec) != fs::perms::none) << '\n';


        APSARA_TEST_FALSE(fs::exists(mTestCwdDir / ".arms.rc"));
        APSARA_TEST_FALSE(mMgr.DoAttach(rule, agentPath, context));
        APSARA_TEST_TRUE(fs::exists(mTestCwdDir / ".arms.rc"));
        std::ifstream rcFile(mTestCwdDir / ".arms.rc");
        std::string content((std::istreambuf_iterator<char>(rcFile)), std::istreambuf_iterator<char>());
        LOG_INFO(sLogger, ("content", content));
        // 恢复权限
        LOG_INFO(sLogger, ("resume", ""));
        fs::permissions(mTestCwdDir, fs::perms::owner_all, fs::perm_options::replace);
        APSARA_TEST_TRUE(mMgr.DoAttach(rule, agentPath, context));
    }

private:
    AttachManager mMgr;
    std::filesystem::path mTestCwdDir;
};

UNIT_TEST_CASE(AttachManagerUnittest, TestPrepareRuntimeInfo);
UNIT_TEST_CASE(AttachManagerUnittest, TestDoAttachTwice);
UNIT_TEST_CASE(AttachManagerUnittest, TestDoDetachNonExist);
UNIT_TEST_CASE(AttachManagerUnittest, TestCheckAttachStatus);
// UNIT_TEST_CASE(AttachManagerUnittest, TestAttachNoPermission);

} // namespace logtail::apm


UNIT_TEST_MAIN
