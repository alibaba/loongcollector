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
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugin_main/flags"
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
	CollecLatencytMetric ilogtail.LatencyMetric
	RawLogMetric         ilogtail.CounterMetric
	SplitLogMetric       ilogtail.CounterMetric
	FlushLogMetric       ilogtail.CounterMetric
	FlushLogGroupMetric  ilogtail.CounterMetric
	FlushReadyMetric     ilogtail.CounterMetric
	FlushLatencyMetric   ilogtail.LatencyMetric
}

type LogstoreConfig struct {
	ProjectName       string
	LogstoreName      string
	ConfigName        string
	LogstoreKey       int64
	MetricPlugins     []*MetricWrapper
	ServicePlugins    []*ServiceWrapper
	ProcessorPlugins  []*ProcessorWrapper
	AggregatorPlugins []*AggregatorWrapper
	FlusherPlugins    []*FlusherWrapper

	// Each LogstoreConfig can have its independent GlobalConfig if the "global" field
	//   is offered in configuration, see build-in StatisticsConfig and AlarmConfig.
	GlobalConfig *GlobalConfig

	LogsChan      chan *protocol.Log
	LogGroupsChan chan *protocol.LogGroup

	processWaitSema   sync.WaitGroup
	flushWaitSema     sync.WaitGroup
	processShutdown   chan struct{}
	flushShutdown     chan struct{}
	Context           ilogtail.Context
	Statistics        LogstoreStatistics
	FlushOutLogGroups []*protocol.LogGroup
	FlushOutFlag      bool

	pauseChan       chan struct{}
	resumeChan      chan struct{}
	pauseOrResumeWg sync.WaitGroup

	alreadyStarted   bool // if this flag is true, do not start it when config Resume
	configDetailHash string
}

func (p *LogstoreStatistics) Init(context ilogtail.Context) {
	p.CollecLatencytMetric = helper.NewLatencyMetric("collect_latency")
	p.RawLogMetric = helper.NewCounterMetric("raw_log")
	p.SplitLogMetric = helper.NewCounterMetric("processed_log")
	p.FlushLogMetric = helper.NewCounterMetric("flush_log")
	p.FlushLogGroupMetric = helper.NewCounterMetric("flush_loggroup")
	p.FlushReadyMetric = helper.NewAverageMetric("flush_ready")
	p.FlushLatencyMetric = helper.NewLatencyMetric("flush_latency")

	context.RegisterLatencyMetric(p.CollecLatencytMetric)
	context.RegisterCounterMetric(p.RawLogMetric)
	context.RegisterCounterMetric(p.SplitLogMetric)
	context.RegisterCounterMetric(p.FlushLogMetric)
	context.RegisterCounterMetric(p.FlushLogGroupMetric)
	context.RegisterCounterMetric(p.FlushReadyMetric)
	context.RegisterLatencyMetric(p.FlushLatencyMetric)
}

// Start initializes plugin instances in config and starts them.
// Procedures:
// 1. Start flusher goroutine and push FlushOutLogGroups inherited from last config
//   instance to LogGroupsChan, so that they can be flushed to flushers.
// 2. Start aggregators, allocate new goroutine for each one.
// 3. Start processor goroutine to process logs from LogsChan.
// 4. Start inputs (including metrics and services), just like aggregator, each input
//   has its own goroutine.
func (lc *LogstoreConfig) Start() {
	lc.FlushOutFlag = false
	logger.Info(lc.Context.GetRuntimeContext(), "config start", "begin")

	lc.pauseChan = make(chan struct{}, 1)
	lc.resumeChan = make(chan struct{}, 1)

	lc.flushShutdown = make(chan struct{}, 1)
	lc.flushWaitSema.Add(1)
	go func() {
		defer lc.flushWaitSema.Done()
		lc.flushInternal()
	}()
	for i := 0; i < len(lc.FlushOutLogGroups); i++ {
		lc.LogGroupsChan <- lc.FlushOutLogGroups[i]
		lc.FlushOutLogGroups[i] = nil
	}
	lc.FlushOutLogGroups = lc.FlushOutLogGroups[:0]

	for _, aggregator := range lc.AggregatorPlugins {
		go func(aw *AggregatorWrapper) {
			aw.Run()
		}(aggregator)
	}

	lc.processShutdown = make(chan struct{}, 1)
	lc.processWaitSema.Add(1)
	go func() {
		defer lc.processWaitSema.Done()
		lc.processInternal()
	}()

	for _, metric := range lc.MetricPlugins {
		go func(mw *MetricWrapper) {
			mw.Run()
		}(metric)
	}
	for _, service := range lc.ServicePlugins {
		service.Run()
	}

	logger.Info(lc.Context.GetRuntimeContext(), "config start", "success")
}

