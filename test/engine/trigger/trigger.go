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

const triggerRegexTemplate = "cd {{.WorkDir}} && TOTAL_LOG={{.TotalLog}} INTERVAL={{.Interval}} FILENAME={{.Filename}} GENERATED_LOG_DIR={{.GeneratedLogDir}} {{.Command}}"

func RegexSingle(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateRegexLogSingle")
}

func RegexSingleGBK(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateRegexLogSingleGBK")
}

func Apsara(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateApsara")
}

func DelimiterSingle(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateDelimiterSingle")
}

func generate(ctx context.Context, totalLog int, path string, interval int, commandName string) (context.Context, error) {
	time.Sleep(3 * time.Second)
	command := getRunTriggerCommand(commandName)
	var triggerRegexCommand strings.Builder
	template := template.Must(template.New("trigger").Parse(triggerRegexTemplate))
	splittedPath := strings.Split(path, "/")
	dir := strings.Join(splittedPath[:len(splittedPath)-1], "/")
	filename := splittedPath[len(splittedPath)-1]
	if err := template.Execute(&triggerRegexCommand, map[string]interface{}{
		"WorkDir":         config.TestConfig.WorkDir,
		"TotalLog":        totalLog,
		"Interval":        interval,
		"GeneratedLogDir": dir,
		"Filename":        filename,
		"Command":         command,
	}); err != nil {
		return ctx, err
	}
	startTime := time.Now().Unix()
	if err := setup.Env.ExecOnSource(ctx, triggerRegexCommand.String()); err != nil {
		return ctx, err
	}
	return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
}
