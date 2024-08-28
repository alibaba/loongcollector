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
	"path"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/alibaba/ilogtail/pkg/config"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type InputCommand struct {
	ScriptType          string   `comment:"Type of script: bash, shell, python2, python3"`
	User                string   `comment:"User executing the script"`
	ScriptContent       string   `comment:"Content of the script"`
	ContentEncoding     string   `comment:"Encoding format of the script: PlainText|Base64"`
	LineSplitSep        string   `comment:"Separator used for splitting lines"`
	TimeoutMilliSeconds int      `comment:"Timeout period for script execution in milliseconds. If the execution exceeds this timeout, the context.Alarm will be triggered"`
	CmdPath             string   `comment:"Path where the executable command needs to be executed"`
	IntervalMs          int      `comment:"Frequency at which the collection is triggered in milliseconds"`
	Environments        []string `comment:"Environment variables"`
	IgnoreError         bool     `comment:"Environment variables"`

	context          pipeline.Context
	storageDir       string
	scriptPath       string
	cmdUser          *user.User
	scriptContentMd5 string
}

func (in *InputCommand) Init(context pipeline.Context) (int, error) {
	in.context = context

	if valid, err := in.Validate(); err != nil || !valid {
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return 0, err
	}

	// Parse Base64 content
	if in.ContentEncoding == ContentTypeBase64 {
		decodeContent, err := base64.StdEncoding.DecodeString(in.ScriptContent)
		if err != nil {
			return 0, fmt.Errorf("base64.StdEncoding error:%s", err)
		}
		in.ScriptContent = string(decodeContent)
	}

	// mkdir
	in.storageDir = path.Join(config.LogtailGlobalConfig.LogtailSysConfDir, "/scripts")
	err := mkdir(in.storageDir)
	if err != nil {
		err = fmt.Errorf("init storageInstance error : %s", err)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return 0, err
	}

	// save ScriptContent
	scriptPath, err := saveContent(in.storageDir, in.ScriptContent, in.context.GetConfigName(), in.ScriptType)
	if err != nil {
		err = fmt.Errorf("SaveContent error: %s", err)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return 0, err
	}
	in.scriptPath = scriptPath

	in.scriptContentMd5 = getContentMd5(in.ScriptContent)

	// Lookup looks up the user
	cmdUser, err := user.Lookup(in.User)
	if err != nil {
		err = fmt.Errorf("cannot find User %s, error: %s", in.User, err)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return 0, err
	}
	in.cmdUser = cmdUser

	return 0, nil
}

func (in *InputCommand) Validate() (bool, error) {
	if _, ok := ScriptTypeToSuffix[in.ScriptType]; !ok {
		err := fmt.Errorf("not support ScriptType %s", in.ScriptType)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if in.User == UserRoot {
		err := fmt.Errorf("exec command not support user root")
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if in.User == "" {
		err := fmt.Errorf("params User can not be empty")
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if _, ok := SupportContentType[in.ContentEncoding]; !ok {
		err := fmt.Errorf("not support ContentType %s", in.ContentEncoding)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if in.ScriptContent == "" {
		err := fmt.Errorf("params ScriptContent can not empty")
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if len(in.ScriptContent) > 512*1024 {
		err := fmt.Errorf("ScriptContent size is greater than 512K")
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}

	if in.TimeoutMilliSeconds > in.IntervalMs {
		in.TimeoutMilliSeconds = in.IntervalMs
		logger.Warning(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command warning", "TimeoutMilliSeconds > IntervalMs", "set TimeoutMilliSeconds = IntervalMs")
	}

	if in.CmdPath == "" {
		in.CmdPath = ScriptTypeToSuffix[in.ScriptType].defaultCmdPath
	}

	if err := isValidBinPath(in.CmdPath); err != nil {
		err = fmt.Errorf("CmdPath %s does not exist, err:%v", in.CmdPath, err)
		logger.Error(in.context.GetRuntimeContext(), util.CategoryConfigAlarm, "init input_command error", err)
		return false, err
	}
	return true, nil
}
func isValidBinPath(cmdPath string) error {
	_, err := os.Stat(cmdPath)
	return err
}
func (in *InputCommand) Collect(collector pipeline.Collector) error {
	// stderrStr is used to store the standard error output generated during command execution.
	// It captures any error messages or diagnostic information produced by the command.
	stdoutStr, stderrStr, isKilled, err := RunCommandWithTimeOut(in.TimeoutMilliSeconds, in.cmdUser, in.CmdPath, in.Environments, in.scriptPath)

	if err != nil {
		if in.IgnoreError {
			err = nil
		} else {
			err = fmt.Errorf("exec cmd error errInfo:%s, stderr:%s, stdout:%s", err, stderrStr, stdoutStr)
			logger.Error(in.context.GetRuntimeContext(), util.InputCollectAlarm, "input_command Collect error", err)
		}
		return err
	}

	if isKilled {
		if in.IgnoreError {
			err = nil
		} else {
			err = fmt.Errorf("timeout run exec script file filepath %s", in.scriptPath)
			logger.Error(in.context.GetRuntimeContext(), util.InputCollectAlarm, "input_command Collect error", err)
		}
		return err
	}

	if stderrStr != "" {
		if in.IgnoreError {
			err = nil
		} else {
			err = fmt.Errorf("exec cmd error, stderr:%s, stdout:%s", stderrStr, stdoutStr)
			logger.Error(in.context.GetRuntimeContext(), util.InputCollectAlarm, "input_command Collect error", err)
		}
		return err
	}

	var outSplitArr []string
	if in.LineSplitSep != "" {
		outSplitArr = strings.Split(stdoutStr, in.LineSplitSep)
	} else {
		outSplitArr = []string{stdoutStr}
	}

	for _, splitStr := range outSplitArr {
		log := &protocol.Log{
			Time: uint32(time.Now().Unix()),
			Contents: []*protocol.Log_Content{
				{
					Key:   models.ContentKey,
					Value: splitStr,
				},
				{
					Key:   ScriptMd5,
					Value: in.scriptContentMd5,
				},
			},
		}
		collector.AddRawLog(log)
	}
	return nil
}

func (in *InputCommand) Description() string {
	return "Collect logs through exec command "
}

func init() {
	pipeline.MetricInputs[pluginType] = func() pipeline.MetricInput {
		return &InputCommand{
			ContentEncoding:     defaultContentType,
			IntervalMs:          defaultIntervalMs,
			TimeoutMilliSeconds: defaltExecScriptTimeOut,
			IgnoreError:         false,
		}
	}
}
