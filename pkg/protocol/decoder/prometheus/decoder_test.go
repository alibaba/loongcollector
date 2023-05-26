// Copyright 2021 iLogtail Authors
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

package prometheus

import (
	"fmt"
	"net/http"
	"reflect"
	"testing"

	"github.com/prometheus/common/model"
	"github.com/prometheus/prometheus/prompb"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/models"
)

var textFormat = `# HELP http_requests_total The total number of HTTP requests.
# TYPE http_requests_total counter
http_requests_total{method="post",code="200"} 1027 1395066363000
http_requests_total{method="post",code="400"}    3 1395066363000

# Escaping in label values:
msdos_file_access_time_seconds{path="C:\\DIR\\FILE.TXT",error="Cannot find file:\n\"FILE.TXT\""} 1.458255915e9

# Minimalistic line:
metric_without_timestamp_and_labels 12.47

# A weird metric from before the epoch:
something_weird{problem="division by zero"} +Inf -3982045

# A histogram, which has a pretty complex representation in the text format:
# HELP http_request_duration_seconds A histogram of the request duration.
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.05"} 24054
http_request_duration_seconds_bucket{le="0.1"} 33444
http_request_duration_seconds_bucket{le="0.2"} 100392
http_request_duration_seconds_bucket{le="0.5"} 129389
http_request_duration_seconds_bucket{le="1"} 133988
http_request_duration_seconds_bucket{le="+Inf"} 144320
http_request_duration_seconds_sum 53423
http_request_duration_seconds_count 144320

# Finally a summary, which has a complex representation, too:
# HELP telemetry_requests_metrics_latency_microseconds A summary of the response latency.
# TYPE telemetry_requests_metrics_latency_microseconds summary
telemetry_requests_metrics_latency_microseconds{quantile="0.01"} 3102
telemetry_requests_metrics_latency_microseconds{quantile="0.05"} 3272
telemetry_requests_metrics_latency_microseconds{quantile="0.5"} 4773
telemetry_requests_metrics_latency_microseconds{quantile="0.9"} 9001
telemetry_requests_metrics_latency_microseconds{quantile="0.99"} 76656
telemetry_requests_metrics_latency_microseconds_sum 1.7560473e+07
telemetry_requests_metrics_latency_microseconds_count 2693
`

func TestNormal(t *testing.T) {
	decoder := &Decoder{}
	req, _ := http.NewRequest("GET", "http://localhost", nil)
	logs, err := decoder.Decode([]byte(textFormat), req, nil)
	assert.Nil(t, err)
	assert.Equal(t, len(logs), 20)
	for _, log := range logs {
		fmt.Printf("%s \n", log.String())
	}
}

func TestDecodeV2(t *testing.T) {
	decoder := &Decoder{}
	req, _ := http.NewRequest("GET", "http://localhost", nil)
	groupeEventsSlice, err := decoder.DecodeV2([]byte(textFormat), req)
	assert.Nil(t, err)
	metricCount := 0
	for _, pg := range groupeEventsSlice {
		for _, event := range pg.Events {
			metricCount++
			fmt.Printf("%s \n", event.(*models.Metric).String())
		}
	}

	assert.Equal(t, 20, metricCount)
}

func BenchmarkDecodeV2(b *testing.B) {
	promRequest := &prompb.WriteRequest{
		Timeseries: []prompb.TimeSeries{
			{
				Labels: []prompb.Label{
					{Name: metricNameKey, Value: "test_metric"},
					{Name: "label1", Value: "value1"}},
				Samples: []prompb.Sample{
					{Timestamp: 1234567890, Value: 1.23},
					{Timestamp: 1234567891, Value: 2.34}}}},
	}
	bytes, _ := promRequest.Marshal()

	decoder := &Decoder{}
	req, _ := http.NewRequest("GET", "http://localhost", nil)
	req.Header.Add(contentEncodingKey, snappyEncoding)
	req.Header.Add(contentTypeKey, pbContentType)
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		_, err := decoder.DecodeV2(bytes, req)
		assert.Nil(b, err)
	}
}