func (lc *LogstoreConfig) TryFlushLoggroups() bool {
	for _, flusher := range lc.FlusherPlugins {
		for waitCount := 0; !flusher.Flusher.IsReady(lc.ProjectName, lc.LogstoreName, lc.LogstoreKey); waitCount++ {
			if waitCount > maxFlushOutTime*100 {
				logger.Error(lc.Context.GetRuntimeContext(), "DROP_DATA_ALARM", "flush out data timeout, drop loggroups", len(lc.FlushOutLogGroups))
				return false
			}
			lc.Statistics.FlushReadyMetric.Add(0)
			time.Sleep(time.Duration(10) * time.Millisecond)
		}
		lc.Statistics.FlushReadyMetric.Add(1)
		lc.Statistics.FlushLatencyMetric.Begin()
		err := flusher.Flusher.Flush(lc.ProjectName, lc.LogstoreName, lc.ConfigName, lc.FlushOutLogGroups)
		lc.Statistics.FlushLatencyMetric.End()
		if err != nil {
			logger.Error(lc.Context.GetRuntimeContext(), "FLUSH_DATA_ALARM", "flush data error", lc.ProjectName, lc.LogstoreName, err)
		}
	}
	lc.FlushOutLogGroups = make([]*protocol.LogGroup, 0)
	return true
}

// Stop stops plugin instances and corresponding goroutines of config.
// @exitFlag passed from Logtail, indicates that if Logtail will quit after this.
// Procedures:
// 1. SetUrgent to all flushers to indicate them current state.
// 2. Stop all input plugins, stop generating logs.
// 3. Stop processor goroutine, pass all existing logs to aggregator.
// 4. Stop all aggregator plugins, make all logs to LogGroups.
// 5. Set stopping flag, stop flusher goroutine.
// 6. If Logtail is exiting and there are remaining data, try to flush once.
// 7. Stop flusher plugins.
func (lc *LogstoreConfig) Stop(exitFlag bool) error {
	logger.Info(lc.Context.GetRuntimeContext(), "config stop", "begin", "exit", exitFlag)

	for _, flusher := range lc.FlusherPlugins {
		flusher.Flusher.SetUrgent(exitFlag)
	}

	for _, metric := range lc.MetricPlugins {
		metric.Stop()
	}
	logger.Info(lc.Context.GetRuntimeContext(), "metric stop", "done")
	for _, service := range lc.ServicePlugins {
		_ = service.Stop()
	}
	logger.Info(lc.Context.GetRuntimeContext(), "service stop", "done")

	close(lc.processShutdown)
	lc.processWaitSema.Wait()
	logger.Info(lc.Context.GetRuntimeContext(), "processor stop", "done")

	for _, aggregator := range lc.AggregatorPlugins {
		aggregator.Stop()
	}
	logger.Info(lc.Context.GetRuntimeContext(), "aggregator stop", "done")

	lc.FlushOutFlag = true
	close(lc.flushShutdown)
	lc.flushWaitSema.Wait()
	logger.Info(lc.Context.GetRuntimeContext(), "flusher goroutine stop", "done")

	if exitFlag && len(lc.FlushOutLogGroups) > 0 {
		logger.Info(lc.Context.GetRuntimeContext(), "flushout loggroups, count", len(lc.FlushOutLogGroups))
		rst := lc.TryFlushLoggroups()
		logger.Info(lc.Context.GetRuntimeContext(), "flushout loggroups, result", rst)
	}

	for idx, flusher := range lc.FlusherPlugins {
		err := flusher.Flusher.Stop()
		if err != nil {
			logger.Warningf(lc.Context.GetRuntimeContext(), "STOP_FLUSHER_ALARM",
				"Failed to stop %vth flusher (description: %v): %v",
				idx, flusher.Flusher.Description(), err)
		}
	}
	logger.Info(lc.Context.GetRuntimeContext(), "flusher stop", "done")

	close(lc.pauseChan)
	close(lc.resumeChan)

	logger.Info(lc.Context.GetRuntimeContext(), "config stop", "success")
	return nil
}

