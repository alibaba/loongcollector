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

package checker

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherChecker_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushChecker := &FlusherChecker{}
	require.NoError(t, flushChecker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, flushChecker.Flush("", "", "", v1Groups))

	exportChecker := &FlusherChecker{}
	require.NoError(t, exportChecker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, exportChecker.Export(v2Groups, nil))

	v1JSON, err := flusherparity.LogsToNormalizedJSON(flushChecker.LogGroup.Logs)
	require.NoError(t, err)
	v2JSON, err := flusherparity.LogsToNormalizedJSON(exportChecker.LogGroup.Logs)
	require.NoError(t, err)
	require.Equal(t, v1JSON, v2JSON)
}
