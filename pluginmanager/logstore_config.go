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
	"bytes"
	"context"
	"crypto/md5" //nolint:gosec
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"sync/atomic"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/input"
)

var maxFlushOutTime = 5

const mixProcessModeFlag = "mix_process_mode"

type mixProcessMode int

const (
	normal mixProcessMode = iota
	file
	observer
)

// checkMixProcessMode
// When inputs plugins not exist, it means this LogConfig is a mixed process mode config.
// And the default mix process mode is the file mode.
func checkMixProcessMode(pluginCfg map[string]interface{}) mixProcessMode {
	config, exists := pluginCfg["inputs"]
	inputs, ok := config.([]interface{})
	if exists && ok && len(inputs) > 0 {
		return normal
	}
	mixModeFlag, mixModeFlagOk := pluginCfg[mixProcessModeFlag]
	if !mixModeFlagOk {
		return file
	}
	s := mixModeFlag.(string)
	switch {
	case strings.EqualFold(s, "observer"):
		return observer
	default:
		return file
	}
}

type LogstoreStatistics struct {
	CollecLatencytMetric pipeline.LatencyMetric
	RawLogMetric         pipeline.CounterMetric
	SplitLogMetric       pipeline.CounterMetric
	FlushLogMetric       pipeline.CounterMetric
	FlushLogGroupMetric  pipeline.CounterMetric
	FlushReadyMetric     pipeline.CounterMetric
	FlushLatencyMetric   pipeline.LatencyMetric
}

type ConfigVersion string

var (
	v1 ConfigVersion = "v1"
	v2 ConfigVersion = "v2"
)

type LogstoreConfig struct {
	// common fields
	ProjectName          string
	LogstoreName         string
	ConfigName           string
	ConfigNameWithSuffix string
	LogstoreKey          int64
	FlushOutFlag         atomic.Bool
	// Each LogstoreConfig can have its independent GlobalConfig if the "global" field
	//   is offered in configuration, see build-in AlarmConfig.
	GlobalConfig *config.GlobalConfig

	Version      ConfigVersion
	Context      pipeline.Context
	Statistics   LogstoreStatistics
	PluginRunner PluginRunner
	// private fields
	configDetailHash string

	K8sLabelSet              map[string]struct{}
	ContainerLabelSet        map[string]struct{}
	EnvSet                   map[string]struct{}
	CollectingContainersMeta bool
	pluginID                 int32
	extensionIndex           int32
}

func (p *LogstoreStatistics) Init(context pipeline.Context) {
	labels := helper.GetPluginCommonLabels(context, &pipeline.PluginMeta{})
	metricsRecord := context.RegisterLogstoreConfigMetricRecord(labels)
	p.CollecLatencytMetric = helper.NewLatencyMetricAndRegister(metricsRecord, "collect_latency")
	p.RawLogMetric = helper.NewCounterMetricAndRegister(metricsRecord, "raw_log")
	p.SplitLogMetric = helper.NewCounterMetricAndRegister(metricsRecord, "processed_log")
	p.FlushLogMetric = helper.NewCounterMetricAndRegister(metricsRecord, "flush_log")
	p.FlushLogGroupMetric = helper.NewCounterMetricAndRegister(metricsRecord, "flush_loggroup")
	p.FlushReadyMetric = helper.NewAverageMetricAndRegister(metricsRecord, "flush_ready")
	p.FlushLatencyMetric = helper.NewLatencyMetricAndRegister(metricsRecord, "flush_latency")
}

// Start initializes plugin instances in config and starts them.
// Procedures:
//  1. Start flusher goroutine and push FlushOutLogGroups inherited from last config
//     instance to LogGroupsChan, so that they can be flushed to flushers.
//  2. Start aggregators, allocate new goroutine for each one.
//  3. Start processor goroutine to process logs from LogsChan.
//  4. Start inputs (including metrics and services), just like aggregator, each input
//     has its own goroutine.
func (lc *LogstoreConfig) Start() {
	lc.FlushOutFlag.Store(false)
	logger.Info(lc.Context.GetRuntimeContext(), "config start", "begin")

	lc.PluginRunner.Run()

	logger.Info(lc.Context.GetRuntimeContext(), "config start", "success")
}

