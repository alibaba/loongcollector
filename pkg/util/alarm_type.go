// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package util

type AlarmType int

const (
	AggregatorAddAlarm AlarmType = iota
	AggregatorInitAlarm
	AnchorFindAlarm
	AnchorJsonAlarm // nolint:revive
	CannalInvalidAlarm
	CanalRuntimeAlarm
	CategoryConfigAlarm
	CheckpointAlarm
	CheckpointInvalidAlarm
	ConfigUpdateAlarm
	DockerCenterAlarm
	DockerFileMappingAlarm
	DockerRegexCompileAlarm
	DockerStdoutStartAlarm
	DockerStdoutStatAlarm
	DockerStdoutStopAlarm
	DropDataAlarm
	DumpDataAlarm
	EcsAlarm
	EntityPipelineRegisterAlarm
	EnvFlagAlarm
	EventRecorderAlarm
	ExtensionAlarm
	FilterInitAlarm
	FlusherFlushAlarm
	FlusherInitAlarm
	FlusherReadyAlarm
	FlusherStopAlarm
	GeoipAlarm
	HttpCollectAlarm     // nolint:revive
	HttpInitAlarm        // nolint:revive
	HttpLoadAddressAlarm // nolint:revive
	HttpParseAlarm       // nolint:revive
	InitCheckpointAlarm
	InitHttpServerAlarm // nolint:revive
	InputCanalAlarm
	InputCollectAlarm
	InputInitAlarm
	InputStartAlarm
	InternalMetricAlarm
	JmxfetchAlarm
	K8sMetaAlarm
	LogRegexFindAlarm
	LumberConnectionAlarm
	LumberListenAlarm
	MysqlCheckpoingAlarm
	MysqlCheckpointAlarm
	MysqlInitAlarm
	MysqlParseAlarm
	MusqlQueryAlarm
	MysqlTimeoutAlarm
	NginxStatusCollectAlarm
	NginxStatusInitAlarm
	OpenLogFileFailAlarm
	ParseDockerLineAlarm
	ParseLogFailAlarm
	ParseTimeFailAlarm
	PluginAlarm
	PluginRuntimeAlarm
	PortManagerAlarm
	PprofProfileAlarm
	ProcessorInitAlarm
	ProcessorProcessAlarm
	ProtocolAlarm
	ReceiveDataAlarm
	RedisParseAddressAlarm
	RegexFindAlarm
	RegexUnmatchedAlarm
	SendDataFailAlarm
	ServiceSyslogCloseAlarm
	ServiceSyslogInitAlarm
	ServiceSyslogPacketAlarm
	ServiceSyslogParseAlarm
	ServiceSyslogStreamAlarm
	SplitFindAlarm
	SplitLogAlarm
	StandardOutputAlarm
	StatFileAlarm
	TelegrafAlarm
	WrongProtobufAlarm
	AllLoongCollectorAlarmNum
	// for test
	BootStopAlarm
	CadvisorComposeAlarm
	ClickhouseSubscriberAlarm
	CopyLogAlarm
	DockerExecAlarm
	DownDockerComposeAlarm
	ElasticsearchSubscriberAlarm
	FetchCoverageAlarm
	GrpcServerAlarm
	HoldonLogtailPluginAlarm
	InfluxdbSubscriberAlarm
	LogtailComposeAlarm
	SshExecAlarm // nolint:revive
	StartDockerComposeAlarm
	StopDockerComposeAlarm
)

