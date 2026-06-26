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

package kafka

import (
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
)

func TestFlusherKafka_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	var flushPayloads, exportPayloads []string
	newCapture := func(dst *[]string) FlusherFunc {
		return func(_ string, _ string, _ string, logGroupList []*protocol.LogGroup) error {
			for _, lg := range logGroupList {
				for _, log := range lg.Logs {
					buf, err := json.Marshal(log)
					require.NoError(t, err)
					*dst = append(*dst, string(buf))
				}
			}
			return nil
		}
	}

	flushFlusher := &FlusherKafka{flusher: newCapture(&flushPayloads)}
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportFlusher := &FlusherKafka{flusher: newCapture(&exportPayloads)}
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, len(flushPayloads), len(exportPayloads))
	for i := range flushPayloads {
		require.JSONEq(t, flushPayloads[i], exportPayloads[i])
	}
}
