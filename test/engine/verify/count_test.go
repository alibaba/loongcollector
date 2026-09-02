// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package verify

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestCountLogsWithFilter(t *testing.T) {
	groups := []*protocol.LogGroup{
		{
			Logs: []*protocol.Log{
				metricLog("_source_", "stdout", "content", "hello"),
				metricLog("_source_", "stderr", "content", "warning"),
				metricLog("_source_", "stdout", "_source_", "stdout"),
			},
		},
		nil,
		{
			Logs: []*protocol.Log{
				nil,
				metricLog("content", "ignored"),
			},
		},
	}

	require.Equal(t, 2, countLogsWithFilter(groups, "_source_", "stdout"))
	require.Equal(t, 1, countLogsWithFilter(groups, "_source_", "stderr"))
	require.Zero(t, countLogsWithFilter(groups, "content", "missing"))
}
