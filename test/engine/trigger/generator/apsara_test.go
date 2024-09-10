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
	"io"
	"os"
	"strconv"
	"testing"
	"time"
)

// TestGenerateApsara will be executed in the environment being collected.
func TestGenerateApsara(t *testing.T) {
	gneratedLogDir := getEnvOrDefault("GENERATED_LOG_DIR", "/tmp/ilogtail")
	totalLog, err := strconv.Atoi(getEnvOrDefault("TOTAL_LOG", "100"))
	if err != nil {
		t.Fatalf("parse TOTAL_LOG failed: %v", err)
		return
	}
	interval, err := strconv.Atoi(getEnvOrDefault("INTERVAL", "1"))
	if err != nil {
		t.Fatalf("parse INTERVAL failed: %v", err)
		return
	}
	fileName := getEnvOrDefault("FILENAME", "apsara.log")

	testLogConent := []string{
		"[%s]\t[ERROR]\t[32337]\t[/build/core/application/Application:12]\tfile:file0\tlogNo:1199997\tmark:-\tmsg:hello world!",
		"[%s]\t[ERROR]\t[20964]\t[/build/core/ilogtail.cpp:127]\tfile:file0\tlogNo:1199998\tmark:F\tmsg:这是一条消息",
		"[%s]\t[WARNING]\t[32337]\t[/build/core/ilogtail.cpp:127]\tfile:file0\tlogNo:1199999\tmark:-\tmsg:hello world!",
		"[%s]\t[INFO]\t[32337]\t[/build/core/ilogtail.cpp:127]\tfile:file0\tlogNo:1200000\tmark:-\tmsg:这是一条消息",
		"[%s]\t[ERROR]\t[00001]\t[/build/core/ilogtail.cpp:127]\tfile:file0\tlogNo:1199992\tmark:-\tmsg:password:123456",
		"[%s]\t[DEBUG]\t[32337]\t[/build/core/ilogtail.cpp:127]\tfile:file0\tlogNo:1199993\tmark:-\tmsg:hello world!",
	}
	file, err := os.OpenFile(fmt.Sprintf("%s/%s", gneratedLogDir, fileName), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		t.Fatalf("open file failed: %v", err)
		return
	}
	defer file.Close()

	logIndex := 0
	for i := 0; i < totalLog; i++ {
		var currentTime string
		if i%2 == 0 {
			currentTime = time.Now().Format("2006-01-02 15:04:05.000000")
		} else {
			currentTime = strconv.FormatInt(time.Now().UnixNano()/1000, 10)
		}
		_, err := io.WriteString(file, fmt.Sprintf(testLogConent[logIndex]+"\n", currentTime))
		if err != nil {
			t.Fatalf("write log failed: %v", err)
			return
		}
		time.Sleep(time.Duration(interval * int(time.Millisecond)))
		logIndex++
		if logIndex >= len(testLogConent) {
			logIndex = 0
		}
	}
}
