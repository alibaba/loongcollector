package engine

import (
	"context"
	"time"

	"github.com/cucumber/godog"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/cleanup"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/setup/monitor"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
	"github.com/alibaba/ilogtail/test/engine/trigger"
	"github.com/alibaba/ilogtail/test/engine/verify"
)

func ScenarioInitializer(ctx *godog.ScenarioContext) {
	// Given
	ctx.Given(`^\{(\S+)\} environment$`, setup.InitEnv)
	ctx.Given(`^iLogtail depends on containers \{(.*)\}`, setup.SetDockerComposeDependOn)
	ctx.Given(`^iLogtail container mount \{(.*)\} to \{(.*)\}`, setup.MountVolume)
	ctx.Given(`^iLogtail expose port \{(.*)\} to \{(.*)\}`, setup.ExposePort)
	ctx.Given(`^\{(.*)\} local config as below`, control.AddLocalConfig)
	ctx.Given(`^\{(.*)\} http config as below`, control.AddHTTPConfig)
	ctx.Given(`^remove http config \{(.*)\}`, control.RemoveHTTPConfig)
	ctx.Given(`^subcribe data from \{(\S+)\} with config`, subscriber.InitSubscriber)
	ctx.Given(`^mkdir \{(.*)\}`, setup.Mkdir)
	ctx.Given(`^docker-compose boot type \{(\S+)\}$`, setup.SetDockerComposeBootType)

	// When
	ctx.When(`^generate \{(\d+)\} regex logs to file \{(.*)\}, with interval \{(\d+)\}ms$`, trigger.RegexSingle)
	ctx.When(`^generate \{(\d+)\} regex gbk logs to file \{(.*)\}, with interval \{(\d+)\}ms$`, trigger.RegexSingleGBK)
	ctx.When(`^generate \{(\d+)\} http logs, with interval \{(\d+)\}ms, url: \{(.*)\}, method: \{(.*)\}, body:`, trigger.HTTP)
	ctx.When(`^add k8s label \{(.*)\}`, control.AddLabel)
	ctx.When(`^remove k8s label \{(.*)\}`, control.RemoveLabel)
	ctx.When(`^start docker-compose \{(\S+)\}`, setup.StartDockerComposeEnv)
	ctx.When(`^switch working on deployment \{(.*)\}`, setup.SwitchCurrentWorkingDeployment)
	ctx.When(`^generate \{(\d+)\} apsara logs to file \{(.*)\}, with interval \{(\d+)\}ms$`, trigger.Apsara)
	ctx.When(`^generate \{(\d+)\} delimiter logs to file \{(.*)\}, with interval \{(\d+)\}ms$`, trigger.DelimiterSingle)
	ctx.When(`^query through \{(.*)\}`, control.SetQuery)
	ctx.When(`^begin trigger`, trigger.BeginTrigger)
	ctx.When(`^execute \{(\d+)\} commands to generate file security events on files \{(.*)\}$`, trigger.TrigerFileSecurityEvents)
	ctx.When(`^generate random nginx logs to file, speed \{(\d+)\}MB/s, total \{(\d+)\}min, to file \{(.*)\}`, trigger.GenerateRandomNginxLogToFile)
	ctx.When(`^start monitor \{(\S+)\}`, monitor.StartMonitor)
	ctx.When(`^wait monitor until log processing finished$`, monitor.WaitMonitorUntilProcessingFinished)

	// Then
	ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
	ctx.Then(`^there is more than \{(\d+)\} metrics in \{(\d+)\} seconds and the value is greater than \{(\d+)\} and less than \{(\d+)\}$`, verify.MetricCountAndValueCompare)
	ctx.Then(`^there is more than \{(\d+)\} metrics in \{(\d+)\} seconds and the value is \{(\d+)\}$`, verify.MetricCountAndValueEqual)
	ctx.Then(`^there is more than \{(\d+)\} metrics in \{(\d+)\} seconds$`, verify.MetricCount)
	ctx.Then(`^there is at least \{(\d+)\} logs$`, verify.LogCountAtLeast)
	ctx.Then(`^there is at least \{(\d+)\} logs with filter key \{(.*)\} value \{(.*)\}$`, verify.LogCountAtLeastWithFilter)
	ctx.Then(`^the log fields match kv`, verify.LogFieldKV)
	ctx.Then(`^the log tags match kv`, verify.TagKV)
	ctx.Then(`^the context of log is valid$`, verify.LogContext)
	ctx.Then(`^the log fields match as below`, verify.LogField)
	ctx.Then(`^the log labels match as below`, verify.LogLabel)
	ctx.Then(`^the logtail log contains \{(\d+)\} times of \{(.*)\}$`, verify.LogtailPluginLog)
	ctx.Then(`wait \{(\d+)\} seconds`, func(ctx context.Context, t int) context.Context {
		time.Sleep(time.Duration(t) * time.Second)
		return ctx
	})
	// special pattern logs
	ctx.Then(`^the log fields match regex single`, verify.RegexSingle)
	ctx.Then(`^the log fields match apsara`, verify.Apsara)

	ctx.Before(func(ctx context.Context, sc *godog.Scenario) (context.Context, error) {
		config.ParseConfig()
		cleanup.HandleSignal()
		return ctx, nil
	})
	ctx.After(func(ctx context.Context, sc *godog.Scenario, err error) (context.Context, error) {
		cleanup.All()
		return ctx, nil
	})
}
