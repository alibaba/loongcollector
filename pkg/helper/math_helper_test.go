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
	"testing"

	"github.com/stretchr/testify/assert"
)

func Test_Max(t *testing.T) {
	assert.Equal(t, int(20), Max(int(10), int(20)))
	assert.Equal(t, int8(20), Max(int8(10), int8(20)))
	assert.Equal(t, int16(20), Max(int16(10), int16(20)))
	assert.Equal(t, int32(20), Max(int32(10), int32(20)))
	assert.Equal(t, int64(20), Max(int64(10), int64(20)))

	assert.Equal(t, uint(20), Max(uint(10), uint(20)))
	assert.Equal(t, uint8(20), Max(uint8(10), uint8(20)))
	assert.Equal(t, uint16(20), Max(uint16(10), uint16(20)))
	assert.Equal(t, uint32(20), Max(uint32(10), uint32(20)))
	assert.Equal(t, uint64(20), Max(uint64(10), uint64(20)))

	assert.Equal(t, float32(20.1), Max(float32(10.1), float32(20.1)))
	assert.Equal(t, float64(20.1), Max(float64(10.1), float64(20.1)))
}
