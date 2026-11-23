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

package stdout

import (
	"encoding/json"
	"testing"

	jsoniter "github.com/json-iterator/go"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestWriteLogBodyContentEncoding(t *testing.T) {
	tests := []struct {
		name    string
		value   any
		decoded any
		raw     string
	}{
		{name: "empty string", value: "", decoded: ""},
		{
			name:    "escaped string",
			value:   "你好 <&> \"quote\" \\ \t\n\r \x00\x1b\x7f \u2028\u2029",
			decoded: "你好 <&> \"quote\" \\ \t\n\r \x00\x1b\x7f \u2028\u2029",
		},
		{name: "integer fallback", value: 42, decoded: float64(42)},
		{name: "boolean fallback", value: true, decoded: true},
		{name: "byte slice fallback", value: []byte{0xff}, raw: "\"field\":[]byte{0xff}"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			log := &models.Log{Contents: models.NewLogContents()}
			log.Contents.Add("field", test.value)
			writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
			writer.WriteObjectStart()
			new(FlusherStdout).writeLogBody(writer, log)
			writer.WriteObjectEnd()

			if test.raw != "" {
				require.Contains(t, string(writer.Buffer()), test.raw)
				return
			}
			var fields map[string]any
			require.NoError(t, json.Unmarshal(writer.Buffer(), &fields))
			require.Equal(t, test.decoded, fields["field"])
		})
	}
}
