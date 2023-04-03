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
	"testing"
)

func TestEnvConfig(t *testing.T) {
	EnvTags = nil
	os.Setenv("ALIYUN_LOG_ENV_TAGS", "a|b|c")
	os.Setenv("a", "1")
	os.Setenv("b", "2")

	LoadEnvTags()

	if EnvTags[0] != "a" {
		t.Error("error")
	}
	if EnvTags[1] != "1" {
		t.Error("error")
	}
	if EnvTags[2] != "b" {
		t.Error("error")
	}
	if EnvTags[3] != "2" {
		t.Error("error")
	}
	if EnvTags[4] != "c" {
		t.Error("error")
	}
	if EnvTags[5] != "" {
		t.Error("error")
	}

	if HasEnvTags("a", "b") {
		t.Error("error")
	}

	if HasEnvTags("a", "2") {
		t.Error("error")
	}

	if !HasEnvTags("a", "1") {
		t.Error("error")
	}
	if !HasEnvTags("b", "2") {
		t.Error("error")
	}
	if !HasEnvTags("c", "") {
		t.Error("error")
	}

	os.Unsetenv("ALIYUN_LOG_ENV_TAGS")
	os.Unsetenv("1")
	os.Unsetenv("2")
}
