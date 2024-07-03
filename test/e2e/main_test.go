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
package e2e

import (
	"context"
	"os"
	"strconv"
	"testing"
	"time"

	"github.com/cucumber/godog"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/cleanup"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
	"github.com/alibaba/ilogtail/test/engine/trigger"
	"github.com/alibaba/ilogtail/test/engine/verify"
)

func TestMain(m *testing.M) {
	loggerOptions := []logger.ConfigOption{
		logger.OptionAsyncLogger,
	}
	loggerOptions = append(loggerOptions, logger.OptionInfoLevel)
	logger.InitTestLogger(loggerOptions...)

	config.TestConfig = config.Config{}
	// Log
	config.TestConfig.GeneratedLogDir = os.Getenv("GENERATED_LOG_DIR")
	if len(config.TestConfig.GeneratedLogDir) == 0 {
		config.TestConfig.GeneratedLogDir = "/tmp/ilogtail"
	}
	config.TestConfig.WorkDir = os.Getenv("WORK_DIR")

	// SSH
	config.TestConfig.SSHUsername = os.Getenv("SSH_USERNAME")
	config.TestConfig.SSHIP = os.Getenv("SSH_IP")
	config.TestConfig.SSHPassword = os.Getenv("SSH_PASSWORD")

	// K8s
	config.TestConfig.KubeConfigPath = os.Getenv("KUBE_CONFIG_PATH")

	// SLS
	config.TestConfig.Project = os.Getenv("PROJECT")
	config.TestConfig.Logstore = os.Getenv("LOGSTORE")
	config.TestConfig.AccessKeyID = os.Getenv("ACCESS_KEY_ID")
	config.TestConfig.AccessKeySecret = os.Getenv("ACCESS_KEY_SECRET")
	config.TestConfig.Endpoint = os.Getenv("ENDPOINT")
	config.TestConfig.Aliuid = os.Getenv("ALIUID")
	config.TestConfig.QueryEndpoint = os.Getenv("QUERY_ENDPOINT")
	config.TestConfig.Region = os.Getenv("REGION")
	timeout, err := strconv.ParseInt(os.Getenv("RETRY_TIMEOUT"), 10, 64)
	if err != nil {
		timeout = 60
	}
	config.TestConfig.RetryTimeout = time.Duration(timeout) * time.Second
	code := m.Run()
	logger.Flush()
	os.Exit(code)
}

func scenarioInitializer(ctx *godog.ScenarioContext) {
	// Given
	ctx.Given(`^\{(\S+)\} environment$`, setup.InitEnv)
	ctx.Given(`^iLogtail depends on containers \{(.*)\}`, setup.SetDockerComposeDependOn)
	ctx.Given(`^iLogtail container mount \{(.*)\} to \{(.*)\}`, setup.MountVolume)
	ctx.Given(`^iLogtail expose port \{(.*)\} to \{(.*)\}`, setup.ExposePort)
	ctx.Given(`^\{(.*)\} local config as below`, control.AddLocalConfig)
	ctx.Given(`^\{(.*)\} http config as below`, control.AddHTTPConfig)
	ctx.Given(`^remove http config \{(.*)\}`, control.RemoveHTTPConfig)
	ctx.Given(`^subcribe data from \{(\S+)\} with config`, subscriber.InitSubscriber)

	// When
	ctx.When(`^generate \{(\d+)\} regex logs, with interval \{(\d+)\}ms$`, trigger.RegexSingle)
	ctx.When(`^generate \{(\d+)\} http logs, with interval \{(\d+)\}ms, url: \{(.*)\}, method: \{(.*)\}, body:`, trigger.HTTP)
	ctx.When(`^add k8s label \{(.*)\}`, control.AddLabel)
	ctx.When(`^remove k8s label \{(.*)\}`, control.RemoveLabel)
	ctx.When(`^start docker-compose \{(\S+)\}`, setup.StartDockerComposeEnv)

	// Then
	ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
	ctx.Then(`^there is at least \{(\d+)\} logs$`, verify.LogCountAtLeast)
	ctx.Then(`^there is at least \{(\d+)\} logs with filter key \{(.*)\} value \{(.*)\}$`, verify.LogCountAtLeastWithFilter)
	ctx.Then(`^the log fields match regex single`, verify.RegexSingle)
	ctx.Then(`^the log fields match kv`, verify.LogFieldKV)
	ctx.Then(`^the log tags match kv`, verify.TagKV)
	ctx.Then(`^the context of log is valid$`, verify.LogContext)
	ctx.Then(`^the log fields match`, verify.LogField)
	ctx.Then(`^the log labels match`, verify.LogLabel)
	ctx.Then(`^the logtail log contains \{(\d+)\} times of \{(.*)\}$`, verify.LogtailPluginLog)
	ctx.Then(`wait \{(\d+)\} seconds`, func(ctx context.Context, t int) context.Context {
		time.Sleep(time.Duration(t) * time.Second)
		return ctx
	})

	// Cleanup
	ctx.Before(func(ctx context.Context, sc *godog.Scenario) (context.Context, error) {
		cleanup.HandleSignal()
		return ctx, nil
	})
	ctx.After(func(ctx context.Context, sc *godog.Scenario, err error) (context.Context, error) {
		cleanup.All()
		return ctx, nil
	})
}
