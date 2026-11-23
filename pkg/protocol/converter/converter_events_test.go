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

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestConvertToSingleProtocolStreamMixedEvents(t *testing.T) {
	c, err := NewConverter(ProtocolCustomSingle, EncodingJSON, nil, nil)
	require.NoError(t, err)

	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1e9)
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("host", "h1"), 2e9, 0.5)
	span := models.NewSpan("op", "trace-id", "span-id", models.SpanKindServer, 3e9, 4e9, models.NewTags(), nil, nil)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric, span},
	}

	stream, _, err := c.ConvertToSingleProtocolStream(groupEvents, nil)
	require.NoError(t, err)
	assert.Len(t, stream, 3)
	// Metric is converted structurally into a canonical metric log, never a
	// pass-through blob.
	assert.NotContains(t, string(stream[1]), "__pipeline_passthrough__")
	assert.Contains(t, string(stream[1]), metricNameKey)
	assert.Contains(t, string(stream[1]), `cpu`)
	assert.Contains(t, string(stream[1]), metricValueKey)
	// Span is converted structurally with its own fields.
	assert.NotContains(t, string(stream[2]), "__pipeline_passthrough__")
	assert.Contains(t, string(stream[2]), "traceID")
	assert.Contains(t, string(stream[2]), "trace-id")
}

func TestEncodeEventRecordsConvertsMetricStructurally(t *testing.T) {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTags(), 1)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 2, 1.0)
	groupEvents := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{log, metric},
	}

	rows, err := EncodeEventRecords(groupEvents)
	require.NoError(t, err)
	require.Len(t, rows, 2)
	assert.Equal(t, "hello", rows[0].Fields["content"])
	assert.Equal(t, "m", rows[1].Fields[metricNameKey])
	assert.Equal(t, "1", rows[1].Fields[metricValueKey])
	assert.NotContains(t, rows[1].Fields, "__pipeline_passthrough__")
}

func TestEventConverterSelectedFieldsAndRename(t *testing.T) {
	log := models.NewSimpleLog([]byte("<body>"), models.NewTagsWithKeyValues("tenant", "event", "omit", "private"), 1700000000123456789)
	log.Contents.Add("method", "PUT")
	group := &models.PipelineGroupEvents{Group: models.NewGroup(nil, models.NewTagsWithKeyValues("tenant", "group", "region", "cn")), Events: []models.PipelineEvent{log}}
	for _, protocolName := range []string{ProtocolCustomSingle, ProtocolCustomSingleFlatten, ProtocolJsonline} {
		t.Run(protocolName, func(t *testing.T) {
			converter, err := NewConverter(protocolName, EncodingJSON,
				map[string]string{"tenant": "renamed", "omit": ""},
				map[string]string{"contents": "fields", "time": "timestamp"})
			require.NoError(t, err)
			payload, selected, err := converter.ToByteStreamWithSelectedFields(group, []string{"content.method", "tag.tenant", "tag.region"})
			require.NoError(t, err)
			switch protocolName {
			case ProtocolCustomSingle:
				rows := payload.([][]byte)
				require.Len(t, rows, 1)
				require.JSONEq(t, `{"fields":{"content":"<body>","method":"PUT"},"tags":{"region":"cn","renamed":"group"},"timestamp":1700000000}`, string(rows[0]))
				require.Contains(t, string(rows[0]), "<body>")
			case ProtocolCustomSingleFlatten:
				require.JSONEq(t, `{"content":"<body>","method":"PUT","region":"cn","renamed":"group","timestamp":1700000000}`, string(payload.([][]byte)[0]))
			case ProtocolJsonline:
				require.JSONEq(t, `{"content":"<body>","method":"PUT","region":"cn","renamed":"group","timestamp":1700000000}`, string(payload.([]byte)))
				return
			}
			require.Equal(t, []map[string]string{{"content.method": "PUT", "tag.tenant": "group", "tag.region": "cn"}}, selected)
			_, _, err = converter.ToByteStreamWithSelectedFields(group, []string{"invalid.method"})
			require.ErrorContains(t, err, "unsupported field")
		})
	}
}
