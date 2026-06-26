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

package sleep

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherSleep_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flusher := &FlusherSleep{SleepMS: 0}
	require.NoError(t, flusher.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, flusher.Flush("", "", "", v1Groups))
	require.NoError(t, flusher.Export(v2Groups, nil))
}
