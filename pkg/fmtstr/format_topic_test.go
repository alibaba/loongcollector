// Copyright 2023 iLogtail Authors
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

package fmtstr

import (
	"testing"

	"github.com/smartystreets/goconvey/convey"
)

func TestFormatTopic(t *testing.T) {
	convey.Convey("Kafka topic format  ", t, func() {
		targetValues := map[string]string{
			"app": "ilogtail",
		}
		kafkaTopic := "test_%{app}"
		actualTopic, _ := FormatTopic(targetValues, kafkaTopic)
		desiredTopic := "test_ilogtail"
		convey.So(desiredTopic, convey.ShouldEqual, *actualTopic)
	})
}
