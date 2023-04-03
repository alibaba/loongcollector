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

package helper

import (
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestReviseFileOffset(t *testing.T) {
	var mock protocol.Log
	mock.Contents = append(mock.Contents, &protocol.Log_Content{Key: FileOffsetKey, Value: "1000"})
	mock.Time = uint32(time.Now().Unix())

	tests := []struct {
		name   string
		log    *protocol.Log
		offset int64
		want   int64
	}{
		{
			name:   "revise_file",
			log:    &mock,
			offset: 101,
			want:   1101,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ReviseFileOffset(tt.log, 101, true)
			cont := GetFileOffsetTag(tt.log)
			assert.True(t, cont != nil)
			off, err := strconv.ParseInt(cont.Value, 10, 64)
			assert.True(t, err == nil)
			assert.Equal(t, tt.want, off)
		})
	}
}
