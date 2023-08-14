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
	"bytes"
	"crypto/md5" //nolint:gosec
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/alibaba/ilogtail/pkg/util"
)

func mkdir(dataDir string) error {
	// if the dir exists
	isExist, err := util.PathExists(dataDir)
	if err != nil {
		err = fmt.Errorf("PathExists %s failed with error:%s", dataDir, err.Error())
		return err
	}
	if isExist {
		return nil
	}
	// Create a directory
	err = os.MkdirAll(dataDir, 0755) //nolint:gosec
	if err != nil {
		err = fmt.Errorf("os.MkdirAll %s failed with error:%s", dataDir, err.Error())
		return err
	}
	return nil
}

func getContentMd5(content string) string {
	contentMd5 := md5.New() //nolint:gosec
	contentMd5.Write([]byte(content))
	return hex.EncodeToString(contentMd5.Sum(nil))
}

// SaveContent save the script to the machine
func saveContent(dataDir string, content string, configName, scriptType string) (string, error) {
	// Use the base64-encoded full name of the configuration as the filename to ensure that each configuration will only have one script.
	fileName := base64.StdEncoding.EncodeToString([]byte(configName))

	suffix := ScriptTypeToSuffix[scriptType].scriptSuffix

	filePath := path.Join(dataDir, fmt.Sprintf("%s.%s", fileName, suffix))

	if err := os.WriteFile(filePath, []byte(content), 0755); err != nil { //nolint:gosec
		return "", err
	}
	return filePath, nil
}

func RunCommandWithTimeOut(timeout int, user *user.User, command string, environments []string, args ...string) (stdout, stderr string, isKilled bool, err error) {
	cmd := exec.Command(command, args...)

	// set Env
	if len(environments) > 0 {
		cmd.Env = append(os.Environ(), environments...)
	} else {
		cmd.Env = os.Environ()
	}

	var stdoutBuf bytes.Buffer
	// stderrBuf is used to store the standard error output generated during command execution.
	var stderrBuf bytes.Buffer

	cmd.Stdout = &stdoutBuf
	cmd.Stderr = &stderrBuf

	// set uid and gid
	uid, err := strconv.ParseUint(user.Uid, 10, 32)
	if err != nil {
		return
	}
	gid, err := strconv.ParseUint(user.Gid, 10, 32)
	if err != nil {
		return
	}
	cmd.SysProcAttr = &syscall.SysProcAttr{}
	cmd.SysProcAttr.Credential = &syscall.Credential{
		Uid: uint32(uid),
		Gid: uint32(gid),
	}
	defer func() {
		stdout = strings.TrimSpace(stdoutBuf.String())
		stderr = strings.TrimSpace(stderrBuf.String())
	}()

	// start
	if err = cmd.Start(); err != nil {
		return
	}

	isKilled, err = WaitTimeout(cmd, time.Millisecond*time.Duration(timeout))
	if err != nil {
		return
	}

	return
}

func WaitTimeout(cmd *exec.Cmd, timeout time.Duration) (isKilled bool, err error) {
	var kill *time.Timer
	term := time.AfterFunc(timeout, func() {
		err = cmd.Process.Signal(syscall.SIGTERM)
		if err != nil {
			err = fmt.Errorf("terminating process err: %v", err)
			return
		}

		kill = time.AfterFunc(10*time.Microsecond, func() {
			err = cmd.Process.Kill()
			if err != nil {
				err = fmt.Errorf("killing process err: %v", err)
				return
			}
		})
	})

	err = cmd.Wait()

	if err != nil && (err.Error() == errWaitNoChild || err.Error() == errWaitIDNoChild) {
		err = nil
	}

	if kill != nil {
		kill.Stop()
	}
	isKilled = !term.Stop()

	if err == nil {
		return
	}

	if isKilled {
		err = errors.New("exec command timed out")
		return
	}

	return
}
