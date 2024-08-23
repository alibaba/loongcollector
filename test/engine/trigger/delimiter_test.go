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

// TestGenerateDelimiterSingle will be executed in the environment being collected.
func TestGenerateDelimiterSingle(t *testing.T) {
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
		"'-' 'file0' '13196' '0.0.0.0' '%s' 'GET' '/index.html' 'HTTP/2.0' '302' '628' 'curl/7.10'",
		"'-' 'file0' '13197' '10.45.26.0' '%s' 'GET' '/' 'HTTP/2.0' '302' '218' 'go-sdk'",
		"'-' 'file0' '13198' '10.45.26.0' '%s' 'GET' '/dir/resource.txt' 'HTTP/1.1' '404' '744' 'Mozilla/5.0'",
		"'-' 'file0' '13199' '127.0.0.1' '%s' 'PUT' '/' 'HTTP/2.0' '200' '320' 'curl/7.10'",
		"'-' 'file0' '13200' '192.168.0.3' '%s' 'PUT' '/dir/resource.txt' 'HTTP/1.1' '404' '949' 'curl/7.10'",
	}
	file, err := os.OpenFile(fmt.Sprintf("%s/%s", gneratedLogDir, fileName), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		t.Fatalf("open file failed: %v", err)
		return
	}
	defer file.Close()

	logIndex := 0
	for i := 0; i < totalLog; i++ {
		currentTime := time.Now().Format("2006-01-02 15:04:05.000000000")
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
