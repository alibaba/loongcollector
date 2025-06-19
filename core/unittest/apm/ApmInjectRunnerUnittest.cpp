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

#include <memory>
#include <thread>

#include "apm/ApmInjectRunner.h"
#include "apm/Types.h"
#include "common/timer/Timer.h"
#include "unittest/Unittest.h"

namespace logtail::apm {

namespace fs = std::filesystem;

class ApmInjectRunnerUnittest : public testing::Test {
public:
    void SetUp() {
        mTestDir = std::filesystem::temp_directory_path() / "ilogtail_test";
        std::filesystem::create_directories(mTestDir);
        mRunner = ApmInjectRunner::GetInstance();
        mRunner->Init();
    }

    void TearDown() {
        mRunner->Stop();
        Timer::GetInstance()->Clear();
        Timer::GetInstance()->Stop();
        fs::remove_all("/opt/.arms");
        std::filesystem::remove_all(mTestDir);
    }

    void TestInitAndStop() {
        mRunner->Init();
        APSARA_TEST_TRUE(mRunner->mStarted);
        mRunner->Stop();
        APSARA_TEST_FALSE(mRunner->mStarted);
    }

    void TestScheduleCheckUpdates() {
        Timer::GetInstance()->Stop();
        mRunner->ScheduleCheckUpdates();
        mRunner->ScheduleCheckUpdates();
        mRunner->ScheduleCheckUpdates();
        APSARA_TEST_EQUAL(Timer::GetInstance()->mQueue.size(), 4);
        Timer::GetInstance()->Clear();
        // Timer::GetInstance()->
        // APSARA_TEST_TRUE(mRunner->mCheckUpdateTimerEvent != nullptr);
    }

    void TestCheckUpdateEvent() {
        auto interval = std::chrono::seconds(1);
        CheckUpdateEvent event(interval);
        APSARA_TEST_TRUE(event.IsValid());
        APSARA_TEST_TRUE(event.Execute());
    }

    void TestInjectApmAgentInner() {
        std::string configName = "inner-test";
        AttachContextWithRetry ctxWithRetry;
        auto config = std::make_unique<AttachConfig>();
        config->mLanguage = APMLanguage::kJava;
        config->mCommandType = CommandType::kInstall;
        config->mAppId = "test-appid";
        config->mAppName = "test-app";
        config->mAgentVersion = "latest";
        config->mLicenseKey = "test-key";
        config->mServiceId = "svc";
        config->mWorkspace = "ws";
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestDir.string();
        config->mMatchRules.push_back(rule);
        ctxWithRetry.context = std::make_unique<AttachContext>(std::move(config));
        ctxWithRetry.retryCount = 0;
        ctxWithRetry.lastStatus = ApmAttachStatus::kInProgress;
        mRunner->injectApmAgentInner(configName, ctxWithRetry, false);
        APSARA_TEST_TRUE(ctxWithRetry.lastStatus == ApmAttachStatus::kSucceed);
        APSARA_TEST_TRUE(fs::exists(mTestDir / ".arms.rc"));
        std::ifstream rcFile(mTestDir / ".arms.rc");
        std::string content((std::istreambuf_iterator<char>(rcFile)), std::istreambuf_iterator<char>());
        APSARA_TEST_TRUE(content.find("test-key") != std::string::npos);
        APSARA_TEST_TRUE(content.find("test-app") != std::string::npos);
        APSARA_TEST_TRUE(content.find("ECS_AUTO") != std::string::npos);
    }