func TestConvertPromRequestToPipelineGroupEvents(t *testing.T) {
	// Create a sample prometheus write request
	promRequest := &prompb.WriteRequest{
		Timeseries: []prompb.TimeSeries{
			{
				Labels: []prompb.Label{
					{Name: metricNameKey, Value: "test_metric"},
					{Name: "label1", Value: "value1"}},
				Samples: []prompb.Sample{
					{Timestamp: 1234567890, Value: 1.23},
					{Timestamp: 1234567891, Value: 2.34}}}},
	}

	data, err := promRequest.Marshal()
	assert.Nil(t, err)

	metaInfo := models.NewMetadataWithKeyValues("meta_name", "test_meta_name")
	commonTags := models.NewTagsWithKeyValues(
		"common_tag1", "common_value1",
		"common_tag2", "common_value2")

	groupEvent, err := ConvertPromRequestToPipelineGroupEvents(data, metaInfo, commonTags)
	assert.NoError(t, err)

	assert.Equal(t, "test_meta_name", groupEvent.Group.Metadata.Get("meta_name"))
	assert.Equal(t, 2, groupEvent.Group.Tags.Len())
	assert.Equal(t, "common_value1", groupEvent.Group.Tags.Get("common_tag1"))
	assert.Equal(t, "common_value2", groupEvent.Group.Tags.Get("common_tag2"))

	// Assert that the events were created correctly
	if len(groupEvent.Events) != 2 {
		t.Errorf("Expected 2 events, but got %d", len(groupEvent.Events))
	}
	if groupEvent.Events[0].GetName() != "test_metric" {
		t.Errorf("Expected event name to be 'test_metric', but got '%s'", groupEvent.Events[0].GetName())
	}

	metric1, ok := groupEvent.Events[0].(*models.Metric)
	assert.True(t, ok)

	assert.Equal(t, models.MetricTypeGauge, metric1.MetricType)
	assert.Equal(t, 1, metric1.Tags.Len())
	assert.Equal(t, "value1", metric1.Tags.Get("label1"))
	assert.Equal(t, uint64(1234567890000000), metric1.Timestamp)
	assert.Equal(t, 1.23, metric1.Value.GetSingleValue())

	metric2, ok := groupEvent.Events[1].(*models.Metric)
	assert.True(t, ok)
	assert.Equal(t, models.MetricTypeGauge, metric2.MetricType)
	assert.Equal(t, 1, metric2.Tags.Len())
	assert.Equal(t, "value1", metric1.Tags.Get("label1"))
	assert.Equal(t, uint64(1234567891000000), metric2.Timestamp)
	assert.Equal(t, 2.34, metric2.Value.GetSingleValue())
}

func TestParsePromPbToPipelineGroupEvents(t *testing.T) {
	promRequest := &prompb.WriteRequest{
		Timeseries: []prompb.TimeSeries{
			{
				Labels: []prompb.Label{
					{Name: metricNameKey, Value: "test_metric"},
					{Name: "label1", Value: "value1"}},
				Samples: []prompb.Sample{
					{Timestamp: 1234567890, Value: 1.23},
					{Timestamp: 1234567891, Value: 2.34}},
			},
			{
				Labels: []prompb.Label{
					{Name: metricNameKey, Value: "test_metric"},
					{Name: "label2", Value: "value2"}},
				Samples: []prompb.Sample{
					{Timestamp: 1234567890, Value: 1.23},
					{Timestamp: 1234567891, Value: 2.34}},
			},
		},
	}
	bytes, err := promRequest.Marshal()
	assert.Nil(t, err)

	group, err := ParsePromPbToPipelineGroupEventsUnsafe(bytes, models.NewMetadata(), models.NewTags())
	assert.Nil(t, err)
	assert.NotNil(t, group)
	assert.Equal(t, &models.PipelineGroupEvents{
		Group: models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{
			models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567890*1e6, 1.23),
			models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567891*1e6, 2.34),
			models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567890*1e6, 1.23),
			models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567891*1e6, 2.34),
		},
	}, group)
}

