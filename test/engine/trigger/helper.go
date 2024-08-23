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
	"fmt"
	"os"
)

const commandTemplate = "/usr/local/go/bin/go test -count=1 -v -run ^%s$ github.com/alibaba/ilogtail/test/engine/trigger"

func getEnvOrDefault(env, fallback string) string {
	if value, ok := os.LookupEnv(env); ok {
		return value
	}
	return fallback
}

func getRunTriggerCommand(triggerName string) string {
	return fmt.Sprintf(commandTemplate, triggerName)
}