// Stop stops plugin instances and corresponding goroutines of config.
// @removedFlag passed from C++, indicates that if config will be removed after this.
// Procedures:
// 1. SetUrgent to all flushers to indicate them current state.
// 2. Stop all input plugins, stop generating logs.
// 3. Stop processor goroutine, pass all existing logs to aggregator.
// 4. Stop all aggregator plugins, make all logs to LogGroups.
// 5. Set stopping flag, stop flusher goroutine.
// 6. If config will be removed and there are remaining data, try to flush once.
// 7. Stop flusher plugins.
func (lc *LogstoreConfig) Stop(removedFlag bool) error {
	logger.Info(lc.Context.GetRuntimeContext(), "config stop", "begin", "removing", removedFlag)
	if err := lc.PluginRunner.Stop(removedFlag); err != nil {
		return err
	}
	logger.Info(lc.Context.GetRuntimeContext(), "Plugin Runner stop", "done")
	logger.Info(lc.Context.GetRuntimeContext(), "config stop", "success")
	return nil
}

const (
	rawStringKey     = "content"
	defaultTagPrefix = "__tag__:__prefix__"
)

var (
	tagDelimiter = []byte("^^^")
	tagSeparator = []byte("~=~")
)

// extractTags extracts tags from rawTags and append them into log.
// Rule: k1~=~v1^^^k2~=~v2
// rawTags
func extractTags(rawTags []byte, log *protocol.Log) {
	defaultPrefixIndex := 0
	for len(rawTags) != 0 {
		idx := bytes.Index(rawTags, tagDelimiter)
		var part []byte
		if idx < 0 {
			part = rawTags
			rawTags = rawTags[len(rawTags):]
		} else {
			part = rawTags[:idx]
			rawTags = rawTags[idx+len(tagDelimiter):]
		}
		if len(part) > 0 {
			pos := bytes.Index(part, tagSeparator)
			if pos > 0 {
				log.Contents = append(log.Contents, &protocol.Log_Content{
					Key:   string(part[:pos]),
					Value: string(part[pos+len(tagSeparator):]),
				})
			} else {
				log.Contents = append(log.Contents, &protocol.Log_Content{
					Key:   defaultTagPrefix + strconv.Itoa(defaultPrefixIndex),
					Value: string(part),
				})
			}
			defaultPrefixIndex++
		}
	}
}

// extractTagsToLogTags extracts tags from rawTags and append them into []*protocol.LogTag.
// Rule: k1~=~v1^^^k2~=~v2
// rawTags
func extractTagsToLogTags(rawTags []byte) []*protocol.LogTag {
	logTags := []*protocol.LogTag{}
	defaultPrefixIndex := 0
	for len(rawTags) != 0 {
		idx := bytes.Index(rawTags, tagDelimiter)
		var part []byte
		if idx < 0 {
			part = rawTags
			rawTags = rawTags[len(rawTags):]
		} else {
			part = rawTags[:idx]
			rawTags = rawTags[idx+len(tagDelimiter):]
		}
		if len(part) > 0 {
			pos := bytes.Index(part, tagSeparator)
			if pos > 0 {
				logTags = append(logTags, &protocol.LogTag{
					Key:   string(part[:pos]),
					Value: string(part[pos+len(tagSeparator):]),
				})
			} else {
				logTags = append(logTags, &protocol.LogTag{
					Key:   defaultTagPrefix + strconv.Itoa(defaultPrefixIndex),
					Value: string(part),
				})
			}
			defaultPrefixIndex++
		}
	}
	return logTags
}

