// Copyright 2022 iLogtail Authors
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

package raw

import (
	"net/http"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/models"
)

var data = `
cpu.load.short,host=server01,region=cn value=0.64 
`

func TestNormal(t *testing.T) {
	decoder := &Decoder{}
	req := &http.Request{}
	byteData := []byte(data)
	groups, err := decoder.DecodeV2(byteData, req)
	assert.Nil(t, err)
	assert.Equal(t, len(groups), 1)
	assert.Equal(t, len(groups[0].Events), 1)
	event, ok := groups[0].Events[0].(models.ByteArray)
	if !ok {
		t.Errorf("raw decoder needs ByteArray")
	}
	assert.Equal(t, len(event), len(byteData))
}

func TestNormalV1(t *testing.T) {
	decoder := &Decoder{}
	req := &http.Request{}
	byteData := []byte(data)
	logs, err := decoder.Decode(byteData, req, make(map[string]string))
	assert.Nil(t, err)
	assert.Equal(t, len(logs), 1)
	assert.Equal(t, len(logs[0].Contents), 1)
	logContent := logs[0].Contents[0].Value
	assert.Equal(t, len(logContent), len(byteData))
}
