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
	"encoding/base64"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path"
	"strconv"
	"sync"
	"syscall"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ScriptStorage struct {
	StorageDir string
	Err        error
}

var storageOnce sync.Once
var storageInstance *ScriptStorage

func GetStorage() *ScriptStorage {
	dataDir := path.Join(config.LogtailGlobalConfig.LogtailSysConfDir, "/scripts")
	storageOnce.Do(func() {
		storageInstance = &ScriptStorage{
			StorageDir: dataDir,
		}
		storageInstance.Init()
	})
	return storageInstance
}

func (storage *ScriptStorage) Init() {
	// if the dir exists
	isExist, err := util.PathExists(storage.StorageDir)
	if err != nil {
		storage.Err = fmt.Errorf("PathExists %s failed with error:%s", storage.StorageDir, err.Error())
		return
	}
	if isExist {
		return
	}
	// Create a directory
	err = os.MkdirAll(storage.StorageDir, 0755) //nolint:gosec
	if err != nil {
		storage.Err = fmt.Errorf("os.MkdirAll %s failed with error:%s", storage.StorageDir, err.Error())
		return
	}
}

// SaveContent save the script to the machine
func (storage *ScriptStorage) SaveContent(content string, configName, scriptType string) (string, error) {
	fileName := base64.StdEncoding.EncodeToString([]byte(configName))

	suffix := ScriptTypeToSuffix[scriptType].scriptSuffix

	filePath := path.Join(storage.StorageDir, fmt.Sprintf("%s.%s", fileName, suffix))

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

	// set std
	var (
		stdoutBuf bytes.Buffer
		stderrBuf bytes.Buffer
	)
	cmd.Stdout = &stdoutBuf
	cmd.Stderr = &stderrBuf

	// set uid and gid
	uid, _ := strconv.Atoi(user.Uid)
	gid, _ := strconv.Atoi(user.Gid)
	cmd.SysProcAttr = &syscall.SysProcAttr{}
	cmd.SysProcAttr.Credential = &syscall.Credential{
		Uid: uint32(uid),
		Gid: uint32(gid),
	}

	// start
	if err = cmd.Start(); err != nil {
		return
	}

	isKilled, err = WaitTimeout(cmd, time.Millisecond*time.Duration(timeout))
	if err != nil {
		return
	}

	stdout = string(bytes.TrimSpace(stdoutBuf.Bytes()))
	stderr = string(bytes.TrimSpace(stderrBuf.Bytes()))

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
