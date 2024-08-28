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

package command

import (
	"encoding/base64"
	"fmt"
	"os"
	"os/user"
	"runtime"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestCommandTestCollecetUserBase64WithTimeout(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)

	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	c := new(test.MockMetricCollector)
	// Script set to sleep for 5 seconds
	scriptContent := `sleep 5 &&  echo -e "__labels__:a#\$#1|b#\$#2    __value__:0  __name__:metric_command_example \n __labels__:a#\$#3|b#\$#4    __value__:3  __name__:metric_command_example2"`

	// base64
	p.ScriptContent = base64.StdEncoding.EncodeToString([]byte(scriptContent))
	p.ContentEncoding = "Base64"
	p.TimeoutMilliSeconds = 6000
	p.ScriptType = "shell"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	p.User = "runner"
	if _, err = p.Init(ctx); err != nil {
		t.Errorf("cannot init InputCommand: %v", err)
		return
	}
	err = p.Collect(c)
	if err == nil {
		t.Errorf("expect error with timeout")
	}
}

func TestCommandTestCollecetUserBase64(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)
	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	c := new(test.MockMetricCollector)
	scriptContent := `echo -e "__labels__:a#\$#1|b#\$#2    __value__:0  __name__:metric_command_example \n __labels__:a#\$#3|b#\$#4    __value__:3  __name__:metric_command_example2"`
	// base64
	p.ScriptType = "shell"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	p.ScriptContent = base64.StdEncoding.EncodeToString([]byte(scriptContent))
	p.ContentEncoding = "Base64"
	p.User = "runner"
	if _, err := p.Init(ctx); err != nil {
		t.Errorf("cannot init InputCommand: %v", err)
		return
	}
	if err := p.Collect(c); err != nil {
		t.Errorf("Collect() error = %v", err)
		return
	}
}

func TestCommandTestCollect(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)

	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	c := new(test.MockMetricCollector)

	p.ScriptContent = `echo "test"`
	p.ScriptType = "shell"
	p.ContentEncoding = "PlainText"
	p.User = "runner"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	if _, err := p.Init(ctx); err != nil {
		t.Errorf("cannot init InputCommand: %v", err)
		return
	}
	if err := p.Collect(c); err != nil {
		t.Errorf("Collect() error = %v", err)
		return
	}
}

func TestCommandTestExceptionCollect(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)
	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	c := new(test.MockMetricCollector)

	p.ScriptContent = `echo "1"`
	p.ScriptType = "shell"
	p.ContentEncoding = "PlainText"
	p.User = "runner"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	if _, err := p.Init(ctx); err != nil {
		t.Errorf("cannot init InputCommand: %v", err)
		return
	}
	if err := p.Collect(c); err != nil {
		t.Errorf("Collect() error = %v", err)
		return
	}

}

func TestCommandTestTimeoutCollect(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)
	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	c := new(test.MockMetricCollector)

	p.ScriptContent = `sleep 10`
	p.ScriptType = "shell"
	p.ContentEncoding = "PlainText"
	p.User = "runner"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	if _, err := p.Init(ctx); err != nil {
		t.Errorf("cannot init InputCommand: %v", err)
		return
	}
	if err := p.Collect(c); err != nil {
		if err.Error() == "exec cmd error errInfo:exec command timed out, stderr:, stdout:" {
			fmt.Println(err.Error())
		} else {
			t.Errorf("Collect() error = %v", err)
			return
		}

	}
}

func TestCommandTestInit(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)

	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	_, err = p.Init(ctx)
	require.Error(t, err)
	if err != nil {
		fmt.Println("default config error", err)
	}

	// Test for wrong script type
	p.ScriptType = "golang_none"
	_, err = p.Init(ctx)
	require.Error(t, err)
	if err != nil {
		fmt.Println("expect error with script type not support", err)
	}
	p.ScriptType = "bash"
	if runtime.GOOS == "darwin" {
		p.CmdPath = "/bin/sh"
	}
	// Test the wrong User
	p.User = "root"
	_, err = p.Init(ctx)
	require.Error(t, err)
	if err != nil {
		fmt.Println("expect error with wrong user root", err)
	}
	p.User = "runner"

	// test contentType
	p.ContentEncoding = "mixin"
	_, err = p.Init(ctx)
	require.Error(t, err)
	if err != nil {
		fmt.Println("expect error with ContentType error", err)
	}
	p.ContentEncoding = defaultContentType

	// The test script content is empty
	p.ScriptContent = ""
	_, err = p.Init(ctx)
	require.Error(t, err)
	if err != nil {
		fmt.Println("expect error with ScriptContent empty error", err)
	}
	p.ScriptContent = "some"
	// Test execution script timed out
	// Test the dataType of the output
	p.IntervalMs = 3000
	p.TimeoutMilliSeconds = 4000
	_, err = p.Init(ctx)
	if err != nil {
		fmt.Println("expect error with ExecScriptTimeOut > IntervalMs ", err)
	}
}

// test script storage
func TestScriptStorage(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)

	content := `echo -e "__labels__:hostname#\$#idc_cluster_env_name|ip#\$#ip_address    __value__:0  __name__:metric_command_example"`
	err = mkdir("./")
	if err != nil {
		t.Errorf("create Storage error %s", err)
		return
	}
	filepath, err := saveContent("./", content, "TestScriptStorage", "shell")
	if err != nil {
		t.Errorf("ScriptStorage save content error %s", err)
		return
	}
	data, err := os.ReadFile(filepath)
	if err != nil {
		t.Errorf("read file error")
		return
	}
	if string(data) != content {
		t.Errorf("content compare error")
		return
	}

	fmt.Print("\n---TestScriptStorage filepath", filepath, "\n")

	// Get again
	filepath, _ = saveContent("./", content, "TestScriptStorage", "shell")
	data, _ = os.ReadFile(filepath)
	if string(data) != content {
		t.Errorf("content compare error")
		return
	}
}

func TestErrorCmdPath(t *testing.T) {
	u, err := user.Current()
	if err != nil {
		t.Errorf("get user.Current() error %s", err)
		return
	}
	fmt.Printf("Username %s\n", u.Username)

	ctx := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputCommand)
	p.ScriptContent = `echo "test"`
	p.ScriptType = "shell"
	p.ContentEncoding = "PlainText"
	p.User = "runner"
	p.CmdPath = "rm -rf *"
	if _, err = p.Init(ctx); err != nil {
		if err.Error() == "CmdPath rm -rf * does not exist, err:stat rm -rf *: no such file or directory" {
			fmt.Println(err)
		} else {
			t.Errorf("cannot init InputCommand: %v", err)
			return
		}
	}
}
