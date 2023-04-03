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

package defaultdecoder

import (
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol/decoder/influxdb"
)

func TestInit_Should_Pass_The_Config_To_Real_Decoder(t *testing.T) {
	d := &ExtensionDefaultDecoder{}
	err := json.Unmarshal([]byte(`{"Format":"influxdb","FieldsExtend":true}`), d)
	assert.Nil(t, err)
	assert.Equal(t, map[string]interface{}{"FieldsExtend": true}, d.options)

	d.Init(nil)
	assert.NotNil(t, d.Decoder)
	assert.Equal(t, true, d.Decoder.(*influxdb.Decoder).FieldsExtend)
}
