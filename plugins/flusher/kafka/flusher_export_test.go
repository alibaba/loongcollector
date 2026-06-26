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
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/flusher/exportutil"
)

// TestFlusherKafkaExportDefaultProtocol covers the custom_single Export path used by FlusherKafka.Export.
func TestFlusherKafkaExportDefaultProtocol(t *testing.T) {
	var flushed int
	groups := mixedLogMetricSpanGroups()
	err := exportutil.ExportLogOnly(groups, "", "", "", func(_ string, _ string, _ string, logGroupList []*protocol.LogGroup) error {
		for _, lg := range logGroupList {
			flushed += len(lg.Logs)
		}
		return nil
	})
	require.NoError(t, err)
	assert.Equal(t, 3, flushed)
}

func mixedLogMetricSpanGroups() []*models.PipelineGroupEvents {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	span := models.NewSpan("op", "trace", "span", models.SpanKindServer, 3, 4, models.NewTags(), nil, nil)
	return []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}}
}
