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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type InputStatistics struct {
	context pipeline.Context
}

func (r *InputStatistics) Init(context pipeline.Context) (int, error) {
	r.context = context
	return 0, nil
}

func (r *InputStatistics) Description() string {
	return "statistics input plugin for logtail"
}

func (r *InputStatistics) Collect(collector pipeline.Collector) error {
	for _, config := range LogtailConfig {
		log := &protocol.Log{}
		metricRecord := config.Context.GetLogstoreConfigMetricRecord()

		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: config.Context.GetProject()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "config_name", Value: config.Context.GetConfigName()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "category", Value: config.Context.GetLogstore()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "source_ip", Value: util.GetIPAddress()})

		for _, metricCollector := range metricRecord.MetricCollectors {
			metrics := metricCollector.Collect()
			for _, metric := range metrics {
				record := metric.Export()
				if len(record) == 0 {
					continue
				}
				for k, v := range record {
					log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: v})
				}
			}
		}

		if len(log.Contents) > 0 && StatisticsConfig != nil {
			StatisticsConfig.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log})
			logger.Debug(r.context.GetRuntimeContext(), "statistics", *log)
		}
	}
	return nil
}

func init() {
	pipeline.MetricInputs["metric_statistics"] = func() pipeline.MetricInput {
		return &InputStatistics{}
	}
}