// ProcessRawLogV2 ...
// V1 -> V2: enable topic field, and use tags field to pass more tags.
// unsafe parameter: rawLog,packID and tags
// safe parameter:  topic
func (lc *LogstoreConfig) ProcessRawLogV2(rawLog []byte, packID string, topic string, tags []byte) int {
	log := &protocol.Log{
		Contents: make([]*protocol.Log_Content, 0, 16),
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: rawStringKey, Value: string(rawLog)})
	if len(topic) > 0 {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "__log_topic__", Value: topic})
	}
	// When UsingOldContentTag is set to false, the tag is now put into the context during cgo.
	if !lc.GlobalConfig.UsingOldContentTag {
		logTags := extractTagsToLogTags(tags)
		lc.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: map[string]interface{}{"source": packID, "topic": topic, "tags": logTags}})
	} else {
		extractTags(tags, log)
		lc.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: map[string]interface{}{"source": packID, "topic": topic}})
	}
	return 0
}

func (lc *LogstoreConfig) ProcessLog(logByte []byte, packID string, topic string, tags []byte) int {
	log := &protocol.Log{}
	err := log.Unmarshal(logByte)
	if err != nil {
		logger.Error(lc.Context.GetRuntimeContext(), "WRONG_PROTOBUF_ALARM",
			"cannot process logs passed by core, err", err)
		return -1
	}
	if len(topic) > 0 {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "__log_topic__", Value: topic})
	}
	// When UsingOldContentTag is set to false, the tag is now put into the context during cgo.
	if !lc.GlobalConfig.UsingOldContentTag {
		logTags := extractTagsToLogTags(tags)
		lc.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: map[string]interface{}{"source": packID, "topic": topic, "tags": logTags}})
	} else {
		extractTags(tags, log)
		lc.PluginRunner.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: map[string]interface{}{"source": packID, "topic": topic}})
	}
	return 0
}

func (lc *LogstoreConfig) ProcessLogGroup(logByte []byte, packID string) int {
	logGroup := &protocol.LogGroup{}
	err := logGroup.Unmarshal(logByte)
	if err != nil {
		logger.Error(lc.Context.GetRuntimeContext(), "WRONG_PROTOBUF_ALARM",
			"cannot process log group passed by core, err", err)
		return -1
	}
	lc.PluginRunner.ReceiveLogGroup(pipeline.LogGroupWithContext{
		LogGroup: logGroup,
		Context:  map[string]interface{}{ctxKeySource: packID}},
	)
	return 0
}

func hasDockerStdoutInput(plugins map[string]interface{}) bool {
	inputs, exists := plugins["inputs"]
	if !exists {
		return false
	}

	inputList, valid := inputs.([]interface{})
	if !valid {
		return false
	}

	for _, detail := range inputList {
		cfg, valid := detail.(map[string]interface{})
		if !valid {
			continue
		}
		pluginTypeWithID, valid := cfg["type"]
		if !valid {
			continue
		}
		if val, valid := pluginTypeWithID.(string); valid {
			pluginType := getPluginType(val)
			if pluginType == input.ServiceDockerStdoutPluginName {
				return true
			}
		}
	}
	return false
}

func createLogstoreConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) (*LogstoreConfig, error) {
	var err error
	contextImp := &ContextImp{}
	contextImp.InitContext(project, logstore, configName)
	logstoreC := &LogstoreConfig{
		ProjectName:          project,
		LogstoreName:         logstore,
		ConfigName:           config.GetRealConfigName(configName),
		ConfigNameWithSuffix: configName,
		LogstoreKey:          logstoreKey,
		Context:              contextImp,
		configDetailHash:     fmt.Sprintf("%x", md5.Sum([]byte(jsonStr))), //nolint:gosec
	}
	contextImp.logstoreC = logstoreC

	var plugins = make(map[string]interface{})
	if err = json.Unmarshal([]byte(jsonStr), &plugins); err != nil {
		return nil, err
	}

	logstoreC.Version = fetchPluginVersion(plugins)
	if logstoreC.PluginRunner, err = initPluginRunner(logstoreC); err != nil {
		return nil, err
	}

	logstoreC.ContainerLabelSet = make(map[string]struct{})
	logstoreC.EnvSet = make(map[string]struct{})
	logstoreC.K8sLabelSet = make(map[string]struct{})
	// add env and label set to logstore config
	inputs, exists := plugins["inputs"]
	if exists {
		inputList, valid := inputs.([]interface{})
		if valid {
			for _, detail := range inputList {
				cfg, valid := detail.(map[string]interface{})
				if !valid {
					continue
				}
				pluginTypeWithID, valid := cfg["type"]
				if !valid {
					continue
				}
				val, valid := pluginTypeWithID.(string)
				if !valid {
					continue
				}
				pluginType := getPluginType(val)
				if pluginType == input.ServiceDockerStdoutPluginName || pluginType == input.MetricDocierFilePluginName {
					configDetail, valid := cfg["detail"]
					if !valid {
						continue
					}
					detailMap, valid := configDetail.(map[string]interface{})
					if !valid {
						continue
					}
					for key, value := range detailMap {
						lowerKey := strings.ToLower(key)
						if strings.Contains(lowerKey, "include") || strings.Contains(lowerKey, "exclude") {
							conditionMap, valid := value.(map[string]interface{})
							if !valid {
								continue
							}
							if strings.Contains(lowerKey, "k8slabel") {
								for key := range conditionMap {
									logstoreC.K8sLabelSet[key] = struct{}{}
								}
							} else if strings.Contains(lowerKey, "label") {
								for key := range conditionMap {
									logstoreC.ContainerLabelSet[key] = struct{}{}
								}
							}
							if strings.Contains(lowerKey, "env") {
								for key := range conditionMap {
									logstoreC.EnvSet[key] = struct{}{}
								}
							}
						}
						if strings.Contains(lowerKey, "collectcontainersflag") {
							collectContainersFlag, valid := value.(bool)
							if !valid {
								continue
							}
							logstoreC.CollectingContainersMeta = collectContainersFlag
						} else if strings.Contains(lowerKey, "collectingcontainersmeta") {
							collectingContainersMeta, valid := value.(bool)
							if !valid {
								continue
							}
							logstoreC.CollectingContainersMeta = collectingContainersMeta
						}
					}
				}
			}
		}
	}

	logstoreC.GlobalConfig = &config.LoongcollectorGlobalConfig
	// If plugins config has "global" field, then override the logstoreC.GlobalConfig
	if pluginConfigInterface, flag := plugins["global"]; flag {
		pluginConfig := &config.GlobalConfig{}
		*pluginConfig = config.LoongcollectorGlobalConfig
		if flag {
			configJSONStr, err := json.Marshal(pluginConfigInterface) //nolint:govet
			if err != nil {
				return nil, err
			}
			err = json.Unmarshal(configJSONStr, &pluginConfig)
			if err != nil {
				return nil, err
			}
		}
		logstoreC.GlobalConfig = pluginConfig
		logger.Debug(contextImp.GetRuntimeContext(), "load plugin config", *logstoreC.GlobalConfig)
	}

	logQueueSize := logstoreC.GlobalConfig.DefaultLogQueueSize
	// Because the transferred data of the file MixProcessMode is quite large, we have to limit queue size to control memory usage here.
	if checkMixProcessMode(plugins) == file {
		logger.Infof(contextImp.GetRuntimeContext(), "no inputs in config %v, maybe file input, limit queue size", configName)
		logQueueSize = 10
	}

	logGroupSize := logstoreC.GlobalConfig.DefaultLogGroupQueueSize

	if err = logstoreC.PluginRunner.Init(logQueueSize, logGroupSize); err != nil {
		return nil, err
	}

	logstoreC.Statistics.Init(logstoreC.Context)

	// extensions should be initialized first
	pluginConfig, ok := plugins["extensions"]
	if ok {
		extensions, extensionsFound := pluginConfig.([]interface{})
		if !extensionsFound {
			return nil, fmt.Errorf("invalid extension type: %s, not json array", "extensions")
		}
		for _, extensionInterface := range extensions {
			extension, ok := extensionInterface.(map[string]interface{})
			if !ok {
				return nil, fmt.Errorf("invalid extension type")
			}
			if pluginTypeWithID, ok := extension["type"]; ok {
				pluginTypeWithIDStr, ok := pluginTypeWithID.(string)
				if !ok {
					return nil, fmt.Errorf("invalid extension type")
				}
				pluginType := getPluginType(pluginTypeWithIDStr)
				logger.Debug(contextImp.GetRuntimeContext(), "add extension", pluginType)
				err = loadExtension(logstoreC.genPluginMeta(pluginTypeWithIDStr), logstoreC, extension["detail"])
				if err != nil {
					return nil, err
				}
				contextImp.AddPlugin(pluginType)
			}
		}
	}

	pluginConfig, inputsFound := plugins["inputs"]
	if inputsFound {
		inputs, ok := pluginConfig.([]interface{})
		if ok {
			for _, inputInterface := range inputs {
				input, ok := inputInterface.(map[string]interface{})
				if ok {
					if pluginTypeWithID, ok := input["type"]; ok {
						if pluginTypeWithIDStr, ok := pluginTypeWithID.(string); ok {
							pluginType := getPluginType(pluginTypeWithIDStr)
							if _, isMetricInput := pipeline.MetricInputs[pluginType]; isMetricInput {
								// Load MetricInput plugin defined in pipeline.MetricInputs
								// pipeline.MetricInputs will be renamed in a future version
								err = loadMetric(logstoreC.genPluginMeta(pluginTypeWithIDStr), logstoreC, input["detail"])
							} else if _, isServiceInput := pipeline.ServiceInputs[pluginType]; isServiceInput {
								// Load ServiceInput plugin defined in pipeline.ServiceInputs
								err = loadService(logstoreC.genPluginMeta(pluginTypeWithIDStr), logstoreC, input["detail"])
							} else {
								err = fmt.Errorf("invalid input type: %s", pluginTypeWithIDStr)
							}
							if err != nil {
								return nil, err
							}
							contextImp.AddPlugin(pluginType)
							continue
						}
					}
				}
				return nil, fmt.Errorf("invalid input type")
			}
		} else {
			return nil, fmt.Errorf("invalid inputs type : %s, not json array", "inputs")
		}
	}
	pluginConfig, processorsFound := plugins["processors"]
	if processorsFound {
		processors, ok := pluginConfig.([]interface{})
		if ok {
			for i, processorInterface := range processors {
				processor, ok := processorInterface.(map[string]interface{})
				if ok {
					if pluginTypeWithID, ok := processor["type"]; ok {
						if pluginTypeWithIDStr, ok := pluginTypeWithID.(string); ok {
							pluginType := getPluginType(pluginTypeWithIDStr)
							logger.Debug(contextImp.GetRuntimeContext(), "add processor", pluginType)
							err = loadProcessor(logstoreC.genPluginMeta(pluginTypeWithIDStr), i, logstoreC, processor["detail"])
							if err != nil {
								return nil, err
							}
							contextImp.AddPlugin(pluginType)
							continue
						}
					}
				}
				return nil, fmt.Errorf("invalid processor type")
			}
		} else {
			return nil, fmt.Errorf("invalid processors type : %s, not json array", "processors")
		}
	}

	pluginConfig, aggregatorsFound := plugins["aggregators"]
	if aggregatorsFound {
		aggregators, ok := pluginConfig.([]interface{})
		if ok {
			for _, aggregatorInterface := range aggregators {
				aggregator, ok := aggregatorInterface.(map[string]interface{})
				if ok {
					if pluginTypeWithID, ok := aggregator["type"]; ok {
						if pluginTypeWithIDStr, ok := pluginTypeWithID.(string); ok {
							pluginType := getPluginType(pluginTypeWithIDStr)
							logger.Debug(contextImp.GetRuntimeContext(), "add aggregator", pluginType)
							err = loadAggregator(logstoreC.genPluginMeta(pluginTypeWithIDStr), logstoreC, aggregator["detail"])
							if err != nil {
								return nil, err
							}
							contextImp.AddPlugin(pluginType)
							continue
						}
					}
				}
				return nil, fmt.Errorf("invalid aggregator type")
			}
		} else {
			return nil, fmt.Errorf("invalid aggregator type : %s, not json array", "aggregators")
		}
	}
	if err = logstoreC.PluginRunner.AddDefaultAggregatorIfEmpty(); err != nil {
		return nil, err
	}

	pluginConfig, flushersFound := plugins["flushers"]
	if flushersFound {
		flushers, ok := pluginConfig.([]interface{})
		if ok {
			for _, flusherInterface := range flushers {
				flusher, ok := flusherInterface.(map[string]interface{})
				if ok {
					if pluginTypeWithID, ok := flusher["type"]; ok {
						if pluginTypeWithIDStr, ok := pluginTypeWithID.(string); ok {
							pluginType := getPluginType(pluginTypeWithIDStr)
							logger.Debug(contextImp.GetRuntimeContext(), "add flusher", pluginType)
							err = loadFlusher(logstoreC.genPluginMeta(pluginTypeWithIDStr), logstoreC, flusher["detail"])
							if err != nil {
								return nil, err
							}
							contextImp.AddPlugin(pluginType)
							continue
						}
					}
				}
				return nil, fmt.Errorf("invalid flusher type")
			}
		} else {
			return nil, fmt.Errorf("invalid flusher type : %s, not json array", "flushers")
		}
	}
	if err = logstoreC.PluginRunner.AddDefaultFlusherIfEmpty(); err != nil {
		return nil, err
	}
	return logstoreC, nil
}

