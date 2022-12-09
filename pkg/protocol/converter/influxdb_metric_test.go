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
	"fmt"
	"testing"

	"github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestConvertToInfluxdbProtocolStream(t *testing.T) {
	convey.Convey("Given protocol.LogGroup", t, func() {
		cases := []struct {
			name       string
			logGroup   *protocol.LogGroup
			wantStream interface{}
			wantErr    bool
		}{
			{
				name: "contains metrics without labels or timestamp part",
				logGroup: &protocol.LogGroup{
					Logs: []*protocol.Log{
						{
							Contents: []*protocol.Log_Content{
								{Key: metricNameKey, Value: "metric:field"},
								{Key: metricLabelsKey, Value: ""},
								{Key: metricValueKey, Value: "1"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: metricNameKey, Value: "metric:field"},
								{Key: metricLabelsKey, Value: "aa#$#bb"},
								{Key: metricValueKey, Value: "1"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: metricNameKey, Value: "metric:field"},
								{Key: metricValueKey, Value: "1"},
								{Key: metricTimeNanoKey, Value: "1667615389000000000"},
							},
						},
					},
				},
				wantStream: [][]byte{[]byte("metric field=1\nmetric,aa=bb field=1\nmetric field=1 1667615389000000000\n")},
			},
		}

		converter, err := NewConverter("influxdb", "custom", nil, nil)
		convey.So(err, convey.ShouldBeNil)

		for _, test := range cases {
			convey.Convey(fmt.Sprintf("When %s, result should match", test.name), func() {

				stream, _, err := converter.ConvertToInfluxdbProtocolStream(test.logGroup, nil)
				if test.wantErr {
					convey.So(err, convey.ShouldNotBeNil)
				} else {
					convey.So(err, convey.ShouldBeNil)
					convey.So(stream, convey.ShouldResemble, test.wantStream)
				}
			})
		}
	})
}
