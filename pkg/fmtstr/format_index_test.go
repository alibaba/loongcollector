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
	"fmt"
	"testing"
	"time"

	"github.com/smartystreets/goconvey/convey"
)

func TestFormatIndex(t *testing.T) {
	convey.Convey("elasticsearch index format  ", t, func() {
		nowTime := time.Now().Local()
		targetValues := map[string]string{
			"app": "ilogtail",
		}
		elasticsearchIndex := "test_%{app}_%{+yyyy.ww}" // Generate index weekly
		actualIndex, _ := FormatIndex(targetValues, elasticsearchIndex, uint32(nowTime.Unix()))
		desiredIndex := fmt.Sprintf("test_ilogtail_%s.%d", nowTime.Format("2006"), GetWeek(&nowTime))
		convey.So(*actualIndex, convey.ShouldEqual, desiredIndex)

		elasticsearchIndexDaily := "test_%{app}_%{+yyyyMMdd}" // Generate index daily
		actualIndex, _ = FormatIndex(targetValues, elasticsearchIndexDaily, uint32(nowTime.Unix()))
		desiredIndex = fmt.Sprintf("test_ilogtail_%s", nowTime.Format("20060102"))
		convey.So(*actualIndex, convey.ShouldEqual, desiredIndex)
	})
}
