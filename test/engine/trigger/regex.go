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

const triggerRegexTemplate = "cd {{.WorkDir}} && TOTAL_LOG={{.TotalLog}} INTERVAL={{.Interval}} FILENAME={{.Filename}} {{.Command}}"

func RegexSingle(ctx context.Context, totalLog, interval int) (context.Context, error) {
	command := getRunTriggerCommand("TestGenerateRegexLogSingle")
	var triggerRegexCommand strings.Builder
	template := template.Must(template.New("triggerRegexSingle").Parse(triggerRegexTemplate))
	if err := template.Execute(&triggerRegexCommand, map[string]interface{}{
		"WorkDir":  config.TestConfig.WorkDir,
		"TotalLog": totalLog,
		"Interval": interval,
		"Filename": "regex_single.log",
		"Command":  command,
	}); err != nil {
		return ctx, err
	}
	startTime := time.Now().Unix()
	if err := setup.Env.ExecOnSource(triggerRegexCommand.String()); err != nil {
		return ctx, err
	}
	return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
}
