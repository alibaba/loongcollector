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
package trigger

import (
	"fmt"
	"io"
	"os"
	"strconv"
	"testing"
	"time"
)

// TestGenerateRegexLogSingle will be executed in the environment being collected.
func TestGenerateRegexLogSingle(t *testing.T) {
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
	fileName := getEnvOrDefault("FILENAME", "regex_single")

	testLogConent := []string{
		`- file2:1 127.0.0.1 - [2024-01-07T12:40:10.505120] "HEAD / HTTP/2.0" 302 809 "未知" "这是一条消息，password:123456"`,
		`- file2:2 127.0.0.1 - [2024-01-07T12:40:11.392101] "GET /index.html HTTP/2.0" 200 139 "Mozilla/5.0" "这是一条消息，password:123456，这是第二条消息，password:00000"`,
		`- file2:3 10.45.26.0 - [2024-01-07T12:40:12.359314] "PUT /index.html HTTP/1.1" 200 913 "curl/7.10" "这是一条消息"`,
		`- file2:4 192.168.0.3 - [2024-01-07T12:40:13.002661] "PUT /dir/resource.txt HTTP/2.0" 501 355 "go-sdk" "这是一条消息，password:123456"`,
	}
	file, err := os.OpenFile(fmt.Sprintf("%s/%s", gneratedLogDir, fileName), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		t.Fatalf("open file failed: %v", err)
		return
	}
	defer file.Close()

	logIndex := 0
	for i := 0; i < totalLog; i++ {
		_, err := io.WriteString(file, testLogConent[logIndex]+"\n")
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
