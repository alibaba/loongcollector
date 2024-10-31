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
package verify

import (
	"context"
	"fmt"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

const (
	queryPIDCommand = "ps -e | grep loongcollector | grep -v grep | awk '{print $1}'"
)

func AgentNotCrash(ctx context.Context) (context.Context, error) {
	// verify agent crash
	result, err := setup.Env.ExecOnLogtail(queryPIDCommand)
	if err != nil {
		if err.Error() == "not implemented" {
			return ctx, nil
		}
		return ctx, err
	}
	agentPID := ctx.Value(config.AgentPIDKey)
	if agentPID == nil {
		return ctx, fmt.Errorf("agent PID not found in context")
	}
	if result != agentPID {
		return ctx, fmt.Errorf("agent crash, expect PID: %s, but got: %s", agentPID, result)
	}
	return ctx, nil
}
