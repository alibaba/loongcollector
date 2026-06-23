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
package setup

import (
	"context"
	"strings"

	"github.com/alibaba/ilogtail/test/config"
)

var Env TestEnv

type TestEnv interface {
	GetType() string
	ExecOnLoongCollector(command string) (string, error)
	ExecOnSource(ctx context.Context, command string) (string, error)
}

func InitEnv(ctx context.Context, envType string) (context.Context, error) {
	switch envType {
	case "host":
		Env = NewHostEnv()
	case "daemonset":
		Env = NewDaemonSetEnv()
	case "docker-compose":
		Env = NewDockerComposeEnv()
	case "deployment":
		Env = NewDeploymentEnv()
	}
	// PID is captured after compose start in StartDockerComposeEnv; avoid host stray processes here.
	if envType == "docker-compose" {
		return ctx, nil
	}
	ctx, err := SetAgentPID(ctx)
	if err != nil {
		return ctx, err
	}
	return EnsureGeneratedLogDir(ctx)
}

// EnsureGeneratedLogDir creates GENERATED_LOG_DIR on the test target before log generation.
func EnsureGeneratedLogDir(ctx context.Context) (context.Context, error) {
	if config.TestConfig.GeneratedLogDir == "" {
		return ctx, nil
	}
	_, err := Env.ExecOnSource(ctx, "mkdir -p "+config.TestConfig.GeneratedLogDir)
	return ctx, err
}

func Mkdir(ctx context.Context, dir string) (context.Context, error) {
	command := "mkdir -p " + dir
	_, err := Env.ExecOnSource(ctx, command)
	return ctx, err
}

func SetAgentPID(ctx context.Context) (context.Context, error) {
	command := "ps -e | grep '[l]oongcollector' | awk '{print $1}' | head -1"
	result, err := Env.ExecOnLoongCollector(command)
	if err != nil {
		if err.Error() == "not implemented" {
			return ctx, nil
		}
		return ctx, err
	}
	return context.WithValue(ctx, config.AgentPIDKey, strings.TrimSpace(result)), nil
}

func RunCommandOnSource(ctx context.Context, command string) (context.Context, error) {
	_, err := Env.ExecOnSource(ctx, command)
	return ctx, err
}

func RunCommandOnLoongCollector(ctx context.Context, command string) (context.Context, error) {
	_, err := Env.ExecOnLoongCollector(command)
	return ctx, err
}
