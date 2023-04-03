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

package helper

import (
	"os"
	"strings"
	"sync"
)

const defaultEnvTagKeys = "ALIYUN_LOG_ENV_TAGS"

// EnvTags to be add to every logroup
var EnvTags []string
var envTagsLock sync.Mutex

// LoadEnvTags load tags from env
func LoadEnvTags() {
	defer envTagsLock.Unlock()
	envTagsLock.Lock()
	envTagKeys := os.Getenv(defaultEnvTagKeys)
	if len(envTagKeys) == 0 || len(EnvTags) > 0 {
		return
	}
	for _, envKey := range strings.Split(envTagKeys, "|") {
		EnvTags = append(EnvTags, envKey)
		EnvTags = append(EnvTags, os.Getenv(envKey))
	}
}

// HasEnvTags check if specific tags exist in envTags
func HasEnvTags(tagKey string, tagValue string) bool {
	for i := 0; i < len(EnvTags)-1; i += 2 {
		if EnvTags[i] == tagKey && EnvTags[i+1] == tagValue {
			return true
		}
	}
	return false
}

func init() {
	LoadEnvTags()
}
