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
	"reflect"
	"testing"
)

func TestLoadEnvTagsVariablePrecedence(t *testing.T) {
	tests := []struct {
		name     string
		loong    string
		loongLog string
		aliyun   string
		want     []string
	}{
		{name: "LOONG_ENV_TAGS only", loong: "ENV_TAGS_TEST_NEW", want: []string{"ENV_TAGS_TEST_NEW", "new"}},
		{name: "LOONG_LOG_ENV_TAGS only", loongLog: "ENV_TAGS_TEST_OLD", want: []string{"ENV_TAGS_TEST_OLD", "old"}},
		{name: "ALIYUN_LOG_ENV_TAGS only", aliyun: "ENV_TAGS_TEST_ALIYUN", want: []string{"ENV_TAGS_TEST_ALIYUN", "aliyun"}},
		{name: "all configured", loong: "ENV_TAGS_TEST_NEW", loongLog: "ENV_TAGS_TEST_OLD", aliyun: "ENV_TAGS_TEST_ALIYUN", want: []string{"ENV_TAGS_TEST_NEW", "new"}},
		{name: "empty LOONG_ENV_TAGS", loongLog: "ENV_TAGS_TEST_OLD", aliyun: "ENV_TAGS_TEST_ALIYUN", want: []string{"ENV_TAGS_TEST_OLD", "old"}},
		{name: "no configured tags"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Setenv("LOONG_ENV_TAGS", tt.loong)
			t.Setenv("LOONG_LOG_ENV_TAGS", tt.loongLog)
			t.Setenv("ALIYUN_LOG_ENV_TAGS", tt.aliyun)
			t.Setenv("ENV_TAGS_TEST_NEW", "new")
			t.Setenv("ENV_TAGS_TEST_OLD", "old")
			t.Setenv("ENV_TAGS_TEST_ALIYUN", "aliyun")

			previous := EnvTags
			t.Cleanup(func() { EnvTags = previous })
			EnvTags = nil

			LoadEnvTags()
			if !reflect.DeepEqual(EnvTags, tt.want) {
				t.Errorf("EnvTags = %q, want %q", EnvTags, tt.want)
			}
		})
	}
}

func TestEnvConfig(t *testing.T) {
	previous := EnvTags
	t.Cleanup(func() { EnvTags = previous })
	EnvTags = nil
	t.Setenv("LOONG_ENV_TAGS", "")
	t.Setenv("LOONG_LOG_ENV_TAGS", "")
	t.Setenv("ALIYUN_LOG_ENV_TAGS", "a|b|c")
	t.Setenv("a", "1")
	t.Setenv("b", "2")
	t.Setenv("c", "")

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
}
