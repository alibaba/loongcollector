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

#include "common/EnvUtil.h"
#include "unittest/Unittest.h"

using namespace logtail;

class EnvUtilUnittest : public ::testing::Test {
protected:
    static constexpr const char* TEST_KEY1 = "LOONG_TEST_ENV_KEY1";
    static constexpr const char* TEST_KEY2 = "ALIYUN_TEST_ENV_KEY1";

    static constexpr const char* TEST_VALUE1 = "loong_test_value_1";
    static constexpr const char* TEST_VALUE2 = "aliyun_test_value_1";
    static constexpr const char* TEST_VALUE_EMPTY = "";

    void CleanupTestEnv() {
        logtail::UnsetEnv(TEST_KEY1);
        logtail::UnsetEnv(TEST_KEY2);
    }
};

TEST_F(EnvUtilUnittest, TestGetEnv) {
    {
        CleanupTestEnv();
        logtail::SetEnv(TEST_KEY1, TEST_VALUE1);

        char* result = logtail::GetEnv(TEST_KEY1, TEST_KEY2);
        APSARA_TEST_STREQ(result, TEST_VALUE1);
    }

    {
        CleanupTestEnv();
        logtail::SetEnv(TEST_KEY2, TEST_VALUE2);

        char* result = logtail::GetEnv(TEST_KEY1, TEST_KEY2);
        APSARA_TEST_STREQ(result, TEST_VALUE2);
    }

    {
        CleanupTestEnv();

        char* result = logtail::GetEnv(TEST_KEY1, TEST_KEY2);
        APSARA_TEST_EQUAL(result, nullptr);
    }

    {
        CleanupTestEnv();
        logtail::SetEnv(TEST_KEY1, TEST_VALUE1);
        logtail::SetEnv(TEST_KEY2, TEST_VALUE2);

        char* result = logtail::GetEnv(TEST_KEY1, TEST_KEY2);
        APSARA_TEST_STREQ(result, TEST_VALUE1);
    }
}

UNIT_TEST_MAIN
