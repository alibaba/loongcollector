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

package doc

import (
	"io/ioutil"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

type TestDoc struct {
	Field1 int      `json:"field_1" comment:"field one"`
	Field2 string   `json:"field_2" comment:"field two"`
	Field3 int64    `json:"field_3" mapstructure:"field_33" comment:"field three"`
	Field4 []string `json:"field_4" comment:"field four"`
	//nolint:unused,structcheck
	ignoreField string
}

func (t TestDoc) Description() string {
	return "this is a test doc demo"
}

func Test_extractDocConfig(t *testing.T) {

	config := extractDocConfig(new(TestDoc))
	assert.Equal(t, []*FieldConfig{
		{
			Name:    "field_1",
			Type:    "int",
			Comment: "field one",
		},
		{
			Name:    "field_2",
			Type:    "string",
			Comment: "field two",
		},
		{
			Name:    "field_33",
			Type:    "int64",
			Comment: "field three",
		},
		{
			Name:    "field_4",
			Type:    "string slice",
			Comment: "field four",
		},
	}, config)

}

func Test_generatePluginDoc(t *testing.T) {
	str := `# test-plugin
## Description
this is a test doc demo
## Config
|  field   |   type   |   description   |
| ---- | ---- | ---- |
|field_1|int|field one|
|field_2|string|field two|
|field_33|int64|field three|
|field_4|string slice|field four|`
	generatePluginDoc("test-plugin.md", "test-plugin", new(TestDoc))
	bytes, _ := ioutil.ReadFile("test-plugin.md")
	assert.Equal(t, str, string(bytes))
	_ = os.Remove("test-plugin.md")
}
