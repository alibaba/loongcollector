// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type InputAlarm struct {
	context pipeline.Context
}

func (r *InputAlarm) Init(context pipeline.Context) (int, error) {
	r.context = context
	return 0, nil
}

func (r *InputAlarm) Description() string {
	return "alarm input plugin for logtail"
}

func (r *InputAlarm) Collect(collector pipeline.Collector) error {
	loggroup := &protocol.LogGroup{}
	LogtailConfigLock.RLock()
	for _, config := range LogtailConfig {
		alarm := config.Context.GetRuntimeContext().Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm()
		if alarm != nil {
			alarm.SerializeToPb(loggroup)
		}
	}
	LogtailConfigLock.RUnlock()
	util.GlobalAlarm.SerializeToPb(loggroup)
	if len(loggroup.Logs) > 0 && AlarmConfig != nil {
		for _, log := range loggroup.Logs {
			AlarmConfig.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log})
		}
	}
	util.RegisterAlarmsSerializeToPb(loggroup)
	logger.Debug(r.context.GetRuntimeContext(), "InputAlarm", *loggroup)
	return nil
}

func init() {
	pipeline.MetricInputs["metric_alarm"] = func() pipeline.MetricInput {
		return &InputAlarm{}
	}
}
