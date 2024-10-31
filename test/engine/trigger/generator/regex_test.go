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
	"bytes"
	"fmt"
	"io"
	"math/rand"
	"os"
	"testing"
	"time"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// TestGenerateRegexLogSingle will be executed in the environment being collected.
func TestGenerateRegexLogSingle(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get config failed: %v", err)
		return
	}
	testLogContentTmpl := string2Template([]string{
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 127.0.0.1 - [{{.Time}}] "HEAD / HTTP/2.0" 302 809 "未知" "这是一条消息，password:123456"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 127.0.0.1 - [{{.Time}}] "GET /index.html HTTP/2.0" 200 139 "Mozilla/5.0" "这是一条消息，password:123456，这是第二条消息，password:00000"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 10.45.26.0 - [{{.Time}}] "PUT /index.html HTTP/1.1" 200 913 "curl/7.10" "这是一条消息"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 192.168.0.3 - [{{.Time}}] "PUT /dir/resource.txt HTTP/2.0" 501 355 "go-sdk" "这是一条消息，password:123456"
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
	location, err := time.LoadLocation("Asia/Shanghai")
	if err != nil {
		t.Fatalf("load location failed: %v", err)
		return
	}
	for i := 0; i < config.TotalLog; i++ {
		err = testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Time":   time.Now().In(location).Format("2006-01-02T15:04:05.000000"),
			"Mark":   getRandomMark(),
			"FileNo": fileNo,
			"LogNo":  logNo + i,
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

// TestGenerateRegexLogSingleGBK will be executed in the environment being collected.
func TestGenerateRegexLogSingleGBK(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get config failed: %v", err)
		return
	}
	encoder := simplifiedchinese.GBK.NewEncoder()
	testLogContentTmpl := string2Template([]string{
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 127.0.0.1 - [{{.Time}}] "HEAD / HTTP/2.0" 302 809 "未知" "这是一条消息，password:123456"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 127.0.0.1 - [{{.Time}}] "GET /index.html HTTP/2.0" 200 139 "Mozilla/5.0" "这是一条消息，password:123456，这是第二条消息，password:00000"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 10.45.26.0 - [{{.Time}}] "PUT /index.html HTTP/1.1" 200 913 "curl/7.10" "这是一条消息"
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} 192.168.0.3 - [{{.Time}}] "PUT /dir/resource.txt HTTP/2.0" 501 355 "go-sdk" "这是一条消息，password:123456"
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
	location, err := time.LoadLocation("Asia/Shanghai")
	if err != nil {
		t.Fatalf("load location failed: %v", err)
		return
	}
	for i := 0; i < config.TotalLog; i++ {
		var buffer bytes.Buffer
		_ = testLogContentTmpl[logIndex].Execute(&buffer, map[string]interface{}{
			"Time":   time.Now().In(location).Format("2006-01-02T15:04:05.000000"),
			"Mark":   getRandomMark(),
			"FileNo": fileNo,
			"LogNo":  logNo + i,
		})
		data, err1 := io.ReadAll(transform.NewReader(&buffer, encoder))
		if err1 != nil {
			t.Fatalf("encode log failed: %v", err1)
		}
		_, err := io.WriteString(file, string(data))
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

func TestGenerateRegexLogMultiline(t *testing.T) {
	config, err := getGenerateFileLogConfigFromEnv()
	if err != nil {
		t.Fatalf("get config failed: %v", err)
		return
	}
	testLogContentTmpl := string2Template([]string{
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} [{{.Time}}] [{{.Level}}] java.lang.Exception: exception happened
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f1(RegexMultiLog.java:73)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.run(RegexMultiLog.java:34)
at java.base/java.lang.Thread.run(Thread.java:833)
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} [{{.Time}}] [{{.Level}}] java.lang.Exception: 发生异常
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:80)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f1(RegexMultiLog.java:75)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.run(RegexMultiLog.java:34)
at java.base/java.lang.Thread.run(Thread.java:833)
`,
		`{{.Mark}} file{{.FileNo}}:{{.LogNo}} [{{.Time}}] [{{.Level}}] java.lang.Exception: exception happened
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f5(RegexMultiLog.java:100)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f4(RegexMultiLog.java:96)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f3(RegexMultiLog.java:89)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:82)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f1(RegexMultiLog.java:75)
at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.run(RegexMultiLog.java:34)
at java.base/java.lang.Thread.run(Thread.java:833)
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
	location, err := time.LoadLocation("Asia/Shanghai")
	if err != nil {
		t.Fatalf("load location failed: %v", err)
		return
	}
	for i := 0; i < config.TotalLog; i++ {
		err = testLogContentTmpl[logIndex].Execute(file, map[string]interface{}{
			"Time":   time.Now().In(location).Format("2006-01-02T15:04:05.000000"),
			"Level":  getRandomLogLevel(),
			"FileNo": fileNo,
			"LogNo":  logNo + i,
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