// processInternal is the routine of processors.
// Each LogstoreConfig has its own goroutine for this routine.
// When log is ready (passed through LogsChan), we will try to get
//   all available logs from the channel, and pass them together to processors.
// All processors of the config share same gogroutine, logs are passed to them
//   one by one, just like logs -> p1 -> p2 -> p3 -> logsGoToNextStep.
// It returns when processShutdown is closed.
func (lc *LogstoreConfig) processInternal() {
	defer panicRecover(lc.ConfigName)
	var log *protocol.Log
	for {
		select {
		case <-lc.processShutdown:
			if len(lc.LogsChan) == 0 {
				return
			}
		case log = <-lc.LogsChan:
			listLen := len(lc.LogsChan) + 1
			logs := make([]*protocol.Log, listLen)
			logs[0] = log
			for i := 1; i < listLen; i++ {
				logs[i] = <-lc.LogsChan
			}
			lc.Statistics.RawLogMetric.Add(int64(len(logs)))
			for _, processor := range lc.ProcessorPlugins {
				logs = processor.Processor.ProcessLogs(logs)
				if len(logs) == 0 {
					break
				}
			}
			nowTime := (uint32)(time.Now().Unix())

			if len(logs) > 0 {
				lc.Statistics.SplitLogMetric.Add(int64(len(logs)))
				for _, aggregator := range lc.AggregatorPlugins {
					for _, l := range logs {
						if len(l.Contents) == 0 {
							continue
						}
						if l.Time == uint32(0) {
							l.Time = nowTime
						}
						for tryCount := 1; true; tryCount++ {
							err := aggregator.Aggregator.Add(l)
							if err == nil {
								break
							}
							// wait until shutdown is active
							if tryCount%100 == 0 {
								logger.Warning(lc.Context.GetRuntimeContext(), "AGGREGATOR_ADD_ALARM", "error", err)
							}
							time.Sleep(time.Millisecond * 10)
						}
					}
				}
			}
		}
	}
}

