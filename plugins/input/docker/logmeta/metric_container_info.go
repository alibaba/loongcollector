//go:build linux || windows
// +build linux windows

// Copyright 2021 iLogtail Authors
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
package logmeta

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type InputDockerFile struct {
	FlushIntervalMs int
	context         pipeline.Context
}

func (i *InputDockerFile) Description() string {
	return "metric_container_info"
}
func (i *InputDockerFile) Collect(collector pipeline.Collector) error {
	return nil
}
func (i *InputDockerFile) Init(context pipeline.Context) (int, error) {
	i.context = context
	logger.Debugf(i.context.GetRuntimeContext(), "InputDockerFile inited successfully")
	return i.FlushIntervalMs, nil
}
func init() {
	pipeline.MetricInputs["metric_container_info"] = func() pipeline.MetricInput {
		return &InputDockerFile{
			FlushIntervalMs: 3000,
		}
	}
}
