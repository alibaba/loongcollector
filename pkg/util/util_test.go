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
	testKey1 = "LOONG_TEST_ENV_KEY1"
	testKey2 = "ALIYUN_TEST_ENV_KEY1"
	testKey3 = "ALICLOUD_TEST_ENV_KEY1"

	testValue1 = "loong_test_value_1"
	testValue2 = "aliyun_test_value_1"
	testValue3 = "alicloud_test_value_1"

	DEFAULT_VALUE = "default_value"
)

func cleanupTestEnv() {
	os.Unsetenv(testKey1)
	os.Unsetenv(testKey2)
	os.Unsetenv(testKey3)
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
		os.Setenv(testKey1, testValue1)

		result := GetEnvTags(testKey1, testKey2)
		assert.Equal(t, testValue1, result)
	}

	{
		cleanupTestEnv()
		os.Setenv(testKey2, testValue2)

		result := GetEnvTags(testKey1, testKey2)
		assert.Equal(t, testValue2, result)
	}

	{
		cleanupTestEnv()

		result := GetEnvTags(testKey1, testKey2)
		assert.Equal(t, "", result)
	}

	{
		cleanupTestEnv()
		os.Setenv(testKey1, testValue1)
		os.Setenv(testKey2, testValue2)

		result := GetEnvTags(testKey1, testKey2)
		assert.Equal(t, testValue1, result)
	}
}

func TestInitFromEnvString(t *testing.T) {
	{
		cleanupTestEnv()
		os.Setenv(testKey1, testValue1)

		var result string
		err := InitFromEnvString(testKey1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, testValue1, result)
	}

	{
		cleanupTestEnv()

		var result string
		err := InitFromEnvString(testKey1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, DEFAULT_VALUE, result)
	}

	{
		cleanupTestEnv()
		os.Setenv("LOONG_TEST_ENV_KEY1", testValue1)
		os.Setenv("ALICLOUD_TEST_ENV_KEY1", testValue3)

		var result string
		err := InitFromEnvString(testKey3, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, testValue1, result)
	}

	{
		cleanupTestEnv()
		os.Setenv("ALICLOUD_TEST_ENV_KEY1", testValue3)

		var result string
		err := InitFromEnvString(testKey3, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, testValue3, result)
	}

	{
		cleanupTestEnv()
		os.Setenv(testKey1, "")

		var result string
		err := InitFromEnvString(testKey1, &result, DEFAULT_VALUE)
		assert.NoError(t, err)
		assert.Equal(t, DEFAULT_VALUE, result)
	}
}
