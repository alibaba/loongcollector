// Copyright 2023 iLogtail Authors
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

#include "Flags.h"
#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "config/InstanceConfig.h"
#include "instance_config/InstanceConfigManager.h"
#include "runner/FlusherRunner.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_BOOL(enable_send_tps_smoothing);
DECLARE_FLAG_BOOL(enable_flow_control);

namespace logtail {

class InstanceConfigManagerUnittest : public testing::Test {
public:
    void TestUpdateInstanceConfigs();
    bool LoadModuleConfig(bool isInit);
    int32_t max_bytes_per_sec;
    int status = 0;
};

bool InstanceConfigManagerUnittest::LoadModuleConfig(bool isInit) {
    auto ValidateFn = [](const std::string key, const auto value) -> bool { return true; };

    if (status == 0) {
        // Added
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(false, false, "bool_true", ValidateFn), true);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(0, 0, "int32_true", ValidateFn), 1234);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(0, 0, "int64_true", ValidateFn), 1234567890);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(0, 0, "double_true", ValidateFn), 1234.56789);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("", "", "string_true", ValidateFn), "string");

        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(false, false, "bool_false", ValidateFn), false);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(0, 0, "int32_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(0, 0, "int64_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(0, 0, "double_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("", "", "string_false", ValidateFn), "");
    } else if (status == 1) {
        // Modified
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(true, true, "bool_true", ValidateFn), false);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(0, 0, "int32_true", ValidateFn), 12340);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(0, 0, "int64_true", ValidateFn), 12345678900);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(0, 0, "double_true", ValidateFn), 12340.56789);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("", "", "string_true", ValidateFn), "string0");

        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(true, true, "bool_false", ValidateFn), true);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(10, 10, "int32_false", ValidateFn), 10);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(10, 10, "int64_false", ValidateFn), 10);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(10.1, 10.1, "double_false", ValidateFn), 10.1);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("10.1", "10.1", "string_false", ValidateFn), "10.1");
    } else if (status == 2) {
        status = 3;
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(false, false, "bool_true", ValidateFn), false);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(0, 0, "int32_true", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(0, 0, "int64_true", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(0, 0, "double_true", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("", "", "string_true", ValidateFn), "");

        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeBool(false, false, "bool_false", ValidateFn), false);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt32(0, 0, "int32_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeInt64(0, 0, "int64_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeDouble(0, 0, "double_false", ValidateFn), 0);
        APSARA_TEST_EQUAL(AppConfig::GetInstance()->MergeString("", "", "string_false", ValidateFn), "");
    }

    return true;
}

void InstanceConfigManagerUnittest::TestUpdateInstanceConfigs() {
    std::function<bool()> mCallback = [this]() { return LoadModuleConfig(false); };
    {
        AppConfig::GetInstance()->GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("bool_true", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("int32_true", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("int64_true", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("double_true", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("string_true", &mCallback);

        AppConfig::GetInstance()->GetInstance()->RegisterCallback("bool_false", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("int32_false", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("int64_false", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("double_false", &mCallback);
        AppConfig::GetInstance()->GetInstance()->RegisterCallback("string_false", &mCallback);
    }
    FlusherRunner::GetInstance()->Init();
    // Added
    {
        InstanceConfigDiff configDiff;
        {
            std::string content = R"({
                "max_bytes_per_sec": 1234
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test0", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }
        {
            std::string content = R"({
                "bool_true": true,
                "int32_true": 1234,
                "int64_true": 1234567890,
                "double_true": 1234.56789,
                "string_true": "string"
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test1", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }
        {
            std::string content = R"({
                "bool_false": 1,
                "int32_false": false,
                "int64_false": false,
                "double_false": false,
                "string_false": false
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test2", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(3U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test2"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
    }
    APSARA_TEST_EQUAL(kDefaultMaxSendBytePerSec, AppConfig::GetInstance()->GetMaxBytePerSec());
    APSARA_TEST_EQUAL(true, FlusherRunner::GetInstance()->mSendRandomSleep);
    APSARA_TEST_EQUAL(true, FlusherRunner::GetInstance()->mSendFlowControl);
    // Modified
    status = 1;
    {
        InstanceConfigDiff configDiff;
        {
            std::string content = R"({
                "max_bytes_per_sec": 31457280
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test0", detail, "dir");
            configDiff.mAdded.emplace_back(config);
        }
        {
            std::string content = R"({
                "bool_true": false,
                "int32_true": 12340,
                "int64_true": 12345678900,
                "double_true": 12340.56789,
                "string_true": "string0"
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test1", detail, "dir");
            configDiff.mModified.emplace_back(config);
        }
        {
            std::string content = R"({
                "bool_false": 1,
                "int32_false": false,
                "int64_false": false,
                "double_false": false,
                "string_false": false
            })";
            std::string errorMsg;
            Json::Value detail;
            APSARA_TEST_TRUE(ParseJsonTable(content, detail, errorMsg));
            APSARA_TEST_TRUE(errorMsg.empty());
            InstanceConfig config("test2", detail, "dir");
            configDiff.mModified.emplace_back(config);
        }
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(3U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test2"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
    }
    APSARA_TEST_EQUAL(31457280, AppConfig::GetInstance()->GetMaxBytePerSec());
    APSARA_TEST_EQUAL(false, FlusherRunner::GetInstance()->mSendRandomSleep);
    APSARA_TEST_EQUAL(false, FlusherRunner::GetInstance()->mSendFlowControl);
    // Removed
    status = 2;
    {
        InstanceConfigDiff configDiff;
        configDiff.mRemoved.emplace_back("test1");
        configDiff.mRemoved.emplace_back("test2");
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(1U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test0"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test2"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
    }
    FlusherRunner::GetInstance()->Stop();
}

UNIT_TEST_CASE(InstanceConfigManagerUnittest, TestUpdateInstanceConfigs)

} // namespace logtail

UNIT_TEST_MAIN
