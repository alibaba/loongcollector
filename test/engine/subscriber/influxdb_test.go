package subscriber

import (
	"encoding/json"
	"testing"

	"github.com/alibaba/ilogtail/pkg/protocol"

	client "github.com/influxdata/influxdb1-client/v2"
	"github.com/stretchr/testify/assert"
)

func TestInfluxdbSubscriber_queryRecords2LogGroup(t *testing.T) {
	cases := []struct {
		resultStr        string
		wantLogGroup     *protocol.LogGroup
		wantMaxTimestamp int64
	}{
		{
			resultStr: `[
			{
			  "statement_id": 0,
			  "Series": [
				{
				  "name": "weather",
				  "tags": {
					"location": "us-midwest"
				  },
				  "columns": [
					"time",
					"temperature"
				  ],
				  "values": [
					[
					  1668507423504962600,
					  82
					],
					[
					  1668507423517745400,
					  83
					]
				  ]
				}
			  ],
			  "Messages": null
			}
		  ]`,
			wantLogGroup: &protocol.LogGroup{
				Logs: []*protocol.Log{
					{
						Contents: []*protocol.Log_Content{
							{Key: "__name__", Value: "weather.temperature"},
							{Key: "__labels__", Value: "location#$#us-midwest"},
							{Key: "__time_nano__", Value: "1668507423504962600"},
							{Key: "__value__", Value: "82"},
							{Key: "__type__", Value: "float"},
						},
					},
					{
						Contents: []*protocol.Log_Content{
							{Key: "__name__", Value: "weather.temperature"},
							{Key: "__labels__", Value: "location#$#us-midwest"},
							{Key: "__time_nano__", Value: "1668507423517745400"},
							{Key: "__value__", Value: "83"},
							{Key: "__type__", Value: "float"},
						},
					},
				},
			},
			wantMaxTimestamp: 1668507423517745400,
		},
	}

	for _, testCase := range cases {
		var results []client.Result
		err := json.Unmarshal([]byte(testCase.resultStr), &results)
		assert.Nil(t, err)

		s := &InfluxdbSubscriber{}
		logGroup, maxTimestamp := s.queryRecords2LogGroup(results)
		assert.Equal(t, testCase.wantLogGroup, logGroup)
		assert.Equal(t, testCase.wantMaxTimestamp, maxTimestamp)
	}
}