func fetchPluginVersion(config map[string]interface{}) ConfigVersion {
	if v, ok := config["global"]; ok {
		if global, ok := v.(map[string]interface{}); ok {
			if version, ok := global["StructureType"]; ok {
				if str, ok := version.(string); ok {
					return ConfigVersion(strings.ToLower(str))
				}
			}
		}
	}
	return v1
}

func initPluginRunner(lc *LogstoreConfig) (PluginRunner, error) {
	switch lc.Version {
	case v1:
		return &pluginv1Runner{
			LogstoreConfig: lc,
			FlushOutStore:  NewFlushOutStore[protocol.LogGroup](),
		}, nil
	case v2:
		return &pluginv2Runner{
			LogstoreConfig: lc,
			FlushOutStore:  NewFlushOutStore[models.PipelineGroupEvents](),
		}, nil
	default:
		return nil, fmt.Errorf("undefined config version %s", lc.Version)
	}
}

func LoadLogstoreConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) error {
	if len(jsonStr) == 0 {
		logger.Info(context.Background(), "delete config", configName, "logstore", logstore)
		LogtailConfigLock.Lock()
		delete(LogtailConfig, configName)
		LogtailConfigLock.Unlock()
		return nil
	}
	logger.Info(context.Background(), "load config", configName, "logstore", logstore)
	logstoreC, err := createLogstoreConfig(project, logstore, configName, logstoreKey, jsonStr)
	if err != nil {
		return err
	}
	if logstoreC.PluginRunner.IsWithInputPlugin() {
		ToStartPipelineConfigWithInput = logstoreC
	} else {
		ToStartPipelineConfigWithoutInput = logstoreC
	}
	return nil
}

