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
package ebpf

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/trigger"
)

/*
********************
input_process_security
********************
*/
func ProcessSecurityEvents(ctx context.Context, commandCnt int) (context.Context, error) {
	time.Sleep(5 * time.Second)
	if err := execveCommands(ctx, commandCnt); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func execveCommands(ctx context.Context, commandCnt int) error {
	execveCommand := "ps -ef | grep loongcollector-e2e-test"
	for i := 0; i < commandCnt; i++ {
		if _, err := setup.Env.ExecOnSource(ctx, execveCommand); err != nil {
			return err
		}
	}
	return nil
}

func ExecveCommandsParallel(ctx context.Context, commandCnt int, command string) (context.Context, error) {
	time.Sleep(5 * time.Second)

	errChan := make(chan error, commandCnt)
	for i := 0; i < commandCnt; i++ {
		go func() {
			_, err := setup.Env.ExecOnSource(ctx, command)
			if err != nil {
				logger.Error(ctx, "ASYNC_EXEC_FAILED", "error", err, "message", "Error executing command asynchronously")
				errChan <- fmt.Errorf("async exec failed: %w", err)
				return
			}
		}()
	}

	const maxWaitTime = 1 * time.Second
	select {
	case err := <-errChan:
		if err != nil {
			return ctx, fmt.Errorf("some async shell script executions failed: %w", err)
		}
	case <-time.After(maxWaitTime):
		return ctx, nil
	}

	return ctx, nil
}

func ExecveCommandsSerial(ctx context.Context, commandCnt int, command string) (context.Context, error) {
	time.Sleep(5 * time.Second)
	for i := 0; i < commandCnt; i++ {
		_, err := setup.Env.ExecOnSource(ctx, command)
		if err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}

func isValidFilename(name string) bool {
	return name != "" && !strings.ContainsAny(name, "/\\")
}

func CreateShellScript(ctx context.Context, tempFileName, content string) (context.Context, error) {
	if !isValidFilename(tempFileName) {
		return ctx, fmt.Errorf("invalid temp file name: %s", tempFileName)
	}

	command := fmt.Sprintf(`cat << 'SCRIPT_END' > %s.sh
%s
SCRIPT_END`, tempFileName, content)
	if _, err := setup.Env.ExecOnSource(ctx, command); err != nil {
		return ctx, err
	}

	command = fmt.Sprintf(`chmod +x %s.sh`, tempFileName)
	if _, err := setup.Env.ExecOnSource(ctx, command); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func ExecuteShellScriptSerial(ctx context.Context, commandCnt int, tempFileName string) (context.Context, error) {
	time.Sleep(5 * time.Second)
	command := fmt.Sprintf(`./%s.sh`, tempFileName)
	for i := 0; i < commandCnt; i++ {
		result, err := setup.Env.ExecOnSource(ctx, command)
		if err != nil {
			return ctx, err
		}
		logger.Debug(ctx, "Shell Script output", "result", result)
	}
	return ctx, nil
}

func ExecuteShellScriptParallel(ctx context.Context, commandCnt int, tempFileName string) (context.Context, error) {
	time.Sleep(5 * time.Second)
	command := fmt.Sprintf(`./%s.sh`, tempFileName)

	errChan := make(chan error, commandCnt)
	for i := 0; i < commandCnt; i++ {
		go func() {
			result, err := setup.Env.ExecOnSource(ctx, command)
			if err != nil {
				logger.Error(ctx, "ASYNC_EXEC_FAILED", "error", err, "message", "Error executing command asynchronously")
				errChan <- fmt.Errorf("async exec failed: %w", err)
				return
			}
			logger.Debug(ctx, "Shell Script output", "result", result)
		}()
	}

	const maxWaitTime = 1 * time.Second
	select {
	case err := <-errChan:
		if err != nil {
			return ctx, fmt.Errorf("some async shell script executions failed: %w", err)
		}
	case <-time.After(maxWaitTime):
		return ctx, nil
	}

	return ctx, nil
}

func RemoveShellScript(ctx context.Context, tempFileName string) (context.Context, error) {
	command := fmt.Sprintf(`rm %s.sh`, tempFileName)
	if _, err := setup.Env.ExecOnSource(ctx, command); err != nil {
		return ctx, err
	}
	return ctx, nil
}

/*
********************
input_network_security
********************
*/
func NetworksSecurityEvents(ctx context.Context, commandCnt int, url string) (context.Context, error) {
	time.Sleep(5 * time.Second)
	if err := curlURL(ctx, commandCnt, url); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func curlURL(ctx context.Context, commandCnt int, url string) error {
	curlCommand := "curl --connect-timeout 1 " + url + ";"
	for i := 0; i < commandCnt; i++ {
		if _, err := setup.Env.ExecOnSource(ctx, curlCommand); err != nil {
			return err
		}
	}
	return nil
}

/*
********************
input_file_security
********************
*/

func FileSecurityEvents(ctx context.Context, commandCnt int, filenames string) (context.Context, error) {
	time.Sleep(5 * time.Second)
	if err := rwFile(ctx, commandCnt, filenames); err != nil {
		return ctx, err
	}
	if err := mmapFile(ctx, commandCnt, filenames); err != nil {
		return ctx, err
	}
	if err := truncateFile(ctx, commandCnt, filenames); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func rwFile(ctx context.Context, commandCnt int, filenames string) error {
	files := strings.Split(filenames, ",")
	for _, file := range files {
		touchFileCommand := "touch " + file + ";"
		if _, err := setup.Env.ExecOnSource(ctx, touchFileCommand); err != nil {
			return err
		}
		catFileCommand := "echo 'Hello, World!' >> " + file + ";"
		for i := 0; i < commandCnt; i++ {
			if _, err := setup.Env.ExecOnSource(ctx, catFileCommand); err != nil {
				return err
			}
		}
		removeCommand := "rm " + file + ";"
		if _, err := setup.Env.ExecOnSource(ctx, removeCommand); err != nil {
			return err
		}
	}
	return nil
}

func mmapFile(ctx context.Context, commandCnt int, filenames string) error {
	files := strings.Split(filenames, ",")
	for _, file := range files {
		mmapFileCommand := trigger.GetRunTriggerCommand("ebpf", "mmap", "commandCnt", strconv.FormatInt(int64(commandCnt), 10), "filename", file)
		if _, err := setup.Env.ExecOnSource(ctx, mmapFileCommand); err != nil {
			return err
		}
	}
	return nil
}

func truncateFile(ctx context.Context, commandCnt int, filenames string) error {
	files := strings.Split(filenames, ",")
	for _, file := range files {
		truncateFileCommand1 := "truncate -s 10k " + file + ";"
		truncateFileCommand2 := "truncate -s 0 " + file + ";"
		for i := 0; i < commandCnt/2; i++ {
			if _, err := setup.Env.ExecOnSource(ctx, truncateFileCommand1); err != nil {
				return err
			}
			if _, err := setup.Env.ExecOnSource(ctx, truncateFileCommand2); err != nil {
				return err
			}
		}
		if commandCnt%2 != 0 {
			if _, err := setup.Env.ExecOnSource(ctx, truncateFileCommand1); err != nil {
				return err
			}
		}
	}
	return nil
}