    void TestRemoveApmAgentInner() {
        std::string configName = "remove-test";
        AttachContextWithRetry ctxWithRetry;
        auto config = std::make_unique<AttachConfig>();
        config->mLanguage = APMLanguage::kJava;
        config->mCommandType = CommandType::kInstall;
        config->mAppId = "test-appid";
        config->mAppName = "test-app";
        config->mAgentVersion = "";
        config->mLicenseKey = "test-key";
        config->mServiceId = "svc";
        config->mWorkspace = "ws";
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestDir.string();
        config->mMatchRules.push_back(rule);
        ctxWithRetry.context = std::make_unique<AttachContext>(std::move(config));
        mRunner->injectApmAgentInner(configName, ctxWithRetry, false);
        APSARA_TEST_TRUE(fs::exists("/opt/.arms/apm-java-agent/test-appid"));
        APSARA_TEST_TRUE(
            fs::exists("/opt/.arms/apm-java-agent/test-appid/current/AliyunJavaAgent/aliyun-java-agent.jar"));
        APSARA_TEST_TRUE(ctxWithRetry.lastStatus == ApmAttachStatus::kSucceed);
        APSARA_TEST_TRUE(fs::exists(mTestDir / ".arms.rc"));
        std::ifstream rcFile(mTestDir / ".arms.rc");
        std::string content((std::istreambuf_iterator<char>(rcFile)), std::istreambuf_iterator<char>());
        APSARA_TEST_TRUE(content.find("test-key") != std::string::npos);
        APSARA_TEST_TRUE(content.find("test-app") != std::string::npos);
        APSARA_TEST_TRUE(content.find("ECS_AUTO") != std::string::npos);

        // remove ...
        auto& config2 = mRunner->mAttachConfigs[configName];
        mRunner->removeApmAgentInner(configName, config2);
        APSARA_TEST_FALSE(fs::exists(mTestDir / ".arms.rc"));
        // java agent path
        APSARA_TEST_FALSE(fs::exists("/opt/.arms/apm-java-agent/test-appid"));
    }

    void TestInjectUpdate() {
        auto config = std::make_unique<AttachConfig>();
        config->mLanguage = APMLanguage::kJava;
        config->mCommandType = CommandType::kUpdate;
        config->mAppId = "test-appid";
        config->mAppName = "test-app";
        config->mAgentVersion = "1.0.1";
        config->mLicenseKey = "test-key";
        config->mServiceId = "svc";
        config->mWorkspace = "ws";
        MatchRule rule;
        rule.mRuleType = RuleType::kCwd;
        rule.mOperation = RuleOperation::kEq;
        rule.mVal = mTestDir.string();
        config->mMatchRules.push_back(rule);
        TaskPipelineContext ctx;
        ctx.SetConfigName("test-0");
        APSARA_TEST_TRUE(mRunner->InjectApmAgent(&ctx, std::move(config)));
    }

    void TestInjectInvalidConfig() {
        auto config = std::make_unique<AttachConfig>();
        // 缺少必要字段
        TaskPipelineContext ctx;
        ctx.SetConfigName("test-invalid");
        APSARA_TEST_TRUE(mRunner->InjectApmAgent(&ctx, std::move(config)));
    }

    void TestCheckUpdatesRetry() {
        // 构造失败的AttachContextWithRetry
        std::string configName = "fail-case";
        AttachContextWithRetry ctxWithRetry;
        ctxWithRetry.context = std::make_unique<AttachContext>(std::make_unique<AttachConfig>());
        ctxWithRetry.retryCount = 0;
        ctxWithRetry.lastStatus = ApmAttachStatus::kFailed;
        mRunner->mAttachConfigs[configName] = std::move(ctxWithRetry);
        mRunner->CheckUpdates();
        APSARA_TEST_TRUE(mRunner->mAttachConfigs[configName].retryCount == 1);
    }

private:
    ApmInjectRunner* mRunner;
    std::filesystem::path mTestDir;
};

UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestInitAndStop);
UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestScheduleCheckUpdates);
UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestCheckUpdateEvent);
UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestInjectApmAgentInner);
UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestRemoveApmAgentInner);
// UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestInjectUpdate);
// UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestInjectInvalidConfig);
// UNIT_TEST_CASE(ApmInjectRunnerUnittest, TestCheckUpdatesRetry);

} // namespace logtail::apm

UNIT_TEST_MAIN
