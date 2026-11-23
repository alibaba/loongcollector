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

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherCheckerExportRecordsEventsNatively(t *testing.T) {
	log := models.NewSimpleLog([]byte("message"), models.NewTagsWithKeyValues("kind", "log"), 1700000000123456789)
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTags(), 1700000000123456789, 0.5)
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}

	checker := &FlusherChecker{}
	require.NoError(t, checker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, checker.Export([]*models.PipelineGroupEvents{group}, nil))

	require.Equal(t, 2, checker.GetLogCount())
	require.Len(t, checker.GroupEvents, 1)
	require.Same(t, group, checker.GroupEvents[0])
	require.Equal(t, "message", string(checker.GroupEvents[0].Events[0].(*models.Log).GetBody()))
}
