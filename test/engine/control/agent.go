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
package control

import (
	"context"
	"fmt"
	"strings"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

func RestartAgent(ctx context.Context) (context.Context, error) {
	if _, err := setup.Env.ExecOnLogtail("/etc/init.d/loongcollectord restart"); err != nil {
		return ctx, err
	}
	return setup.SetAgentPID(ctx)
}

func ForceRestartAgent(ctx context.Context) (context.Context, error) {
	currentPID := ctx.Value(config.AgentPIDKey)
	if currentPID != nil {
		currentPIDs := strings.Split(strings.TrimSpace(currentPID.(string)), "\n")
		for _, pid := range currentPIDs {
			if _, err := setup.Env.ExecOnLogtail("kill -9 " + pid); err != nil {
				fmt.Println("Force kill agent pid failed: ", err)
			}
		}
	} else {
		fmt.Println("No agent pid found, skip force restart")
	}
	if _, err := setup.Env.ExecOnLogtail("/etc/init.d/loongcollectord restart"); err != nil {
		return ctx, err
	}
	return setup.SetAgentPID(ctx)
}
