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

// TestGenerateApsara will be executed in the environment being collected.
func TestGenerateApsara(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get generate file log config from env failed: %v", err)
		return
	}
	testLogContentTmpl := string2Template([]string{
		"[{{.Time}}]\t[{{.Level}}]\t[32337]\t[/build/core/application/Application:12]\tfile:file{{.FileNo}}\tlogNo:{{.LogNo}}\tmark:{{.Mark}}\tmsg:hello world!\n",
		"[{{.Time}}]\t[{{.Level}}]\t[20964]\t[/build/core/ilogtail.cpp:127]\tfile:file{{.FileNo}}\tlogNo:{{.LogNo}}\tmark:{{.Mark}}\tmsg:这是一条消息\n",
		"[{{.Time}}]\t[{{.Level}}]\t[32337]\t[/build/core/ilogtail.cpp:127]\tfile:file{{.FileNo}}\tlogNo:{{.LogNo}}\tmark:{{.Mark}}\tmsg:hello world!\n",
		"[{{.Time}}]\t[{{.Level}}]\t[32337]\t[/build/core/ilogtail.cpp:127]\tfile:file{{.FileNo}}\tlogNo:{{.LogNo}}\tmark:{{.Mark}}\tmsg:这是一条消息\n",
		"[{{.Time}}]\t[{{.Level}}]\t[00001]\t[/build/core/ilogtail.cpp:127]\tfile:file{{.FileNo}}\tlogNo:{{.LogNo}}\tmark:{{.Mark}}\tmsg:password:123456\n",
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
			currentTime = time.Now().Format("2006-01-02 15:04:05.000000")
		} else {
			currentTime = strconv.FormatInt(time.Now().UnixNano()/1000, 10)
		}
		err = testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Time":   currentTime,
			"Level":  getRandomLogLevel(),
			"LogNo":  logNo + i,
			"FileNo": fileNo,
			"Mark":   getRandomMark(),
		})
		if err != nil {
			t.Fatalf("write log failed: %v", err)
			return
		}
		time.Sleep(time.Duration(config.Interval * int(time.Millisecond)))
		logIndex++
		if logIndex >= len(testLogContentTmpl) {
			logIndex = 0
		}
	}
}
