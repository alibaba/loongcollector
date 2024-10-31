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
	"testing"
	"time"
)

// TestGenerateDelimiterSingle will be executed in the environment being collected.
func TestGenerateDelimiterSingle(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv("Delimiter", "Quote")
	if err != nil {
		t.Fatalf("get generate file log config from env failed: %v", err)
		return
	}
	delimiter := config.Custom["Delimiter"]
	if delimiter == "" {
		delimiter = " "
	}
	quote := config.Custom["Quote"]
	if quote == "" {
		quote = ""
	}
	testLogContentTmpl := string2Template([]string{
		"{{.Quote}}{{.Mark}}{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}0.0.0.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/index.html{{.Quote}}{{.Delimiter}}{{.Quote}}HTTP/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}302{{.Quote}}{{.Delimiter}}{{.Quote}}628{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
		"{{.Quote}}{{.Mark}}{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}10.45.26.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/{{.Quote}}{{.Delimiter}}{{.Quote}}HTTP/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}302{{.Quote}}{{.Delimiter}}{{.Quote}}218{{.Quote}}{{.Delimiter}}{{.Quote}}go-sdk{{.Quote}}\n",
		"{{.Quote}}{{.Mark}}{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}10.45.26.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/dir/resource.txt{{.Quote}}{{.Delimiter}}{{.Quote}}HTTP/1.1{{.Quote}}{{.Delimiter}}{{.Quote}}404{{.Quote}}{{.Delimiter}}{{.Quote}}744{{.Quote}}{{.Delimiter}}{{.Quote}}Mozilla/5.0{{.Quote}}\n",
		"{{.Quote}}{{.Mark}}{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}127.0.0.1{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}PUT{{.Quote}}{{.Delimiter}}{{.Quote}}/{{.Quote}}{{.Delimiter}}{{.Quote}}HTTP/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}200{{.Quote}}{{.Delimiter}}{{.Quote}}320{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
		"{{.Quote}}{{.Mark}}{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}192.168.0.3{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}PUT{{.Quote}}{{.Delimiter}}{{.Quote}}/dir/resource.txt{{.Quote}}{{.Delimiter}}{{.Quote}}HTTP/1.1{{.Quote}}{{.Delimiter}}{{.Quote}}404{{.Quote}}{{.Delimiter}}{{.Quote}}949{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
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
		err = testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Mark":      getRandomMark(),
			"FileNo":    fileNo,
			"LogNo":     logNo,
			"Time":      time.Now().Format("2006-01-02 15:04:05.000000000"),
			"Delimiter": delimiter,
			"Quote":     quote,
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

// TestGenerateDelimiterMultiline will be executed in the environment being collected.
func TestGenerateDelimiterMultiline(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv("Delimiter", "Quote")
	if err != nil {
		t.Fatalf("get generate file log config from env failed: %v", err)
		return
	}
	delimiter := config.Custom["Delimiter"]
	if delimiter == "" {
		delimiter = " "
	}
	quote := config.Custom["Quote"]
	if quote == "" {
		quote = ""
	}
	testLogContentTmpl := string2Template([]string{
		"{{.Quote}}F{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}0.0.0.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/index.html{{.Quote}}{{.Delimiter}}{{.Quote}}\nHTTP\n/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}302{{.Quote}}{{.Delimiter}}{{.Quote}}628{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
		"{{.Quote}}-{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}10.45.26.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/{{.Quote}}{{.Delimiter}}{{.Quote}}\nHTTP\n/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}302{{.Quote}}{{.Delimiter}}{{.Quote}}218{{.Quote}}{{.Delimiter}}{{.Quote}}go-sdk{{.Quote}}\n",
		"{{.Quote}}F{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}10.45.26.0{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}GET{{.Quote}}{{.Delimiter}}{{.Quote}}/dir/resource.txt{{.Quote}}{{.Delimiter}}{{.Quote}}\nHTTP\n/1.1{{.Quote}}{{.Delimiter}}{{.Quote}}404{{.Quote}}{{.Delimiter}}{{.Quote}}744{{.Quote}}{{.Delimiter}}{{.Quote}}Mozilla/5.0{{.Quote}}\n",
		"{{.Quote}}-{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}127.0.0.1{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}PUT{{.Quote}}{{.Delimiter}}{{.Quote}}/{{.Quote}}{{.Delimiter}}{{.Quote}}\nHTTP\n/2.0{{.Quote}}{{.Delimiter}}{{.Quote}}200{{.Quote}}{{.Delimiter}}{{.Quote}}320{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
		"{{.Quote}}F{{.Quote}}{{.Delimiter}}{{.Quote}}file{{.FileNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}{{.LogNo}}{{.Quote}}{{.Delimiter}}{{.Quote}}192.168.0.3{{.Quote}}{{.Delimiter}}{{.Quote}}{{.Time}}{{.Quote}}{{.Delimiter}}{{.Quote}}PUT{{.Quote}}{{.Delimiter}}{{.Quote}}/dir/resource.txt{{.Quote}}{{.Delimiter}}{{.Quote}}\nHTTP\n/1.1{{.Quote}}{{.Delimiter}}{{.Quote}}404{{.Quote}}{{.Delimiter}}{{.Quote}}949{{.Quote}}{{.Delimiter}}{{.Quote}}curl/7.10{{.Quote}}\n",
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
		err = testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"FileNo":    fileNo,
			"LogNo":     logNo,
			"Time":      time.Now().Format("2006-01-02 15:04:05.000000000"),
			"Delimiter": delimiter,
			"Quote":     quote,
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