func UnloadPartiallyLoadedConfig(configName string) error {
	logger.Info(context.Background(), "unload config", configName)
	if ToStartPipelineConfigWithInput.ConfigNameWithSuffix == configName {
		ToStartPipelineConfigWithInput = nil
		return nil
	}
	if ToStartPipelineConfigWithoutInput.ConfigNameWithSuffix == configName {
		ToStartPipelineConfigWithoutInput = nil
		return nil
	}
	logger.Error(context.Background(), "unload config", "config not found", configName)
	return fmt.Errorf("config not found")
}

func loadBuiltinConfig(name string, project string, logstore string,
	configName string, cfgStr string) (*LogstoreConfig, error) {
	logger.Infof(context.Background(), "load built-in config %v, config name: %v, logstore: %v", name, configName, logstore)
	return createLogstoreConfig(project, logstore, configName, -1, cfgStr)
}

// loadMetric creates a metric plugin object and append to logstoreConfig.MetricPlugins.
// @pluginType: the type of metric plugin.
// @logstoreConfig: where to store the created metric plugin object.
// It returns any error encountered.
func loadMetric(pluginMeta *pipeline.PluginMeta, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.MetricInputs[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginMeta.PluginType)
	}
	metric := creator()
	if err = applyPluginConfig(metric, configInterface); err != nil {
		return err
	}

	interval := logstoreConfig.GlobalConfig.InputIntervalMs
	configMapI, convertSuc := configInterface.(map[string]interface{})
	if convertSuc {
		valI, keyExist := configMapI["IntervalMs"]
		if keyExist {
			if val, convSuc := valI.(float64); convSuc {
				interval = int(val)
			}
		}
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginMetricInput, metric, map[string]interface{}{"interval": interval})
}