func TestConvertExpFmtDataToPipelineGroupEvents(t *testing.T) {
	var data = `# HELP http_requests_total The total number of HTTP requests. 
# TYPE http_requests_total counter
http_requests_total{method="GET",code="200"} 100 1680073018671516463
http_requests_total{method="POST",code="200"} 50 1680073018671516463
http_requests_total{method="GET",code="404"} 10 1680073018671516463
`

	inputData := []byte(data)
	metaInfo := models.NewMetadataWithKeyValues("meta_name", "test_meta_name")
	commonTags := models.NewTagsWithKeyValues(
		"common_tag1", "common_value1",
		"common_tag2", "common_value2")

	// Expected output
	expectedPG := []*models.PipelineGroupEvents{
		{
			Group: &models.GroupInfo{
				Metadata: metaInfo,
				Tags:     commonTags,
			},
			Events: []models.PipelineEvent{
				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "GET", "code", "200"),
					1680073018671516463,
					100,
				),

				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "POST", "code", "200"),
					1680073018671516463,
					50,
				),

				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "GET", "code", "404"),
					1680073018671516463,
					10,
				),
			},
		},
	}

	pg, err := ConvertExpFmtDataToPipelineGroupEvents(inputData, metaInfo, commonTags)
	assert.NoError(t, err)
	assert.Equal(t, len(pg), len(expectedPG))

	for i, expected := range expectedPG {
		actual := pg[i]

		if !reflect.DeepEqual(expected.Group, actual.Group) {
			t.Errorf("Expected metadata %v, but got %v", expected.Group, actual.Group)
		}

		assert.Equal(t, len(expected.Events), len(actual.Events))

		for j, expectedEvent := range expected.Events {
			actualEvent := actual.Events[j]
			assert.Equal(t, expectedEvent.GetName(), actualEvent.GetName())
			expectedMetric, ok := expectedEvent.(*models.Metric)
			assert.True(t, ok)
			actualMetric, ok := actualEvent.(*models.Metric)
			assert.True(t, ok)
			assert.Equal(t, expectedMetric.Value.GetSingleValue(), actualMetric.Value.GetSingleValue())
			assert.Equal(t, expectedMetric.Tags, actualMetric.Tags)
		}

	}
}

func TestConvertVectorToPipelineGroupEvents(t *testing.T) {
	// Create a sample vector with three metrics
	vector := &model.Vector{
		&model.Sample{
			Metric:    model.Metric{model.MetricNameLabel: "http_requests_total", "method": "GET", "code": "200"},
			Timestamp: model.Time(1680073018671516463),
			Value:     100,
		},
		&model.Sample{
			Metric:    model.Metric{model.MetricNameLabel: "http_requests_total", "method": "POST", "code": "200"},
			Timestamp: model.Time(1680073018671516463),
			Value:     50,
		},
		&model.Sample{
			Metric:    model.Metric{model.MetricNameLabel: "http_requests_total", "method": "GET", "code": "404"},
			Timestamp: model.Time(1680073018671516463),
			Value:     10,
		},
	}

	// Create some sample metadata and tags
	metaInfo := models.NewMetadataWithKeyValues("meta_name", "test_meta_name")
	commonTags := models.NewTagsWithKeyValues(
		"common_tag1", "common_value1",
		"common_tag2", "common_value2")

	// Expected output
	expectedPG := []*models.PipelineGroupEvents{
		{
			Group: &models.GroupInfo{
				Metadata: metaInfo,
				Tags:     commonTags,
			},
			Events: []models.PipelineEvent{
				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "GET", "code", "200"),
					1680073018671516463,
					100,
				),

				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "POST", "code", "200"),
					1680073018671516463,
					50,
				),

				models.NewSingleValueMetric(
					"http_requests_total",
					models.MetricTypeGauge,
					models.NewTagsWithKeyValues("method", "GET", "code", "404"),
					1680073018671516463,
					10,
				),
			},
		},
	}

	g, err := ConvertVectorToPipelineGroupEvents(vector, metaInfo, commonTags)
	pg := []*models.PipelineGroupEvents{g}
	assert.NoError(t, err)
	assert.Equal(t, len(pg), len(expectedPG))

	for i, expected := range expectedPG {
		actual := pg[i]

		if !reflect.DeepEqual(expected.Group, actual.Group) {
			t.Errorf("Expected metadata %v, but got %v", expected.Group, actual.Group)
		}

		assert.Equal(t, len(expected.Events), len(actual.Events))

		for j, expectedEvent := range expected.Events {
			actualEvent := actual.Events[j]
			assert.Equal(t, expectedEvent.GetName(), actualEvent.GetName())
			expectedMetric, ok := expectedEvent.(*models.Metric)
			assert.True(t, ok)
			actualMetric, ok := actualEvent.(*models.Metric)
			assert.True(t, ok)
			assert.Equal(t, expectedMetric.Value.GetSingleValue(), actualMetric.Value.GetSingleValue())
			assert.Equal(t, expectedMetric.Tags, actualMetric.Tags)
		}

	}
}
