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

#include <string>

#include "apm/Types.h"
#include "common/JsonUtil.h"
#include "unittest/Unittest.h"

namespace logtail::apm {

class ApmInjectConfigUnittest : public testing::Test {
public:
    void TestInitApmAttachOption() {
        Json::Value configJson;
        Json::Value optionalGoPipeline;
        std::string configStr;
        std::string errorMsg;

        // valid optional param
        configStr = R"(
            {
                "Type": "apm_agent_inject",
                "AppId": "test-app-id",
                "AppName": "test-app-name",
                "Workspace": "test-workspace",
                "ServiceId": "test-service-id",
                "LicenseKey": "test-license-key",
                "Language": "jAva",
                "CommandId": "123",
                "AgentVersion": "test-agent-version",
                "MatchRules": 
                [
                    {
                        "RuleType": "cwd",
                        "Operation": "eq",
                        "Value": "aaa",
                    },
                    {
                        "RuleType": "cwd",
                        "Operation": "eq",
                        "Value": "bbb",
                    },
                ]
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        TaskPipelineContext ctx;
        ctx.SetConfigName("test-config");
        ctx.SetCreateTime(100);
        std::unique_ptr<AttachConfig> attachConfig = std::make_unique<AttachConfig>();
        bool status = InitApmAttachOption(&ctx, configJson, attachConfig, "apm_agent_inject");
        APSARA_TEST_TRUE(status);
        APSARA_TEST_TRUE(attachConfig != nullptr);
        APSARA_TEST_EQUAL(attachConfig->mAppId, "test-app-id");
        APSARA_TEST_EQUAL(attachConfig->mAppName, "test-app-name");
        APSARA_TEST_EQUAL(attachConfig->mServiceId, "test-service-id");
        APSARA_TEST_EQUAL(attachConfig->mWorkspace, "test-workspace");
        APSARA_TEST_EQUAL(attachConfig->mAgentVersion, "test-agent-version");
        APSARA_TEST_EQUAL(attachConfig->mMatchRules.size(), 2);
    }

private:
};

UNIT_TEST_CASE(ApmInjectConfigUnittest, TestInitApmAttachOption);

} // namespace logtail::apm


UNIT_TEST_MAIN
