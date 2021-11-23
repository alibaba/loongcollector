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
	"testing"

	"github.com/stretchr/testify/assert"
)

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
