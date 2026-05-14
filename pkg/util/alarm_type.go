// Copyright 2026 iLogtail Authors
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

package util

type AlarmType int

const (
	EnvFlagAlarm AlarmType = iota
	DefaultFlusherAlarm
	PluginRuntimeAlarm
	PluginAlarm
	LoadPluginAlarm
	ConfigLoadAlarm
	ConfigStopTimeoutAlarm
	CheckpointAlarm
	CheckpointSaveAlarm
	CheckpointGetAlarm
	CheckpointInvalidAlarm
	CheckpointInitAlarm
	LoadConfigAlarm
	InitHTTPServerAlarm
	PluginUnmarshalAlarm
	StartPluginAlarm
	TaskExecuteSlowAlarm
	PluginRunAlarm
	DropDataAlarm
	FlushDataAlarm
	AggregatorInitErrorAlarm
	AggregatorAddAlarm
	StopFlusherAlarm
	StopExtensionAlarm
	MetricInputV2StartFailureAlarm
	ReceiveLogGroupAlarm
	ReceiveRawLogAlarm
	WrongProtobufAlarm
	InvalidProcessorTypeAlarm
	InvalidAggregatorTypeAlarm
)

var AlarmTypeName = map[AlarmType]string{
	EnvFlagAlarm:                   "ENV_FLAG_ALARM",
	DefaultFlusherAlarm:            "DEFAULT_FLUSHER_ALARM",
	PluginRuntimeAlarm:             "PLUGIN_RUNTIME_ALARM",
	PluginAlarm:                    "PLUGIN_ALARM",
	LoadPluginAlarm:                "LOAD_PLUGIN_ALARM",
	ConfigLoadAlarm:                "CONFIG_LOAD_ALARM",
	ConfigStopTimeoutAlarm:         "CONFIG_STOP_TIMEOUT_ALARM",
	CheckpointAlarm:                "CHECKPOINT_ALARM",
	CheckpointSaveAlarm:            "CHECKPOINT_SAVE_ALARM",
	CheckpointGetAlarm:             "CHECKPOINT_GET_ALARM",
	CheckpointInvalidAlarm:         "CHECKPOINT_INVALID_ALARM",
	CheckpointInitAlarm:            "CHECKPOINT_INIT_ALARM",
	LoadConfigAlarm:                "LOAD_CONFIG_ALARM",
	InitHTTPServerAlarm:            "INIT_HTTP_SERVER_ALARM",
	PluginUnmarshalAlarm:           "PLUGIN_UNMARSHAL_ALARM",
	StartPluginAlarm:               "START_PLUGIN_ALARM",
	TaskExecuteSlowAlarm:           "TASK_EXECUTE_SLOW",
	PluginRunAlarm:                 "PLUGIN_RUN_ALARM",
	DropDataAlarm:                  "DROP_DATA_ALARM",
	FlushDataAlarm:                 "FLUSH_DATA_ALARM",
	AggregatorInitErrorAlarm:       "AGGREGATOR_INIT_ERROR",
	AggregatorAddAlarm:             "AGGREGATOR_ADD_ALARM",
	StopFlusherAlarm:               "STOP_FLUSHER_ALARM",
	StopExtensionAlarm:             "STOP_EXTENSION_ALARM",
	MetricInputV2StartFailureAlarm: "METRIC_INPUT_V2_START_FAILURE",
	ReceiveLogGroupAlarm:           "RECEIVE_LOG_GROUP_ALARM",
	ReceiveRawLogAlarm:             "RECEIVE_RAW_LOG_ALARM",
	WrongProtobufAlarm:             "WRONG_PROTOBUF_ALARM",
	InvalidProcessorTypeAlarm:      "INVALID_PROCESSOR_TYPE",
	InvalidAggregatorTypeAlarm:     "INVALID_AGGREGATOR_TYPE",
}

func (t AlarmType) String() string {
	if name, ok := AlarmTypeName[t]; ok {
		return name
	}
	return ""
}
