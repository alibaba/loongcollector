// Copyright 2022 iLogtail Authors
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
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func Test_newMetricReader(t *testing.T) {
	tests := []struct {
		log        *protocol.Log
		wantReader *metricReader
		wantErr    bool
	}{
		{
			log: &protocol.Log{
				Contents: []*protocol.Log_Content{
					{Key: metricNameKey, Value: "name"},
					{Key: metricLabelsKey, Value: "aaa#$#bbb"},
					{Key: metricValueKey, Value: "1"},
					{Key: metricTimeNanoKey, Value: "12344"},
				},
			},
			wantReader: &metricReader{
				name:      "name",
				labels:    "aaa#$#bbb",
				value:     "1",
				timestamp: "12344",
			},
		},
		{
			log: &protocol.Log{
				Contents: []*protocol.Log_Content{
					{Key: metricNameKey, Value: ""},
					{Key: metricLabelsKey, Value: "aaa#$#bbb"},
					{Key: metricValueKey, Value: "1"},
					{Key: metricTimeNanoKey, Value: "12344"},
				},
			},
			wantErr: true,
		},
		{
			log: &protocol.Log{
				Contents: []*protocol.Log_Content{
					{Key: metricNameKey, Value: "name"},
					{Key: metricLabelsKey, Value: "aaa#$#bbb"},
					{Key: metricValueKey, Value: ""},
					{Key: metricTimeNanoKey, Value: "12344"},
				},
			},
			wantErr: true,
		},
	}

	for _, test := range tests {
		reader := newMetricReader()
		err := reader.set(test.log)
		if test.wantErr {
			assert.NotNil(t, err)
			continue
		}
		assert.Nil(t, err)
		assert.Equal(t, test.wantReader, reader)
	}
}

func Test_metricReader_readNames(t *testing.T) {
	tests := []struct {
		reader         *metricReader
		wantMetricName string
		wantFieldName  string
	}{
		{
			reader: &metricReader{
				name: "aa",
			},
			wantMetricName: "aa",
			wantFieldName:  "value",
		},
		{
			reader: &metricReader{
				name: "aa:bb",
			},
			wantMetricName: "aa:bb",
			wantFieldName:  "value",
		},
		{
			reader: &metricReader{
				name:      "aa:bb",
				fieldName: "bb",
			},
			wantMetricName: "aa",
			wantFieldName:  "bb",
		},
		{
			reader: &metricReader{
				name: ":",
			},
			wantMetricName: ":",
			wantFieldName:  "value",
		},
		{
			reader: &metricReader{
				name:      "aa:value",
				fieldName: "value",
			},
			wantMetricName: "aa:value",
			wantFieldName:  "value",
		},
	}

	for _, test := range tests {
		metricName, fieldName := test.reader.readNames()
		assert.Equal(t, test.wantMetricName, metricName)
		assert.Equal(t, test.wantFieldName, fieldName)
	}
}

func Test_metricReader_readSortedLabels(t *testing.T) {
	tests := []struct {
		reader     *metricReader
		wantLabels []MetricLabel
		wantErr    bool
	}{
		{
			reader: &metricReader{
				labels: "bb#$#aa|aa#$#bb",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: "bb"},
				{Key: "bb", Value: "aa"},
			},
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa",
			},
			wantLabels: []MetricLabel{
				{Key: "bb", Value: "aa"},
			},
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa|aa#$#",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: ""},
				{Key: "bb", Value: "aa"},
			},
		},
		{
			reader: &metricReader{
				labels: "",
			},
			wantLabels: nil,
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa|aa#$#bb|",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: "bb"},
				{Key: "bb", Value: "aa"},
			},
		},
		{
			reader: &metricReader{
				labels: "bb",
			},
			wantErr: true,
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa|eee",
			},
			wantLabels: []MetricLabel{
				{Key: "bb", Value: "aa|eee"},
			},
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa|eee|aa#$#bb",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: "bb"},
				{Key: "bb", Value: "aa|eee"},
			},
		},
		{
			reader: &metricReader{
				labels: "bb#$#aa||eee||aa#$#bb",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: "bb"},
				{Key: "bb", Value: "aa||eee|"},
			},
		},
		{
			reader: &metricReader{
				labels: "cc||bb#$#aa||eee||aa#$#bb",
			},
			wantLabels: []MetricLabel{
				{Key: "aa", Value: "bb"},
				{Key: "cc||bb", Value: "aa||eee|"},
			},
		},
	}

	for _, test := range tests {
		labels, err := test.reader.readSortedLabels()
		if test.wantErr {
			assert.NotNilf(t, err, "reader", *test.reader)
			continue
		}
		assert.Nil(t, err)
		assert.Equal(t, test.wantLabels, labels)
	}
}

