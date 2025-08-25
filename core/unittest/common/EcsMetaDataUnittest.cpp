// Copyright 2025 loongcollector Authors
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

#include "common/JsonUtil.h"
#include "common/MachineInfoUtil.h"
#include "unittest/Unittest.h"

using namespace logtail;

class EcsMetaDataUnittest : public ::testing::Test {};

TEST_F(EcsMetaDataUnittest, TestParseEcsMeta) {
    std::string metaString = "{invalid json}";
    {
        ECSMeta ecsMeta;
        APSARA_TEST_FALSE(ParseECSMeta(metaString, ecsMeta));
    }

    {
        ECSMeta ecsMeta;
        metaString = R"({
            "instance-id": "i-1234567890abcdef0",
            "owner-account-id": "123456789012345678",
            "region-id": "cn-hangzhou"
        })";
        APSARA_TEST_TRUE(ParseECSMeta(metaString, ecsMeta));
    }

    {
        ECSMeta ecsMeta;
        metaString = R"({
            "instance-id": "i-1234567890abcdef0",
            "region-id": "cn-hangzhou"
        })";
        APSARA_TEST_FALSE(ParseECSMeta(metaString, ecsMeta));
    }

    {
        ECSMeta ecsMeta;
        metaString = R"({
            "instance-id": 12345,
            "owner-account-id": "123456789012345678",
            "region-id": ["cn-hangzhou"]
        })";
        APSARA_TEST_FALSE(ParseECSMeta(metaString, ecsMeta));
    }

    {
        ECSMeta ecsMeta;
        metaString = "{}";
        APSARA_TEST_FALSE(ParseECSMeta(metaString, ecsMeta));
    }
}

TEST_F(EcsMetaDataUnittest, TestParseCredentials) {
    std::string credStr = "";
    Json::Value cred;
    std::string errMsg;

    { APSARA_TEST_TRUE(!ParseJsonTable(credStr, cred, errMsg)); }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;
        credStr = R"({
            "Code": "Success",
            "AccessKeyId": "test-access-key-id",
            "AccessKeySecret": "test-access-key-secret",
            "SecurityToken": "test-security-token",
            "Expiration": "2025-07-28T02:06:13Z"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_TRUE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
        APSARA_TEST_EQUAL(accessKeyId, "test-access-key-id");
        APSARA_TEST_EQUAL(accessKeySecret, "test-access-key-secret");
        APSARA_TEST_EQUAL(secToken, "test-security-token");
        APSARA_TEST_EQUAL(expTime, 1753668373);
    }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;
        credStr = R"({
            "Code": "Success",
            "AccessKeySecret": "test-access-key-secret",
            "SecurityToken": "test-security-token",
            "Expiration": "2025-07-28T02:06:13Z"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_FALSE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
    }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;
        credStr = R"({
            "Code": "Success",
            "AccessKeyId": "test-access-key-id",
            "SecurityToken": "test-security-token",
            "Expiration": "2025-07-28T02:06:13Z"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_FALSE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
    }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;
        credStr = R"({
            "Code": "Success",
            "AccessKeyId": "test-access-key-id",
            "AccessKeySecret": "test-access-key-secret",
            "Expiration": "2025-07-28T02:06:13Z"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_FALSE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
    }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;
        credStr = R"({
            "Code": "Success",
            "AccessKeyId": "test-access-key-id",
            "AccessKeySecret": "test-access-key-secret",
            "SecurityToken": "test-security-token"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_FALSE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
    }

    {
        std::string accessKeyId, accessKeySecret, secToken;
        int64_t expTime = 0;

        credStr = R"({
            "Code": "Success",
            "AccessKeyId": 12345,
            "AccessKeySecret": "test-access-key-secret",
            "SecurityToken": "test-security-token",
            "Expiration": "2025-07-28T02:06:13Z"
        })";
        APSARA_TEST_TRUE(ParseJsonTable(credStr, cred, errMsg));
        APSARA_TEST_FALSE(ParseCredentials(cred, accessKeyId, accessKeySecret, secToken, expTime));
    }
}

UNIT_TEST_MAIN