// flushInternal is the routine of flushers.
// Each LogstoreConfig has its own goroutine for this routine.
// All flushers of @logstoreConfig share this goroutine, when log group is ready (passed
// through @logstoreConfig.LogGroupsChan), it will be flushed by all flushers one by one.
// It returns when flushShutdown is closed.
func (lc *LogstoreConfig) flushInternal() {
	defer panicRecover(lc.ConfigName)
	var logGroup *protocol.LogGroup
	for {
		select {
		case <-lc.flushShutdown:
			if len(lc.LogGroupsChan) == 0 {
				return
			}
		case <-lc.pauseChan:
			lc.waitForResume()

		case logGroup = <-lc.LogGroupsChan:
			if logGroup == nil {
				continue
			}

			// Check pause status if config is still alive, if paused, wait for resume.
			select {
			case <-lc.pauseChan:
				lc.waitForResume()
			default:
			}

			listLen := len(lc.LogGroupsChan) + 1
			logGroups := make([]*protocol.LogGroup, listLen)
			logGroups[0] = logGroup
			for i := 1; i < listLen; i++ {
				logGroups[i] = <-lc.LogGroupsChan
			}
			lc.Statistics.FlushLogGroupMetric.Add(int64(len(logGroups)))

			// Add tags for each non-empty LogGroup, includes: default hostname tag,
			// env tags and global tags in config.
			for _, logGroup := range logGroups {
				if len(logGroup.Logs) == 0 {
					continue
				}
				lc.Statistics.FlushLogMetric.Add(int64(len(logGroup.Logs)))
				logGroup.Source = util.GetIPAddress()
				logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: "__hostname__", Value: util.GetHostName()})
				for i := 0; i < len(helper.EnvTags); i += 2 {
					logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: helper.EnvTags[i], Value: helper.EnvTags[i+1]})
				}
				for key, value := range lc.GlobalConfig.Tags {
					logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: key, Value: value})
				}
			}

			// Flush LogGroups to all flushers.
			// Note: multiple flushers is unrecommended, because all flushers will
			//   be blocked if one of them is unready.
			for {
				allReady := true
				for _, flusher := range lc.FlusherPlugins {
					if !flusher.Flusher.IsReady(lc.ProjectName,
						lc.LogstoreName, lc.LogstoreKey) {
						allReady = false
						break
					}
				}
				if allReady {
					for _, flusher := range lc.FlusherPlugins {
						lc.Statistics.FlushReadyMetric.Add(1)
						lc.Statistics.FlushLatencyMetric.Begin()
						err := flusher.Flusher.Flush(lc.ProjectName,
							lc.LogstoreName, lc.ConfigName, logGroups)
						lc.Statistics.FlushLatencyMetric.End()
						if err != nil {
							logger.Error(lc.Context.GetRuntimeContext(), "FLUSH_DATA_ALARM", "flush data error",
								lc.ProjectName, lc.LogstoreName, err)
						}
					}
					break
				}
				if !lc.FlushOutFlag {
					time.Sleep(time.Duration(10) * time.Millisecond)
					continue
				}

				// Config is stopping, move unflushed LogGroups to FlushOutLogGroups.
				logger.Info(lc.Context.GetRuntimeContext(), "flush loggroup to slice, loggroup count", listLen)
				lc.FlushOutLogGroups = append(lc.FlushOutLogGroups, logGroups...)
				break
			}
		}
	}
}

func (lc *LogstoreConfig) pause() {
	lc.pauseOrResumeWg.Add(1)
	lc.pauseChan <- struct{}{}
	lc.pauseOrResumeWg.Wait()
}

func (lc *LogstoreConfig) waitForResume() {
	lc.pauseOrResumeWg.Done()
	select {
	case <-lc.resumeChan:
		lc.pauseOrResumeWg.Done()
	case <-lc.flushShutdown:
	}
}

func (lc *LogstoreConfig) resume() {
	lc.pauseOrResumeWg.Add(1)
	lc.resumeChan <- struct{}{}
	lc.pauseOrResumeWg.Wait()
}

const (
	rawStringKey     = "content"
	defaultTagPrefix = "__tag__:__prefix__"
)

var (
	tagDelimiter = []byte("^^^")
	tagSeparator = []byte("~=~")
)

func (lc *LogstoreConfig) ProcessRawLog(rawLog []byte, packID string, topic string) int {
	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: rawStringKey, Value: string(rawLog)})
	logger.Debug(context.Background(), "Process raw log ", packID, topic, len(rawLog))
	lc.LogsChan <- log
	return 0
}

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
	extractTags(tags, log)
	lc.LogsChan <- log
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
	extractTags(tags, log)
	lc.LogsChan <- log
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
		typeName, valid := cfg["type"]
		if !valid {
			continue
		}
		if val, valid := typeName.(string); valid && val == input.ServiceDockerStdoutPluginName {
			return true
		}
	}
	return false
}

var enableAlwaysOnlineForStdout = false

func createLogstoreConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) (*LogstoreConfig, error) {
	var err error
	contextImp := &ContextImp{}
	contextImp.InitContext(project, logstore, configName)
	logstoreC := &LogstoreConfig{
		ProjectName:      project,
		LogstoreName:     logstore,
		ConfigName:       configName,
		LogstoreKey:      logstoreKey,
		Context:          contextImp,
		configDetailHash: fmt.Sprintf("%x", md5.Sum([]byte(jsonStr))), //nolint:gosec
	}

	// Check if the config has been disabled (keep disabled if config detail is unchanged).
	DisabledLogtailConfigLock.Lock()
	if disabledConfig, hasDisabled := DisabledLogtailConfig[configName]; hasDisabled {
		if disabledConfig.configDetailHash == logstoreC.configDetailHash {
			DisabledLogtailConfigLock.Unlock()
			return nil, fmt.Errorf("failed to create config because timeout "+
				"stop has happened on it: %v", configName)
		}
		delete(DisabledLogtailConfig, configName)
		DisabledLogtailConfigLock.Unlock()
		logger.Info(contextImp.GetRuntimeContext(), "retry timeout config because config detail has changed")
	} else {
		DisabledLogtailConfigLock.Unlock()
	}

	// Move unsent LogGroups from last config to new config.
	if lastConfig, hasLastConfig := LastLogtailConfig[configName]; hasLastConfig {
		if len(lastConfig.FlushOutLogGroups) > 0 {
			logger.Info(contextImp.GetRuntimeContext(), "repush loggroup to logstore, count",
				len(lastConfig.FlushOutLogGroups))
			logstoreC.FlushOutLogGroups = lastConfig.FlushOutLogGroups
		}
	}

	// check AlwaysOnlineManager
	if oldConfig, ok := GetAlwaysOnlineManager().GetCachedConfig(configName); ok {
		if oldConfig.configDetailHash == logstoreC.configDetailHash {
			logstoreC = oldConfig
			logstoreC.alreadyStarted = true
			logger.Info(contextImp.GetRuntimeContext(), "config is same after reload, use it again",
				len(logstoreC.FlushOutLogGroups))
			return logstoreC, nil
		}
		_ = oldConfig.Stop(false)
		logstoreC.FlushOutLogGroups = oldConfig.FlushOutLogGroups
		logger.Info(contextImp.GetRuntimeContext(), "config is changed after reload, stop and create a new one",
			len(oldConfig.FlushOutLogGroups))
	}

	var plugins = make(map[string]interface{})
	if err = json.Unmarshal([]byte(jsonStr), &plugins); err != nil {
		return nil, err
	}
	enableAlwaysOnline := enableAlwaysOnlineForStdout && hasDockerStdoutInput(plugins)

	logstoreC.GlobalConfig = &LogtailGlobalConfig
	// If plugins config has "global" field, then override the logstoreC.GlobalConfig
	if pluginConfigInterface, flag := plugins["global"]; flag || enableAlwaysOnline {
		pluginConfig := &GlobalConfig{}
		*pluginConfig = LogtailGlobalConfig
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
		if enableAlwaysOnline {
			pluginConfig.AlwaysOnline = true
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
	logstoreC.LogsChan = make(chan *protocol.Log, logQueueSize)
	// loggroup chan size must >= flushout loggroups
	logGroupSize := logstoreC.GlobalConfig.DefaultLogGroupQueueSize
	if logGroupSize < len(logstoreC.FlushOutLogGroups) {
		logger.Info(contextImp.GetRuntimeContext(), "config", configName,
			"expand loggroup chan size from", logGroupSize,
			"to", len(logstoreC.FlushOutLogGroups))
		logGroupSize = len(logstoreC.FlushOutLogGroups)
	}
	logstoreC.LogGroupsChan = make(chan *protocol.LogGroup, logGroupSize)
	logstoreC.Statistics.Init(logstoreC.Context)

	for pluginType, pluginConfig := range plugins {
		if pluginType == "inputs" {
			inputs, ok := pluginConfig.([]interface{})
			if ok {
				for _, inputInterface := range inputs {
					input, ok := inputInterface.(map[string]interface{})
					if ok {
						if typeName, ok := input["type"]; ok {
							if typeNameStr, ok := typeName.(string); ok {
								if strings.HasPrefix(typeNameStr, "metric_") {
									err = loadMetric(getPluginType(typeNameStr), logstoreC, input["detail"])
								} else if strings.HasPrefix(typeNameStr, "service_") {
									err = loadService(getPluginType(typeNameStr), logstoreC, input["detail"])
								}
								if err != nil {
									return nil, err
								}
								contextImp.AddPlugin(typeNameStr)
								continue
							}

						}
					}
					return nil, fmt.Errorf("invalid input type")
				}
			} else {
				return nil, fmt.Errorf("invalid inputs type : %s, not json array", pluginType)
			}
			continue
		}

		if pluginType == "processors" {
			processors, ok := pluginConfig.([]interface{})
			if ok {
				for i, processorInterface := range processors {
					processor, ok := processorInterface.(map[string]interface{})
					if ok {
						if typeName, ok := processor["type"]; ok {
							if typeNameStr, ok := typeName.(string); ok {
								logger.Debug(contextImp.GetRuntimeContext(), "add processor", typeNameStr)
								err = loadProcessor(getPluginType(typeNameStr), i, logstoreC, processor["detail"])
								if err != nil {
									return nil, err
								}
								contextImp.AddPlugin(typeNameStr)
								continue
							}
						}
					}
					return nil, fmt.Errorf("invalid processor type")
				}
			} else {
				return nil, fmt.Errorf("invalid processors type : %s, not json array", pluginType)
			}
			continue
		}

		if pluginType == "aggregators" {
			aggregators, ok := pluginConfig.([]interface{})
			if ok {
				for _, aggregatorInterface := range aggregators {
					aggregator, ok := aggregatorInterface.(map[string]interface{})
					if ok {
						if typeName, ok := aggregator["type"]; ok {
							if typeNameStr, ok := typeName.(string); ok {
								logger.Debug(contextImp.GetRuntimeContext(), "add aggregator", typeNameStr)
								err = loadAggregator(getPluginType(typeNameStr), logstoreC, aggregator["detail"])
								if err != nil {
									return nil, err
								}
								contextImp.AddPlugin(typeNameStr)
								continue
							}
						}
					}
					return nil, fmt.Errorf("invalid aggregator type")
				}
			} else {
				return nil, fmt.Errorf("invalid aggregator type : %s, not json array", pluginType)
			}
			continue
		}

		if pluginType == "flushers" {
			flushers, ok := pluginConfig.([]interface{})
			if ok {
				for _, flusherInterface := range flushers {
					flusher, ok := flusherInterface.(map[string]interface{})
					if ok {
						if typeName, ok := flusher["type"]; ok {
							if typeNameStr, ok := typeName.(string); ok {
								logger.Debug(contextImp.GetRuntimeContext(), "add flusher", typeNameStr)
								err = loadFlusher(getPluginType(typeNameStr), logstoreC, flusher["detail"])
								if err != nil {
									return nil, err
								}
								contextImp.AddPlugin(typeNameStr)
								continue
							}
						}
					}
					return nil, fmt.Errorf("invalid flusher type")
				}
			} else {
				return nil, fmt.Errorf("invalid flusher type : %s, not json array", pluginType)
			}
			continue
		}

		if pluginType != "global" && pluginType != mixProcessModeFlag {
			return nil, fmt.Errorf("error plugin name %s", pluginType)
		}
	}
	if len(logstoreC.AggregatorPlugins) == 0 {
		logger.Debug(contextImp.GetRuntimeContext(), "add default aggregator")
		_ = loadAggregator("aggregator_default", logstoreC, nil)
	}
	if len(logstoreC.FlusherPlugins) == 0 {
		logger.Debug(contextImp.GetRuntimeContext(), "add default flusher")
		category, options := flags.GetFlusherConfiguration()
		_ = loadFlusher(category, logstoreC, options)
	}
	return logstoreC, nil
}

func LoadLogstoreConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) error {
	if len(jsonStr) == 0 {
		logger.Info(context.Background(), "delete config", configName, "logstore", logstore)
		delete(LogtailConfig, configName)
		return nil
	}
	logger.Info(context.Background(), "load config", configName, "logstore", logstore)
	logstoreC, err := createLogstoreConfig(project, logstore, configName, logstoreKey, jsonStr)
	if err != nil {
		return err
	}
	LogtailConfig[configName] = logstoreC
	return nil
}

func loadBuiltinConfig(name string, project string, logstore string,
	configName string, cfgStr string) (*LogstoreConfig, error) {
	logger.Infof(context.Background(), "load built-in config %v, config name: %v, logstore: %v", name, configName, logstore)
	return createLogstoreConfig(project, logstore, configName, 0, cfgStr)
}

// loadMetric creates a metric plugin object and append to logstoreConfig.MetricPlugins.
// @pluginType: the type of metric plugin.
// @logstoreConfig: where to store the created metric plugin object.
// It returns any error encountered.
func loadMetric(pluginType string, logstoreConfig *LogstoreConfig, configInterface interface{}) error {
	creator, existFlag := ilogtail.MetricInputs[pluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginType)
	}
	metric := creator()
	jsonStr, err := json.Marshal(configInterface)
	if err != nil {
		return err
	}
	err = json.Unmarshal(jsonStr, metric)
	if err != nil {
		return err
	}
	interval, err := metric.Init(logstoreConfig.Context)
	if err != nil {
		return err
	}
	if interval == 0 {
		interval = logstoreConfig.GlobalConfig.InputIntervalMs
		configMapI, convertSuc := configInterface.(map[string]interface{})
		if convertSuc {
			valI, keyExist := configMapI["IntervalMs"]
			if keyExist {
				if val, convSuc := valI.(float64); convSuc {
					interval = int(val)
				}
			}
		}
	}
	var wrapper MetricWrapper
	wrapper.Config = logstoreConfig
	wrapper.Input = metric
	wrapper.Interval = time.Duration(interval) * time.Millisecond
	wrapper.LogsChan = logstoreConfig.LogsChan
	wrapper.LatencyMetric = logstoreConfig.Statistics.CollecLatencytMetric
	logstoreConfig.MetricPlugins = append(logstoreConfig.MetricPlugins, &wrapper)
	return nil
}

