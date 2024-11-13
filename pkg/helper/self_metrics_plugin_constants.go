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

import "github.com/alibaba/ilogtail/pkg/pipeline"

//////////////////////////////////////////////////////////////////////////
// plugin
//////////////////////////////////////////////////////////////////////////

// label keys
const (
	MetricLabelKeyMetricCategory = "metric_category"
	MetricLabelKeyProject        = "project"
	MetricLabelKeyLogstore       = "logstore"
	MetricLabelKeyPipelineName   = "pipeline_name"
	MetricLabelKeyPluginType     = "plugin_type"
	MetricLabelKeyPluginID       = "plugin_id"
)

// label values
const (
	MetricLabelValueMetricCategoryPlugin = "plugin"
)

// metric keys
const (
	MetricPluginInEventsTotal       = "in_events_total"
	MetricPluginInEventGroupsTotal  = "in_event_groups_total"
	MetricPluginInSizeBytes         = "in_size_bytes"
	MetricPluginOutEventsTotal      = "out_events_total"
	MetricPluginOutEventGroupsTotal = "out_event_groups_total"
	MetricPluginOutSizeBytes        = "out_size_bytes"
	MetricPluginTotalDelayMs        = "total_delay_ms"
	MetricPluginTotalProcessTimeMs  = "total_process_time_ms"
)

/**********************************************************
*   input_canal
**********************************************************/
const (
	MetricPluginBinlogRotate     = "binlog_rotate"
	MetricPluginBinlogSync       = "binlog_sync"
	MetricPluginBinlogDdl        = "binlog_ddl"
	MetricPluginBinlogRow        = "binlog_row"
	MetricPluginBinlogXgid       = "binlog_xgid"
	MetricPluginBinlogCheckpoint = "binlog_checkpoint"
	MetricPluginBinlogFilename   = "binlog_filename"
	MetricPluginBinlogGtid       = "binlog_gtid"
)

/**********************************************************
*   metric_container_info
*	service_docker_stdout_v2
**********************************************************/
const (
	MetricPluginContainerTotal       = "container_total"
	MetricPluginAddContainerTotal    = "add_container_total"
	MetricPluginRemoveContainerTotal = "remove_container_total"
	MetricPluginUpdateContainerTotal = "update_container_total"
)

/**********************************************************
*   service_mysql
*   service_rdb
**********************************************************/
const (
	MetricPluginCollectAvgCostTimeMs = "collect_avg_cost_time_ms"
	MetricPluginCollectTotal         = "collect_total"
)

/**********************************************************
*   service_k8s_meta
**********************************************************/
const (
	MetricCollectEntityTotal = "collect_entity_total"
	MetricCollectLinkTotal   = "collect_link_total"
)

/**********************************************************
*   all processor（所有解析类的处理插件通用指标。Todo：目前统计还不全、不准确）
**********************************************************/
const (
	MetricPluginDiscardedEventsTotal      = "discarded_events_total"
	MetricPluginOutFailedEventsTotal      = "out_failed_events_total"
	MetricPluginOutKeyNotFoundEventsTotal = "out_key_not_found_events_total"
	MetricPluginOutSuccessfulEventsTotal  = "out_successful_events_total"
)

/**********************************************************
*   processor_anchor
*   processor_regex
*   processor_string_replace
**********************************************************/
const (
	PluginPairsPerLogTotal = "pairs_per_log_total"
)

func GetPluginCommonLabels(context pipeline.Context, pluginMeta *pipeline.PluginMeta) []pipeline.LabelPair {
	labels := make([]pipeline.LabelPair, 0)
	labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyMetricCategory, Value: MetricLabelValueMetricCategoryPlugin})
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
