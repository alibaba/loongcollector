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

package validator

import (
	"bufio"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"

	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/test/config"
)

var flags = []string{
	"abcjfsaldjflasdf", "deffjhaslfnvjdfh", "crffskladfjsalkdf",
}

func prepare() {
	config.LogDir = "./"
	fd, _ := os.OpenFile(config.LogDir+"logtail_plugin.LOG", os.O_RDWR|os.O_CREATE|os.O_APPEND, 0644)
	writer := bufio.NewWriter(fd)
	for i := 0; i < 1000000; i++ {
		for _, flag := range flags {
			writer.WriteString(flag + "\n")
		}
	}
	writer.Flush()
	defer fd.Close()
}

func clean() {
	_ = os.Remove(config.LogDir + "logtail_plugin.LOG")
}

func Test_logtailLogValidator_FetchResult(t *testing.T) {
	prepare()
	defer clean()
	s := new(logtailLogValidator)
	_ = s.Start()
	count, err := s.lineCounter()
	assert.NoError(t, err)
	assert.Equal(t, 1000000*len(flags), count)

	sv, err := NewSystemValidator(logtailLogValidatorName, map[string]interface{}{
		"expect_contains_log_times": map[string]int{
			"jfsald":  1000000,
			"aslfn":   1000000,
			"adfjsal": 1000000,
		},
	})
	_ = sv.Start()
	assert.NoError(t, err)
	result := sv.FetchResult()
	assert.Equal(t, 0, len(result), "got %v", result)
}

func Benchmark(b *testing.B) {
	prepare()
	defer clean()
	b.ResetTimer()
	sv, _ := NewSystemValidator(logtailLogValidatorName, map[string]interface{}{
		"expect_contains_log_times": map[string]int{
			"jfsald":  1000000,
			"aslfn":   1000000,
			"adfjsal": 1000000,
		},
	})
	_ = sv.Start()
	for i := 0; i < b.N; i++ {
		sv.FetchResult()
	}
}