// loadService creates a service plugin object and append to logstoreConfig.ServicePlugins.
// @pluginType: the type of service plugin.
// @logstoreConfig: where to store the created service plugin object.
// It returns any error encountered.
func loadService(pluginMeta *pipeline.PluginMeta, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.ServiceInputs[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginMeta.PluginType)
	}
	service := creator()
	if err = applyPluginConfig(service, configInterface); err != nil {
		return err
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginServiceInput, service, map[string]interface{}{})
}

func loadProcessor(pluginMeta *pipeline.PluginMeta, priority int, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.Processors[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		logger.Error(logstoreConfig.Context.GetRuntimeContext(), "INVALID_PROCESSOR_TYPE", "invalid processor type, maybe type is wrong or logtail version is too old", pluginMeta.PluginType)
		return nil
	}
	processor := creator()
	if err = applyPluginConfig(processor, configInterface); err != nil {
		return err
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginProcessor, processor, map[string]interface{}{"priority": priority})
}

func loadAggregator(pluginMeta *pipeline.PluginMeta, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.Aggregators[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		logger.Error(logstoreConfig.Context.GetRuntimeContext(), "INVALID_AGGREGATOR_TYPE", "invalid aggregator type, maybe type is wrong or logtail version is too old", pluginMeta.PluginType)
		return nil
	}
	aggregator := creator()
	if err = applyPluginConfig(aggregator, configInterface); err != nil {
		return err
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginAggregator, aggregator, map[string]interface{}{})
}

func loadFlusher(pluginMeta *pipeline.PluginMeta, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.Flushers[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginMeta.PluginType)
	}
	flusher := creator()
	if err = applyPluginConfig(flusher, configInterface); err != nil {
		return err
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginFlusher, flusher, map[string]interface{}{})
}

func loadExtension(pluginMeta *pipeline.PluginMeta, logstoreConfig *LogstoreConfig, configInterface interface{}) (err error) {
	creator, existFlag := pipeline.Extensions[pluginMeta.PluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginMeta.PluginType)
	}
	extension := creator()
	if err = applyPluginConfig(extension, configInterface); err != nil {
		return err
	}
	if err = extension.Init(logstoreConfig.Context); err != nil {
		return err
	}
	return logstoreConfig.PluginRunner.AddPlugin(pluginMeta, pluginExtension, extension, map[string]interface{}{})
}

