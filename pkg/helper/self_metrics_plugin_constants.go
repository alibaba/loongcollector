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
	MetricLabelKeyMetricCategoryPlugin = "plugin"
)

// metric keys
const (
	MetricPluginInEventsTotal       = "plugin_in_events_total"
	MetricPluginInEventGroupsTotal  = "plugin_in_event_groups_total"
	MetricPluginInSizeBytes         = "plugin_in_size_bytes"
	MetricPluginOutEventsTotal      = "plugin_out_events_total"
	MetricPluginOutEventGroupsTotal = "plugin_out_event_groups_total"
	MetricPluginOutSizeBytes        = "plugin_out_size_bytes"
	MetricPluginTotalDelayMs        = "plugin_total_delay_ms"
	MetricPluginTotalProcessTimeMs  = "plugin_total_process_time_ms"
)

/**********************************************************
*   input_canal
**********************************************************/
const (
	MetricPluginBinlogRotate     = "plugin_binlog_rotate"
	MetricPluginBinlogSync       = "plugin_binlog_sync"
	MetricPluginBinlogDdl        = "plugin_binlog_ddl"
	MetricPluginBinlogRow        = "plugin_binlog_row"
	MetricPluginBinlogXgid       = "plugin_binlog_xgid"
	MetricPluginBinlogCheckpoint = "plugin_binlog_checkpoint"
	MetricPluginBinlogFilename   = "plugin_binlog_filename"
	MetricPluginBinlogGtid       = "plugin_binlog_gtid"
)

/**********************************************************
*   metric_container_info
*	service_docker_stdout_v2
**********************************************************/
const (
	MetricPluginContainerTotal       = "plugin_container_total"
	MetricPluginAddContainerTotal    = "plugin_add_container_total"
	MetricPluginRemoveContainerTotal = "plugin_remove_container_total"
	MetricPluginUpdateContainerTotal = "plugin_update_container_total"
)

/**********************************************************
*   service_mysql
*   service_rdb
**********************************************************/
const (
	MetricPluginCollectAvgCostTimeMs = "plugin_collect_avg_cost_time_ms"
	MetricPluginCollectTotal         = "plugin_collect_total"
)

/**********************************************************
*   service_k8s_meta
**********************************************************/
const (
	MetricCollectEntityTotal = "plugin_collect_entity_total"
	MetricCollectLinkTotal   = "plugin_collect_link_total"
)

/**********************************************************
*   all processor（所有解析类的处理插件通用指标。Todo：目前统计还不全、不准确）
**********************************************************/
const (
	MetricPluginDiscardedEventsTotal      = "plugin_discarded_events_total"
	MetricPluginOutFailedEventsTotal      = "plugin_out_failed_events_total"
	MetricPluginOutKeyNotFoundEventsTotal = "plugin_out_key_not_found_events_total"
	MetricPluginOutSuccessfulEventsTotal  = "plugin_out_successful_events_total"
)

/**********************************************************
*   processor_anchor
*   processor_regex
*   processor_string_replace
**********************************************************/
const (
	PluginPairsPerLogTotal = "plugin_pairs_per_log_total"
)

func GetPluginCommonLabels(context pipeline.Context, pluginMeta *pipeline.PluginMeta) []pipeline.LabelPair {
	labels := make([]pipeline.LabelPair, 0)
	labels = append(labels, pipeline.LabelPair{Key: MetricLabelKeyMetricCategory, Value: MetricLabelKeyMetricCategoryPlugin})
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
