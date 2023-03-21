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

package fmtstr

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestFormatString(t *testing.T) {
	topic := "kafka_%{app_name}"
	fields := map[string]string{
		"app_name": "ilogtail",
	}

	expectTopic := "kafka_ilogtail"

	sf, _ := Compile(topic, func(key string, ops []VariableOp) (FormatEvaler, error) {
		if v, found := fields[key]; found {
			return StringElement{v}, nil
		}
		return StringElement{key}, nil
	})
	actualTopic, _ := sf.Run(nil)

	assert.Equal(t, expectTopic, actualTopic)
}

func TestCompileKeys(t *testing.T) {
	topic := "kafka_%{app_name}"
	expectTopicKeys := []string{"app_name"}
	topicKeys, _ := CompileKeys(topic)
	assert.Equal(t, expectTopicKeys, topicKeys)
}
