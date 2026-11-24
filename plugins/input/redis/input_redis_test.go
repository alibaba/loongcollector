// Copyright 2026 iLogtail Authors
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

package redis

import (
	"bufio"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestGatherInfoOutputAddressFieldsInContents(t *testing.T) {
	tests := []struct {
		name          string
		addressFields map[string]string
		info          string
		want          map[string]string
	}{
		{
			name:          "tcp",
			addressFields: map[string]string{"server": "localhost", "port": "6379"},
			info:          "port:6380\n",
			want:          map[string]string{"server": "localhost", "port": "6380"},
		},
		{
			name:          "unix",
			addressFields: map[string]string{"socket": "/tmp/redis.sock"},
			want:          map[string]string{"socket": "/tmp/redis.sock"},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ctx := helper.NewObservePipelineContext(1)
			reader := bufio.NewReader(strings.NewReader("connected_clients:2\n" + tt.info))

			gatherInfoOutput(reader, ctx.Collector(), tt.addressFields)

			groups := ctx.Collector().ToArray()
			require.Len(t, groups, 1)
			require.Len(t, groups[0].Events, 1)
			log := groups[0].Events[0].(*models.Log)
			for key, value := range tt.want {
				assert.Equal(t, value, log.Contents.Get(key))
			}
			assert.Equal(t, "2", log.Contents.Get("clients"))
			assert.Zero(t, log.Tags.Len())
		})
	}
	assert.Equal(t, "6379", tests[0].addressFields["port"])
}