func Test_metricReader_countLabels(t *testing.T) {
	tests := []struct {
		reader    *metricReader
		wantCount int
	}{
		{
			reader: &metricReader{
				labels: "aa#$#bb",
			},
			wantCount: 1,
		},
		{
			reader: &metricReader{
				labels: "",
			},
			wantCount: 0,
		},
		{
			reader: &metricReader{
				labels: "aa#$#bb|bb#$#cc",
			},
			wantCount: 2,
		},
	}

	for _, test := range tests {
		n := test.reader.countLabels()
		assert.Equal(t, test.wantCount, n)
	}
}

func Test_metricReader_readValue(t *testing.T) {
	tests := []struct {
		reader    *metricReader
		wantValue interface{}
		wantErr   bool
	}{
		{
			reader: &metricReader{
				value:     "1",
				valueType: valueTypeBool,
			},
			wantValue: true,
		},
		{
			reader: &metricReader{
				value:     "1",
				valueType: valueTypeInt,
			},
			wantValue: int64(1),
		},
		{
			reader: &metricReader{
				value:     "1.1",
				valueType: valueTypeFloat,
			},
			wantValue: float64(1.1),
		},
		{
			reader: &metricReader{
				value: "",
			},
			wantErr: true,
		},
		{
			reader: &metricReader{
				value:     "aaa",
				valueType: valueTypeString,
			},
			wantValue: "aaa",
		},
	}

	for _, test := range tests {
		value, err := test.reader.readValue()
		if test.wantErr {
			assert.NotNil(t, err)
			continue
		}
		assert.Equal(t, test.wantValue, value)
	}
}

func Test_metricReader_readTimestamp(t *testing.T) {
	tests := []struct {
		reader   *metricReader
		wantTime time.Time
		wantErr  bool
	}{
		{
			reader: &metricReader{
				timestamp: strconv.FormatInt(time.Date(2022, 01, 01, 01, 01, 01, 01, time.UTC).UnixNano(), 10),
			},
			wantTime: time.Date(2022, 01, 01, 01, 01, 01, 01, time.UTC),
		},
		{
			reader: &metricReader{
				timestamp: "",
			},
			wantTime: time.Time{},
		},
		{
			reader: &metricReader{
				timestamp: "abc",
			},
			wantErr: true,
		},
	}

	for _, test := range tests {
		timestamp, err := test.reader.readTimestamp()
		if test.wantErr {
			assert.NotNil(t, err)
			continue
		}
		assert.Nil(t, err)
		assert.Equal(t, test.wantTime, timestamp)
	}
}

func Test_metricReader_set(t *testing.T) {
	tests := []struct {
		log        *protocol.Log
		wantErr    bool
		wantReader metricReader
	}{
		{
			log: &protocol.Log{Contents: []*protocol.Log_Content{
				{Key: metricNameKey, Value: "_metric"},
				{Key: metricFieldKey, Value: "value"},
				{Key: metricValueTypeKey, Value: "float"},
				{Key: metricValueKey, Value: "1"},
			}},
			wantErr: false,
			wantReader: metricReader{
				name:      "_metric",
				value:     "1",
				valueType: "float",
				fieldName: "value",
			},
		},
		{
			log: &protocol.Log{Contents: []*protocol.Log_Content{
				{Key: metricNameKey, Value: "_metric"},
				{Key: metricFieldKey, Value: "value"},
				{Key: metricValueTypeKey, Value: "string"},
				{Key: metricValueKey, Value: ""},
			}},
			wantErr: false,
			wantReader: metricReader{
				name:      "_metric",
				value:     "",
				valueType: "string",
				fieldName: "value",
			},
		},
		{
			log: &protocol.Log{Contents: []*protocol.Log_Content{
				{Key: metricNameKey, Value: "_metric"},
				{Key: metricFieldKey, Value: "value"},
				{Key: metricValueTypeKey, Value: "float"},
				{Key: metricValueKey, Value: ""},
			}},
			wantErr: true,
		},
	}

	for _, test := range tests {
		reader := metricReader{}
		err := reader.set(test.log)
		if test.wantErr {
			assert.NotNil(t, err)
			continue
		}
		assert.Nil(t, err)
		assert.Equal(t, test.wantReader, reader)
	}
}
