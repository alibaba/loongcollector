// Copyright 2021 iLogtail Authors
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

package util

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

const (
	TEST_KEY1 = "LOONG_TEST_ENV_KEY1"
	TEST_KEY2 = "ALIYUN_TEST_ENV_KEY1"
	TEST_KEY3 = "ALICLOUD_TEST_ENV_KEY1"

	TEST_VALUE1 = "loong_test_value_1"
	TEST_VALUE2 = "aliyun_test_value_1"
	TEST_VALUE3 = "alicloud_test_value_1"

	DEFAULT_VALUE = "default_value"
)

func cleanupTestEnv() {
	os.Unsetenv(TEST_KEY1)
	os.Unsetenv(TEST_KEY2)
	os.Unsetenv(TEST_KEY3)
}

func Test(t *testing.T) {
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("cn-hangzhou.log.aliyuncs.com", "xx"))
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("cn-hangzhou-vpc.log.aliyuncs.com", "xx"))
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("cn-hangzhou-intranet.log.aliyuncs.com", "xx"))
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("cn-hangzhou-share.log.aliyuncs.com", "xx"))
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("http://cn-hangzhou.log.aliyuncs.com", "xx"))
	assert.Equal(t, "cn-hangzhou", GuessRegionByEndpoint("https://cn-hangzhou.log.aliyuncs.com", "xx"))
	assert.Equal(t, "xx", GuessRegionByEndpoint("hangzhou", "xx"))
	assert.Equal(t, "xx", GuessRegionByEndpoint("", "xx"))
	assert.Equal(t, "xx", GuessRegionByEndpoint("http://", "xx"))
}

func TestGetEnvTags(t *testing.T) {
	{
		cleanupTestEnv()
		os.Setenv(TEST_KEY1, TEST_VALUE1)

		result := GetEnvTags(TEST_KEY1, TEST_KEY2)
		assert.Equal(t, TEST_VALUE1, result)
	}

	{
		cleanupTestEnv()
		os.Setenv(TEST_KEY2, TEST_VALUE2)

		result := GetEnvTags(TEST_KEY1, TEST_KEY2)
		assert.Equal(t, TEST_VALUE2, result)
	}

	{
		cleanupTestEnv()

		result := GetEnvTags(TEST_KEY1, TEST_KEY2)
		assert.Equal(t, "", result)
	}

	{
		cleanupTestEnv()
		os.Setenv(TEST_KEY1, TEST_VALUE1)
		os.Setenv(TEST_KEY2, TEST_VALUE2)

		result := GetEnvTags(TEST_KEY1, TEST_KEY2)
		assert.Equal(t, TEST_VALUE1, result)
	}
}

func TestInitFromEnvString(t *testing.T) {
	{
		cleanupTestEnv()
		os.Setenv(TEST_KEY1, TEST_VALUE1)

		var result string
		err := InitFromEnvString(TEST_KEY1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, TEST_VALUE1, result)
	}

	{
		cleanupTestEnv()

		var result string
		err := InitFromEnvString(TEST_KEY1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, DEFAULT_VALUE, result)
	}

	{
		cleanupTestEnv()
		os.Setenv("LOONG_TEST_ENV_KEY1", TEST_VALUE1)
		os.Setenv("ALICLOUD_TEST_ENV_KEY1", TEST_VALUE3)

		var result string
		err := InitFromEnvString(TEST_KEY3, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, TEST_VALUE1, result)
	}

	{
		cleanupTestEnv()
		os.Setenv("ALICLOUD_TEST_ENV_KEY1", TEST_VALUE3)

		var result string
		err := InitFromEnvString(TEST_KEY3, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, TEST_VALUE3, result)
	}

	{
		cleanupTestEnv()
		os.Setenv(TEST_KEY1, "")

		var result string
		err := InitFromEnvString(TEST_KEY1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, DEFAULT_VALUE, result)
	}
}