// loadService creates a service plugin object and append to logstoreConfig.ServicePlugins.
// @pluginType: the type of service plugin.
// @logstoreConfig: where to store the created service plugin object.
// It returns any error encountered.
func loadService(pluginType string, logstoreConfig *LogstoreConfig, configInterface interface{}) error {
	creator, existFlag := ilogtail.ServiceInputs[pluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginType)
	}
	service := creator()
	jsonStr, err := json.Marshal(configInterface)
	if err != nil {
		return err
	}
	err = json.Unmarshal(jsonStr, service)
	if err != nil {
		return err
	}
	interval, err := service.Init(logstoreConfig.Context)
	if err != nil {
		return err
	}
	if interval == 0 {
		interval = logstoreConfig.GlobalConfig.InputIntervalMs
	}
	var wrapper ServiceWrapper
	wrapper.Config = logstoreConfig
	wrapper.Input = service
	wrapper.Interval = time.Duration(interval) * time.Millisecond
	wrapper.LogsChan = logstoreConfig.LogsChan
	logstoreConfig.ServicePlugins = append(logstoreConfig.ServicePlugins, &wrapper)
	return nil
}

func loadProcessor(pluginType string, priority int, logstoreConfig *LogstoreConfig, configInterface interface{}) error {
	creator, existFlag := ilogtail.Processors[pluginType]
	if !existFlag || creator == nil {
		logger.Error(logstoreConfig.Context.GetRuntimeContext(), "INVALID_PROCESSOR_TYPE", "invalid processor type, maybe type is wrong or logtail version is too old", pluginType)
		return nil
	}
	processor := creator()
	jsonStr, err := json.Marshal(configInterface)
	if err != nil {
		return err
	}
	err = json.Unmarshal(jsonStr, processor)
	if err != nil {
		return err
	}
	err = processor.Init(logstoreConfig.Context)
	if err != nil {
		return err
	}

	var wrapper ProcessorWrapper
	wrapper.Config = logstoreConfig
	wrapper.Processor = processor
	wrapper.LogsChan = logstoreConfig.LogsChan
	wrapper.Priority = priority
	logstoreConfig.ProcessorPlugins = append(logstoreConfig.ProcessorPlugins, &wrapper)
	return nil
}

