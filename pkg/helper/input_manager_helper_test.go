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

package helper

import (
	"fmt"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg"

	"github.com/stretchr/testify/assert"
)

func TestNewmanagerMeta(t *testing.T) {
	genFunc := func(prefix string, start, end int) string {
		var res []string
		for i := start; i <= end; i++ {
			res = append(res, fmt.Sprintf("%s_%d", prefix, i))
		}
		return strings.Join(res, ",")
	}
	meta := NewmanagerMeta("telegraf")
	for i := 0; i < 8; i++ {
		prjNum := i / 4
		logNum := i / 2
		cfgNum := i
		meta.Add(fmt.Sprintf("p_%d", prjNum), fmt.Sprintf("l_%d", logNum), fmt.Sprintf("c_%d", cfgNum))
		assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, genFunc("p", 0, prjNum))
		assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, genFunc("l", 0, logNum))
	}

	for i := 7; i >= 0; i-- {
		prjNum := i / 4
		logNum := i / 2
		cfgNum := i
		meta.Delete(fmt.Sprintf("p_%d", prjNum), fmt.Sprintf("l_%d", logNum), fmt.Sprintf("c_%d", cfgNum))
		cfgNum--
		if i%2 == 0 {
			logNum--
		}
		if i%4 == 0 {
			prjNum--
		}
		if prjNum == -1 && logNum == -1 && cfgNum == -1 {
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, "")
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, "")
		} else {
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, genFunc("p", 0, prjNum))
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, genFunc("l", 0, logNum))

		}
	}
}
