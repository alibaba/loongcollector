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

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherSleepExport(t *testing.T) {
	log := models.NewSimpleLog([]byte("test"), models.NewTags(), 100)
	flusher := &FlusherSleep{SleepMS: 0}
	require.NoError(t, flusher.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{{Events: []models.PipelineEvent{log}}}, nil))
}
