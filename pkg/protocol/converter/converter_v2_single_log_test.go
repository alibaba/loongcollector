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

package protocol

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestConvertToSingleProtocolStreamV2MixedEvents(t *testing.T) {
	c, err := NewConverter(ProtocolCustomSingle, EncodingJSON, nil, nil, &config.GlobalConfig{})
	require.NoError(t, err)

	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1e9)
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "h1"), 2e9, 0.5)
	span := models.NewSpan("op", "trace-id", "span-id", models.SpanKindServer, 3e9, 4e9, models.NewTags(), nil, nil)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}

	stream, _, err := c.ConvertToSingleProtocolStreamV2(groupEvents, nil)
	require.NoError(t, err)
	assert.Len(t, stream, 3)
	assert.Contains(t, string(stream[1]), passthroughLogKey)
	assert.Contains(t, string(stream[1]), `\"eventType\":\"metric\"`)
	assert.Contains(t, string(stream[2]), passthroughLogKey)
	assert.Contains(t, string(stream[2]), `\"eventType\":\"span\"`)
}

func TestPipelineGroupEventsToLogGroupPreservesPassthrough(t *testing.T) {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric},
	}

	logGroup, err := PipelineGroupEventsToLogGroup(groupEvents)
	require.NoError(t, err)
	require.Len(t, logGroup.Logs, 2)
	assert.Equal(t, passthroughLogKey, logGroup.Logs[1].Contents[0].Key)
}
