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
	"strings"
	"text/template"
	"time"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

const triggerTemplate = "cd {{.WorkDir}} && TOTAL_LOG={{.TotalLog}} INTERVAL={{.Interval}} FILENAME={{.Filename}} GENERATED_LOG_DIR={{.GeneratedLogDir}} {{.Custom}} {{.Command}}"

func RegexSingle(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateRegexLogSingle")
}

func RegexSingleGBK(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateRegexLogSingleGBK")
}

func RegexMultiline(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateRegexLogMultiline")
}

func JSONSingle(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateJSONSingle")
}

func JSONMultiline(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateJSONMultiline")
}

func Apsara(ctx context.Context, totalLog int, path string, interval int) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateApsara")
}

func DelimiterSingle(ctx context.Context, totalLog int, path string, interval int, delimiter, quote string) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateDelimiterSingle", "Delimiter", delimiter, "Quote", quote)
}

func DelimiterMultiline(ctx context.Context, totalLog int, path string, interval int, delimiter, quote string) (context.Context, error) {
	return generate(ctx, totalLog, path, interval, "TestGenerateDelimiterMultiline", "Delimiter", delimiter, "Quote", quote)
}

func generate(ctx context.Context, totalLog int, path string, interval int, commandName string, customKV ...string) (context.Context, error) {
	time.Sleep(3 * time.Second)
	command := getRunTriggerCommand(commandName)
	var triggerCommand strings.Builder
	template := template.Must(template.New("trigger").Parse(triggerTemplate))
	splittedPath := strings.Split(path, "/")
	dir := strings.Join(splittedPath[:len(splittedPath)-1], "/")
	filename := splittedPath[len(splittedPath)-1]
	customString := strings.Builder{}
	for i := 0; i < len(customKV); i++ {
		customString.WriteString(customKV[i])
		customString.WriteString("=")
		customString.WriteString(customKV[i+1])
		customString.WriteString(" ")
		i++
	}
	if err := template.Execute(&triggerCommand, map[string]interface{}{
		"WorkDir":         config.TestConfig.WorkDir,
		"TotalLog":        totalLog,
		"Interval":        interval,
		"GeneratedLogDir": dir,
		"Filename":        filename,
		"Custom":          customString.String(),
		"Command":         command,
	}); err != nil {
		return ctx, err
	}
	if _, err := setup.Env.ExecOnSource(ctx, triggerCommand.String()); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func BeginTrigger(ctx context.Context) (context.Context, error) {
	startTime := time.Now().Unix()
	return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
}
