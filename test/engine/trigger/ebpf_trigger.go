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
	"context"
	"html/template"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

const triggerFileSecurityTemplate = "cd {{.WorkDir}} && COMMAND_CNT={{.CommandCnt}} FILE_NAME={{.FileName}} {{.Command}}"

func TrigerFileSecurityEvents(ctx context.Context, commandCnt int, filenames string) (context.Context, error) {
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
		if err := setup.Env.ExecOnSource(ctx, touchFileCommand); err != nil {
			return err
		}
		catFileCommand := "echo 'Hello, World!' >> " + file + ";"
		for i := 0; i < commandCnt; i++ {
			if err := setup.Env.ExecOnSource(ctx, catFileCommand); err != nil {
				return err
			}
		}
		removeCommand := "rm " + file + ";"
		if err := setup.Env.ExecOnSource(ctx, removeCommand); err != nil {
			return err
		}
	}
	return nil
}

func mmapFile(ctx context.Context, commandCnt int, filenames string) error {
	mmapFileCommand := getRunTriggerCommand("TestGenerateMmapCommand")
	files := strings.Split(filenames, ",")
	for _, file := range files {
		var triggerEBPFCommand strings.Builder
		template := template.Must(template.New("trigger").Parse(triggerFileSecurityTemplate))
		if err := template.Execute(&triggerEBPFCommand, map[string]interface{}{
			"WorkDir":    config.TestConfig.WorkDir,
			"CommandCnt": commandCnt,
			"FileName":   file,
			"Command":    mmapFileCommand,
		}); err != nil {
			return err
		}
		if err := setup.Env.ExecOnSource(ctx, triggerEBPFCommand.String()); err != nil {
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
			if err := setup.Env.ExecOnSource(ctx, truncateFileCommand1); err != nil {
				return err
			}
			if err := setup.Env.ExecOnSource(ctx, truncateFileCommand2); err != nil {
				return err
			}
		}
		if commandCnt%2 != 0 {
			if err := setup.Env.ExecOnSource(ctx, truncateFileCommand1); err != nil {
				return err
			}
		}
	}
	return nil
}