func applyPluginConfig(plugin interface{}, pluginConfig interface{}) error {
	config, err := json.Marshal(pluginConfig)
	if err != nil {
		return err
	}
	err = json.Unmarshal(config, plugin)
	return err
}

// Rule: pluginTypeWithID=pluginType/pluginID#pluginPriority.
func getPluginType(pluginTypeWithID string) string {
	if ids := strings.IndexByte(pluginTypeWithID, '/'); ids != -1 {
		return pluginTypeWithID[:ids]
	}
	return pluginTypeWithID
}

func (lc *LogstoreConfig) genPluginMeta(pluginTypeWithID string) *pipeline.PluginMeta {
	if isPluginTypeWithID(pluginTypeWithID) {
		pluginTypeWithID := pluginTypeWithID
		if idx := strings.IndexByte(pluginTypeWithID, '#'); idx != -1 {
			pluginTypeWithID = pluginTypeWithID[:idx]
		}
		if ids := strings.IndexByte(pluginTypeWithID, '/'); ids != -1 {
			if pluginID, err := strconv.ParseInt(pluginTypeWithID[ids+1:], 10, 32); err == nil {
				atomic.StoreInt32(&lc.extensionIndex, 0)
				atomic.StoreInt32(&lc.pluginID, int32(pluginID))
			}
			return &pipeline.PluginMeta{
				PluginTypeWithID: getPluginTypeWithID(pluginTypeWithID),
				PluginType:       getPluginType(pluginTypeWithID),
				PluginID:         getPluginID(pluginTypeWithID),
			}
		}
	}

	// only for anonymous extensions.
	pluginType := pluginTypeWithID
	pluginID := lc.genPluginID()
	pluginTypeWithID = fmt.Sprintf("%s/%s", pluginType, pluginID)
	return &pipeline.PluginMeta{
		PluginTypeWithID: getPluginTypeWithID(pluginTypeWithID),
		PluginType:       getPluginType(pluginTypeWithID),
		PluginID:         getPluginID(pluginTypeWithID),
	}
}

func isPluginTypeWithID(pluginTypeWithID string) bool {
	if idx := strings.IndexByte(pluginTypeWithID, '/'); idx != -1 {
		return true
	}
	return false
}

func getPluginID(pluginTypeWithID string) string {
	slashCount := strings.Count(pluginTypeWithID, "/")
	switch slashCount {
	case 0:
		return ""
	case 1:
		if idx := strings.IndexByte(pluginTypeWithID, '/'); idx != -1 {
			return pluginTypeWithID[idx+1:]
		}
	default:
		if firstIdx := strings.IndexByte(pluginTypeWithID, '/'); firstIdx != -1 {
			if lastIdx := strings.LastIndexByte(pluginTypeWithID, '/'); lastIdx != -1 {
				return pluginTypeWithID[firstIdx+1 : lastIdx]
			}
		}
	}
	return ""
}

func getPluginTypeWithID(pluginTypeWithID string) string {
	return fmt.Sprintf("%s/%s", getPluginType(pluginTypeWithID), getPluginID(pluginTypeWithID))
}

func GetPluginPriority(pluginTypeWithID string) int {
	if idx := strings.IndexByte(pluginTypeWithID, '#'); idx != -1 {
		val, err := strconv.Atoi(pluginTypeWithID[idx+1:])
		if err != nil {
			return 0
		}
		return val
	}
	return 0
}

func (lc *LogstoreConfig) genPluginID() string {
	return fmt.Sprintf("%v_%v", atomic.LoadInt32(&lc.pluginID), atomic.AddInt32(&lc.extensionIndex, 1))
}

func init() {
	LogtailConfigLock.Lock()
	LogtailConfig = make(map[string]*LogstoreConfig)
	LogtailConfigLock.Unlock()
}