func loadAggregator(pluginType string, logstoreConfig *LogstoreConfig, configInterface interface{}) error {
	creator, existFlag := ilogtail.Aggregators[pluginType]
	if !existFlag || creator == nil {
		logger.Error(logstoreConfig.Context.GetRuntimeContext(), "INVALID_AGGREGATOR_TYPE", "invalid aggregator type, maybe type is wrong or logtail version is too old", pluginType)
		return nil
	}
	aggregator := creator()
	jsonStr, err := json.Marshal(configInterface)
	if err != nil {
		return err
	}
	err = json.Unmarshal(jsonStr, aggregator)
	if err != nil {
		return err
	}

	var wrapper AggregatorWrapper
	wrapper.Config = logstoreConfig
	wrapper.Aggregator = aggregator
	wrapper.LogGroupsChan = logstoreConfig.LogGroupsChan
	interval, err := aggregator.Init(logstoreConfig.Context, &wrapper)
	if err != nil {
		return err
	}
	if interval == 0 {
		interval = logstoreConfig.GlobalConfig.AggregatIntervalMs
	}
	wrapper.Interval = time.Millisecond * time.Duration(interval)
	logstoreConfig.AggregatorPlugins = append(logstoreConfig.AggregatorPlugins, &wrapper)
	return nil
}

