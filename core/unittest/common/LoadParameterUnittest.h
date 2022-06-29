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
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_BOOL(ilogtail_discard_old_data);
DECLARE_FLAG_STRING(check_point_filename);

namespace logtail {

class LoadParameterUnittest : public ::testing::Test {
public:
    void TestLoadInt32Parameter();

    void TestLoadBooleanParameter();

    void TestLoadStringParameter();
};

void LoadParameterUnittest::TestLoadInt32Parameter() {
    const char* kJSONName = "batch_send_interval";
    const char* kEnvName = "ALIYUN_LOGTAIL_BATCH_SEND_INTERVAL";
    const int32_t kDefaultVal = -1;
    const int32_t kVal = 10;

    // Not found.
    {
        Json::Value confJSON;
        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(!LoadInt32Parameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kDefaultVal, value);
    }

    // Load from config JSON.
    {
        Json::Value confJSON;
        confJSON[kJSONName] = kVal;

        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        // Test flag as value.
        INT32_FLAG(batch_send_interval) = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(INT32_FLAG(batch_send_interval), confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, INT32_FLAG(batch_send_interval));
    }

    // Load from env named by config JSON key.
    {
        SetEnv(kJSONName, ToString(kVal).c_str());

        Json::Value confJSON;
        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kJSONName);
    }

    // Load from specified env.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(value, confJSON, NULL, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }

    // Test priority: config JSON > env.
    {
        SetEnv(kEnvName, ToString(kDefaultVal).c_str());

        Json::Value confJSON;
        confJSON[kJSONName] = kVal;
        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
    }

    // Test env priority.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        int32_t value = kDefaultVal;
        APSARA_TEST_TRUE(LoadInt32Parameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }
}

void LoadParameterUnittest::TestLoadBooleanParameter() {
    const char* kJSONName = "discard_old_data";
    const char* kEnvName = "ALIYUN_LOGTAIL_DISCARD_OLD_DATA";
    const bool kDefaultVal = false;
    const bool kVal = true;
    APSARA_TEST_EQUAL(ToString(kDefaultVal), "false");
    APSARA_TEST_EQUAL(ToString(kVal), "true");

    // Not found.
    {
        Json::Value confJSON;
        bool value = kDefaultVal;
        APSARA_TEST_TRUE(!LoadBooleanParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kDefaultVal, value);
    }

    // Load from config JSON.
    {
        Json::Value confJSON;
        confJSON[kJSONName] = kVal;

        bool value = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        // Test flag as value.
        BOOL_FLAG(ilogtail_discard_old_data) = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(BOOL_FLAG(ilogtail_discard_old_data), confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, BOOL_FLAG(ilogtail_discard_old_data));
    }

    // Load from env named by config JSON key.
    {
        SetEnv(kJSONName, ToString(kVal).c_str());

        Json::Value confJSON;
        bool value = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kJSONName);
    }

    // Load from specified env.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        bool value = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(value, confJSON, NULL, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }

    // Test priority: config JSON > env.
    {
        SetEnv(kEnvName, ToString(kDefaultVal).c_str());

        Json::Value confJSON;
        confJSON[kJSONName] = kVal;
        bool value = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
    }

    // Test env priority.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        bool value = kDefaultVal;
        APSARA_TEST_TRUE(LoadBooleanParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }
}

void LoadParameterUnittest::TestLoadStringParameter() {
    const char* kJSONName = "check_point_filename";
    const char* kEnvName = "ALIYUN_LOGTAIL_CHECK_POINT_PATH";
    const std::string kDefaultVal = "default";
    const std::string kVal = "value";

    // Not found.
    {
        Json::Value confJSON;
        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(!LoadStringParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kDefaultVal, value);
    }

    // Load from config JSON.
    {
        Json::Value confJSON;
        confJSON[kJSONName] = kVal;

        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        // Test flag as value.
        STRING_FLAG(check_point_filename) = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(STRING_FLAG(check_point_filename), confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, STRING_FLAG(check_point_filename));
    }

    // Load from env named by config JSON key.
    {
        SetEnv(kJSONName, ToString(kVal).c_str());

        Json::Value confJSON;
        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kJSONName);
    }

    // Load from specified env.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(value, confJSON, NULL, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }

    // Test priority: config JSON > env.
    {
        SetEnv(kEnvName, ToString(kDefaultVal).c_str());

        Json::Value confJSON;
        confJSON[kJSONName] = kVal;
        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
    }

    // Test env priority.
    {
        SetEnv(kJSONName, ToString(kDefaultVal).c_str());
        SetEnv(kEnvName, ToString(kVal).c_str());

        Json::Value confJSON;
        std::string value = kDefaultVal;
        APSARA_TEST_TRUE(LoadStringParameter(value, confJSON, kJSONName, kEnvName));
        APSARA_TEST_EQUAL(kVal, value);

        UnsetEnv(kEnvName);
        UnsetEnv(kJSONName);
    }
}

APSARA_UNIT_TEST_CASE(LoadParameterUnittest, TestLoadInt32Parameter, 0);
APSARA_UNIT_TEST_CASE(LoadParameterUnittest, TestLoadBooleanParameter, 0);
APSARA_UNIT_TEST_CASE(LoadParameterUnittest, TestLoadStringParameter, 0);

} // namespace logtail
