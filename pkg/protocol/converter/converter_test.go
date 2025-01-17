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
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestInvalidProtocol(t *testing.T) {
	Convey("When constructing converter with invalid protocol", t, func() {
		_, err := NewConverter("xml", "pb", nil, nil, &config.GlobalConfig{})

		Convey("Then error should be returned", func() {
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a converter with unsupported protocol", t, func() {
		c := &Converter{
			Protocol: "xml",
			Encoding: "pb",
		}

		Convey("When performing conversion", func() {
			logs := []*protocol.Log{
				{
					Time: 1662434209,
					Contents: []*protocol.Log_Content{
						{Key: "method", Value: "PUT"},
					},
				},
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
				{Key: "__path__", Value: "/root/test/origin/example.log"},
				{Key: "__log_topic__", Value: "file"},
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "topic",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			_, err := c.Do(logGroup)

			Convey("Then error should be returned", func() {
				So(err, ShouldNotBeNil)
			})
		})
	})
}