func loadFlusher(pluginType string, logstoreConfig *LogstoreConfig, configInterface interface{}) error {

	creator, existFlag := ilogtail.Flushers[pluginType]
	if !existFlag || creator == nil {
		return fmt.Errorf("can't find plugin %s", pluginType)
	}
	flusher := creator()
	jsonStr, err := json.Marshal(configInterface)
	if err != nil {
		return err
	}
	err = json.Unmarshal(jsonStr, flusher)
	if err != nil {
		return err
	}
	err = flusher.Init(logstoreConfig.Context)
	if err != nil {
		return err
	}

	var wrapper FlusherWrapper
	wrapper.Config = logstoreConfig
	wrapper.Flusher = flusher
	wrapper.LogGroupsChan = logstoreConfig.LogGroupsChan
	wrapper.Interval = time.Millisecond * time.Duration(logstoreConfig.GlobalConfig.FlushIntervalMs)
	logstoreConfig.FlusherPlugins = append(logstoreConfig.FlusherPlugins, &wrapper)
	return nil
}

// getPluginType extracts plugin type from pluginName.
// Rule: pluginName=pluginType#pluginPriority.
// It returns the plugin type.
func getPluginType(pluginName string) string {
	if idx := strings.IndexByte(pluginName, '#'); idx != -1 {
		return pluginName[:idx]
	}
	return pluginName
}

func GetPluginPriority(pluginName string) int {
	if idx := strings.IndexByte(pluginName, '#'); idx != -1 {
		val, err := strconv.Atoi(pluginName[idx+1:])
		if err != nil {
			return 0
		}
		return val
	}
	return 0
}

func init() {
	LogtailConfig = make(map[string]*LogstoreConfig)
	_ = util.InitFromEnvBool("ALIYUN_LOGTAIL_ENABLE_ALWAYS_ONLINE_FOR_STDOUT", &enableAlwaysOnlineForStdout, false)
}