var AlarmTypeName = map[AlarmType]string{
	AggregatorAddAlarm:          "AGGREGATOR_ADD_ALARM",
	AggregatorInitAlarm:         "AGGREGATOR_INIT_ALARM",
	AnchorFindAlarm:             "ANCHOR_FIND_ALARM",
	AnchorJsonAlarm:             "ANCHOR_JSON_ALARM",
	CannalInvalidAlarm:          "CANAL_INVALID_ALARM",
	CanalRuntimeAlarm:           "CANAL_RUNTIME_ALARM",
	CategoryConfigAlarm:         "CATEGORY_CONFIG_ALARM",
	CheckpointAlarm:             "CHECKPOINT_ALARM",
	CheckpointInvalidAlarm:      "CHECKPOINT_INVALID_ALARM",
	ConfigUpdateAlarm:           "CONFIG_UPDATE_ALARM",
	DockerCenterAlarm:           "DOCKER_CENTER_ALARM",
	DockerFileMappingAlarm:      "DOCKER_FILE_MAPPING_ALARM",
	DockerRegexCompileAlarm:     "DOCKER_REGEX_COMPILE_ALARM",
	DockerStdoutStartAlarm:      "DOCKER_STDOUT_START_ALARM",
	DockerStdoutStatAlarm:       "DOCKER_STDOUT_STAT_ALARM",
	DockerStdoutStopAlarm:       "DOCKER_STDOUT_STOP_ALARM",
	DropDataAlarm:               "DROP_DATA_ALARM",
	DumpDataAlarm:               "DUMP_DATA_ALARM",
	EcsAlarm:                    "ECS_ALARM",
	EntityPipelineRegisterAlarm: "ENTITY_PIPELINE_REGISTER_AlARM",
	EnvFlagAlarm:                "ENV_FLAG_ALARM",
	EventRecorderAlarm:          "EVENT_RECORDER_ALARM",
	ExtensionAlarm:              "EXTENSION_ALARM",
	FilterInitAlarm:             "FILTER_INIT_ALARM",
	FlusherFlushAlarm:           "FLUSHER_FLUSH_ALARM",
	FlusherInitAlarm:            "FLUSHER_INIT_ALARM",
	FlusherReadyAlarm:           "FLUSHER_READY_ALARM",
	FlusherStopAlarm:            "FLUSHER_STOP_ALARM",
	GeoipAlarm:                  "GEOIP_ALARM",
	HttpCollectAlarm:            "HTTP_COLLECT_ALARM",
	HttpInitAlarm:               "HTTP_INIT_ALARM",
	HttpLoadAddressAlarm:        "HTTP_LOAD_ADDRESS_ALARM",
	HttpParseAlarm:              "HTTP_PARSE_ALARM",
	InitCheckpointAlarm:         "INIT_CHECKPOINT_ALARM",
	InitHttpServerAlarm:         "INIT_HTTP_SERVER_ALARM",
	InputCanalAlarm:             "INPUT_CANAL_ALARM",
	InputCollectAlarm:           "INPUT_COLLECT_ALARM",
	InputInitAlarm:              "INPUT_INIT_ALARM",
	InputStartAlarm:             "INPUT_START_ALARM",
	InternalMetricAlarm:         "INTERNAL_METRIC_ALARM",
	JmxfetchAlarm:               "JMXFETCH_ALARM",
	K8sMetaAlarm:                "K8S_META_ALARM",
	LogRegexFindAlarm:           "LOG_REGEX_FIND_ALARM",
	LumberConnectionAlarm:       "LUMBER_CONNECTION_ALARM",
	LumberListenAlarm:           "LUMBER_LISTEN_ALARM",
	MysqlCheckpoingAlarm:        "MYSQL_CHECKPOING_ALARM",
	MysqlCheckpointAlarm:        "MYSQL_CHECKPOINT_ALARM",
	MysqlInitAlarm:              "MYSQL_INIT_ALARM",
	MysqlParseAlarm:             "MYSQL_PARSE_ALARM",
	MusqlQueryAlarm:             "MYSQL_QUERY_ALARM",
	MysqlTimeoutAlarm:           "MYSQL_TIMEOUT_ALARM",
	NginxStatusCollectAlarm:     "NGINX_STATUS_COLLECT_ALARM",
	NginxStatusInitAlarm:        "NGINX_STATUS_INIT_ALARM",
	OpenLogFileFailAlarm:        "OPEN_LOGFILE_FAIL_ALARM",
	ParseDockerLineAlarm:        "PARSE_DOCKER_LINE_ALARM",
	ParseLogFailAlarm:           "PARSE_LOG_FAIL_ALARM",
	ParseTimeFailAlarm:          "PARSE_TIME_FAIL_ALARM",
	PluginAlarm:                 "PLUGIN_ALARM",
	PluginRuntimeAlarm:          "PLUGIN_RUNTIME_ALARM",
	PortManagerAlarm:            "PORT_MANAGER_ALARM",
	PprofProfileAlarm:           "PPROF_PROFILE_ALARM",
	ProcessorInitAlarm:          "PROCESSOR_INIT_ALARM",
	ProcessorProcessAlarm:       "PROCESSOR_PROCESS_ALARM",
	ProtocolAlarm:               "PROTOCOL_ALARM",
	ReceiveDataAlarm:            "RECEIVE_DATA_ALARM",
	RedisParseAddressAlarm:      "REDIS_PARSE_ADDRESS_ALARM",
	RegexFindAlarm:              "REGEX_FIND_ALARM",
	RegexUnmatchedAlarm:         "REGEX_UNMATCHED_ALARM",
	SendDataFailAlarm:           "SEND_DATA_FAIL_ALARM",
	ServiceSyslogCloseAlarm:     "SERVICE_SYSLOG_CLOSE_ALARM",
	ServiceSyslogInitAlarm:      "SERVICE_SYSLOG_INIT_ALARM",
	ServiceSyslogPacketAlarm:    "SERVICE_SYSLOG_PACKET_ALARM",
	ServiceSyslogParseAlarm:     "SERVICE_SYSLOG_PARSE_ALARM",
	ServiceSyslogStreamAlarm:    "SERVICE_SYSLOG_STREAM_ALARM",
	SplitFindAlarm:              "SPLIT_FIND_ALARM",
	SplitLogAlarm:               "SPLIT_LOG_ALARM",
	StandardOutputAlarm:         "STANDARD_OUTPUT_ALARM",
	StatFileAlarm:               "STAT_FILE_ALARM",
	TelegrafAlarm:               "TELEGRAF_ALARM",
	WrongProtobufAlarm:          "WRONG_PROTOBUF_ALARM",
	AllLoongCollectorAlarmNum:   "ALL_LOONGCOLLECTOR_ALARM_NUM",
	// for test
	BootStopAlarm:                "BOOT_STOP_ALARM",
	CadvisorComposeAlarm:         "CADVISOR_COMPOSE_ALARM",
	ClickhouseSubscriberAlarm:    "CLICKHOUSE_SUBSCRIBER_ALARM",
	CopyLogAlarm:                 "COPY_LOG_ALARM",
	DockerExecAlarm:              "DOCKER_EXEC_ALARM",
	DownDockerComposeAlarm:       "DOWN_DOCKER_COMPOSE_ALARM",
	ElasticsearchSubscriberAlarm: "ELASTICSEARCH_SUBSCRIBER_ALARM",
	FetchCoverageAlarm:           "FETCH_COVERAGE_ALARM",
	GrpcServerAlarm:              "GRPC_SERVER_ALARM",
	HoldonLogtailPluginAlarm:     "HOLDON_LOGTAILPLUGIN_ALARM",
	InfluxdbSubscriberAlarm:      "INFLUXDB_SUBSCRIBER_ALARM",
	LogtailComposeAlarm:          "LOGTAIL_COMPOSE_ALARM",
	SshExecAlarm:                 "SSH_EXEC_ALARM",
	StartDockerComposeAlarm:      "START_DOCKER_COMPOSE_ALARM",
	StopDockerComposeAlarm:       "STOP_DOCKER_COMPOSE_ALARM",
}

func (t AlarmType) String() string {
	if name, ok := AlarmTypeName[t]; ok {
		return name
	}
	return ""
}
