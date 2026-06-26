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

package statistics

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherStatistics_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushFlusher := &FlusherStatistics{RateIntervalMs: 1_000_000, GeneratePB: true, SleepMsPerLogGroup: 0}
	require.NoError(t, flushFlusher.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))
	flushGroups := flushFlusher.loggroupRateCounter.Rate()

	exportFlusher := &FlusherStatistics{RateIntervalMs: 1_000_000, GeneratePB: true, SleepMsPerLogGroup: 0}
	require.NoError(t, exportFlusher.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, exportFlusher.Export(v2Groups, nil))
	exportGroups := exportFlusher.loggroupRateCounter.Rate()

	require.Equal(t, flushGroups, exportGroups)
	require.Equal(t, flushFlusher.logRateCounter.Rate(), exportFlusher.logRateCounter.Rate())
}
