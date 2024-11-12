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
	"fmt"
	"path/filepath"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/test/config"
)

const commandTemplate = "python3 %s.py %s"

func BeginTrigger(ctx context.Context) (context.Context, error) {
	startTime := time.Now().Unix()
	return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
}

func GetRunTriggerCommand(scenrio, triggerName string, kvs ...string) string {
	args := make([]string, 0)
	for i := 0; i < len(kvs); i += 2 {
		args = append(args, fmt.Sprintf("--%s", kvs[i]), kvs[i+1])
	}
	return fmt.Sprintf(commandTemplate, filepath.Join(config.TestConfig.WorkDir, "engine", "trigger", scenrio, "remote_"+triggerName), strings.Join(args, " "))
}
