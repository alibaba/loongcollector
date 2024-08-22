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

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "config/InstanceConfig.h"
#include "pipeline/InstanceConfigManager.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_BOOL(enable_send_tps_smoothing);
DECLARE_FLAG_BOOL(enable_flow_control);

namespace logtail {

class InstanceConfigManagerUnittest : public testing::Test {
public:
    void TestUpdateInstanceConfigs();
};

void InstanceConfigManagerUnittest::TestUpdateInstanceConfigs() {
    AppConfig::GetInstance();
    // Added
    {
        InstanceConfigDiff configDiff;
        std::string content
            = R"({"enable":true,"max_bytes_per_sec":1234,"mem_usage_limit":456,"cpu_usage_limit":2,"bool":false,"int":-1,"int64":-1000000,"uint":10000,"uint64":100000000000,"double":123123.1,"string":"string","array":[1,2,3],"object":{"a":1}})";
        std::string errorMsg;
        unique_ptr<Json::Value> detail = unique_ptr<Json::Value>(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(content, *detail, errorMsg));
        APSARA_TEST_TRUE(errorMsg.empty());
        InstanceConfig config("test1", std::move(detail));
        configDiff.mAdded.emplace_back(config);
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(1U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
    }

    // Modified
    {
        InstanceConfigDiff configDiff;
        std::string content
            = R"({"enable": true,"max_bytes_per_sec": 209715200, "mem_usage_limit":123, "cpu_usage_limit":4,"bool":false,"int":-1,"int64":-1000000,"uint":10000,"uint64":100000000000,"double":123123.1,"string":"string","array":[1,2,3],"object":{"a":1}})";
        std::string errorMsg;
        unique_ptr<Json::Value> detail = unique_ptr<Json::Value>(new Json::Value());
        APSARA_TEST_TRUE(ParseJsonTable(content, *detail, errorMsg));
        APSARA_TEST_TRUE(errorMsg.empty());
        InstanceConfig config("test1", std::move(detail));
        configDiff.mModified.emplace_back(config);
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(1U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
        APSARA_TEST_NOT_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
    }

    // mRemoved
    {
        InstanceConfigDiff configDiff;
        configDiff.mRemoved.emplace_back("test1");
        InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(configDiff);

        APSARA_TEST_EQUAL(0U, InstanceConfigManager::GetInstance()->GetAllConfigNames().size());
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test1"));
        APSARA_TEST_EQUAL(nullptr, InstanceConfigManager::GetInstance()->FindConfigByName("test3"));
    }
}

UNIT_TEST_CASE(InstanceConfigManagerUnittest, TestUpdateInstanceConfigs)

} // namespace logtail

UNIT_TEST_MAIN
