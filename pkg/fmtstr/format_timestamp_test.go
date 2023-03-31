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

package fmtstr

import (
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestFormatTime(t *testing.T) {
	someTime := time.Now().Local()
	currentStrTime := FormatTimestamp(&someTime, "yyyyMMdd")
	expectTime := time.Now().Format("20060102")
	assert.Equal(t, expectTime, currentStrTime)

	currentWeekNumber := FormatTimestamp(&someTime, "yyyy.ww")
	expectWeekNumber := fmt.Sprintf("%s.%d", someTime.Format("2006"), GetWeek(&someTime))
	assert.Equal(t, expectWeekNumber, currentWeekNumber)
}
