// Copyright 2024 iLogtail Authors
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

package helper

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

//////////////////////////////////////////////////////////////////////////
// agent
//////////////////////////////////////////////////////////////////////////

// metric keys
const (
	MetricAgentMemoryGo        = "agent_go_memory_used_mb"
	MetricAgentGoRoutinesTotal = "agent_go_routines_total"
)

func GetCommonLabels(context pipeline.Context, pluginMeta *pipeline.PluginMeta) []pipeline.LabelPair {
	labels := make([]pipeline.LabelPair, 0)
	labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyProject, Value: context.GetProject()})
	labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyLogstore, Value: context.GetLogstore()})
	labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyPipelineName, Value: context.GetConfigName()})

	if len(pluginMeta.PluginID) > 0 {
		labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyPluginID, Value: pluginMeta.PluginID})
	}
	if len(pluginMeta.PluginType) > 0 {
		labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyPluginType, Value: pluginMeta.PluginType})
	}
	return labels
}
