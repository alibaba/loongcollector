// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package generator

import (
	"fmt"
	"math/rand"
	"os"
	"strconv"
	"testing"
	"time"
)

// TestGenerateJSONSingle will be executed in the environment being collected.
func TestGenerateJSONSingle(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get generate file log config from env failed: %v", err)
		return
	}
	testLogContentTmpl := string2Template([]string{
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"POST","userAgent":"mozilla firefox","size":263}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"GET","userAgent":"go-sdk","size":569}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"HEAD","userAgent":"go-sdk","size":210}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"192.168.0.3","method":"PUT","userAgent":"curl/7.10","size":267}
`,
	})
	file, err := os.OpenFile(fmt.Sprintf("%s/%s", config.GeneratedLogDir, config.FileName), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		t.Fatalf("open file failed: %v", err)
		return
	}
	defer file.Close()

	logIndex := 0
	logNo := rand.Intn(10000)
	fileNo := rand.Intn(10000)
	for i := 0; i < config.TotalLog; i++ {
		var currentTime string
		if i%2 == 0 {
			currentTime = time.Now().Format("2006-01-02T15:04:05.999999999")
		} else {
			currentTime = strconv.FormatInt(time.Now().UnixNano()/1000, 10)
		}
		testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Mark":   getRandomMark(),
			"FileNo": fileNo,
			"LogNo":  logNo + i,
			"Time":   currentTime,
		})

		time.Sleep(time.Duration(config.Interval * int(time.Millisecond)))
		logIndex++
		if logIndex >= len(testLogContentTmpl) {
			logIndex = 0
		}
	}
}

func TestGenerateJSONMultiline(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get generate file log config from env failed: %v", err)
		return
	}
	testLogContentTmpl := string2Template([]string{
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"POST","userAgent":"mozilla firefox",
"size":263}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"GET","userAgent":"go-sdk",
"size":569}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"0.0.0.0","method":"HEAD","userAgent":"go-sdk",
"size":210}
`,
		`{"mark":"{{.Mark}}","file":"file{{.FileNo}}","logNo":{{.LogNo}},"time":"{{.Time}}","ip":"192.168.0.3","method":"PUT","userAgent":"curl/7.10",
"size":267}
`,
	})
	file, err := os.OpenFile(fmt.Sprintf("%s/%s", config.GeneratedLogDir, config.FileName), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		t.Fatalf("open file failed: %v", err)
		return
	}
	defer file.Close()

	logIndex := 0
	logNo := rand.Intn(10000)
	fileNo := rand.Intn(10000)
	for i := 0; i < config.TotalLog; i++ {
		currentTime := time.Now().Format("2006-01-02T15:04:05.999999999")
		testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Mark":   getRandomMark(),
			"FileNo": fileNo,
			"LogNo":  logNo + i,
			"Time":   currentTime,
		})

		time.Sleep(time.Duration(config.Interval * int(time.Millisecond)))
		logIndex++
		if logIndex >= len(testLogContentTmpl) {
			logIndex = 0
		}
	}
}
