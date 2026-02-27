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

package pipeline

import (
	"context"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// Context for plugin
type Context interface {
	InitContext(project, logstore, configName string)

	GetConfigName() string
	GetProject() string
	GetLogstore() string
	GetRuntimeContext() context.Context
	GetPipelineScopeConfig() *config.GlobalConfig
	GetExtension(name string, cfg any) (Extension, error)
	// RegisterCounterMetric(metric CounterMetric)
	// RegisterStringMetric(metric StringMetric)
	// RegisterLatencyMetric(metric LatencyMetric)

	SaveCheckPoint(key string, value []byte) error
	GetCheckPoint(key string) (value []byte, exist bool)
	SaveCheckPointObject(key string, obj interface{}) error
	GetCheckPointObject(key string, obj interface{}) (exist bool)

	// APIs for self monitor
	RegisterMetricRecord(labels []selfmonitor.LabelPair) *selfmonitor.MetricsRecord
	GetMetricRecord() *selfmonitor.MetricsRecord
	ExportMetricRecords() []map[string]string
	RegisterLogstoreConfigMetricRecord(labels []selfmonitor.LabelPair) *selfmonitor.MetricsRecord
	GetLogstoreConfigMetricRecord() *selfmonitor.MetricsRecord
}
