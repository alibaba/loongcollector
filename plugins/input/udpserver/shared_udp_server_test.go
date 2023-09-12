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

package udpserver

import (
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestSharedUDPServer_cutDispatchTag(t *testing.T) {

	tests := []struct {
		name        string
		dispatchKey string
		wantTag     string
		wantVal     string
	}{
		{
			"first one",
			"key1",
			"val1",
			"key2#$#val2|key3#$#val3",
		},
		{
			"mid one",
			"key2",
			"val2",
			"key1#$#val1|key3#$#val3",
		},
		{
			"last one",
			"key3",
			"val3",
			"key1#$#val1|key2#$#val2",
		},
		{
			"not exist",
			"key4",
			"",
			"key1#$#val1|key2#$#val2|key3#$#val3",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := &SharedUDPServer{
				dispatchKey: tt.dispatchKey,
				lastLog:     time.Now(),
			}
			nowTime := time.Now()
			log := &protocol.Log{
				Contents: []*protocol.Log_Content{
					{
						Key:   labelName,
						Value: "key1#$#val1|key2#$#val2|key3#$#val3",
					},
				},
			}
			protocol.SetLogTime(log, uint32(nowTime.Unix()))

			if gotTag := s.cutDispatchTag(log); gotTag != tt.wantTag || log.Contents[0].Value != tt.wantVal {
				t.Errorf("cutDispatchTag() = %v, want %v, got val = %v, want val= %v",
					gotTag, tt.wantTag, log.Contents[0].Value, tt.wantVal)
			}
		})

	}
}
