// Copyright 2022 iLogtail Authors
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
	"time"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type pluginv1Runner struct {
	// pipeline v1 fields
	LogsChan      chan *pipeline.LogWithContext
	LogGroupsChan chan *protocol.LogGroup

	MetricPlugins     []*MetricWrapperV1
	ServicePlugins    []*ServiceWrapperV1
	ProcessorPlugins  []*ProcessorWrapperV1
	AggregatorPlugins []*AggregatorWrapperV1
	FlusherPlugins    []*FlusherWrapperV1
	ExtensionPlugins  map[string]pipeline.Extension

	FlushOutStore  *FlushOutStore[protocol.LogGroup]
	LogstoreConfig *LogstoreConfig

	InputControl     *pipeline.AsyncControl
	ProcessControl   *pipeline.AsyncControl
	AggregateControl *pipeline.AsyncControl
	FlushControl     *pipeline.AsyncControl
}

func (p *pluginv1Runner) Init(inputQueueSize int, flushQueueSize int) error {
	p.InputControl = pipeline.NewAsyncControl()
	p.ProcessControl = pipeline.NewAsyncControl()
	p.AggregateControl = pipeline.NewAsyncControl()
	p.FlushControl = pipeline.NewAsyncControl()
	p.MetricPlugins = make([]*MetricWrapperV1, 0)
	p.ServicePlugins = make([]*ServiceWrapperV1, 0)
	p.ProcessorPlugins = make([]*ProcessorWrapperV1, 0)
	p.AggregatorPlugins = make([]*AggregatorWrapperV1, 0)
	p.FlusherPlugins = make([]*FlusherWrapperV1, 0)
	p.ExtensionPlugins = make(map[string]pipeline.Extension, 0)
	p.LogsChan = make(chan *pipeline.LogWithContext, inputQueueSize)
	p.LogGroupsChan = make(chan *protocol.LogGroup, helper.Max(flushQueueSize, p.FlushOutStore.Len()))
	p.FlushOutStore.Write(p.LogGroupsChan)
	return nil
}

func (p *pluginv1Runner) AddDefaultAggregatorIfEmpty() error {
	if len(p.AggregatorPlugins) == 0 {
		pluginMeta := p.LogstoreConfig.genPluginMeta("aggregator_default")
		logger.Debug(p.LogstoreConfig.Context.GetRuntimeContext(), "add default aggregator")
		if err := loadAggregator(pluginMeta, p.LogstoreConfig, nil); err != nil {
			return err
		}
	}
	return nil
}

func (p *pluginv1Runner) AddDefaultFlusherIfEmpty() error {
	if len(p.FlusherPlugins) == 0 {
		logger.Debug(p.LogstoreConfig.Context.GetRuntimeContext(), "add default flusher")
		category, options := flags.GetFlusherConfiguration()
		pluginMeta := p.LogstoreConfig.genPluginMeta(category)
		if err := loadFlusher(pluginMeta, p.LogstoreConfig, options); err != nil {
			return err
		}
	}
	return nil
}

func (p *pluginv1Runner) AddPlugin(pluginMeta *pipeline.PluginMeta, category pluginCategory, plugin interface{}, config map[string]interface{}) error {
	switch category {
	case pluginMetricInput:
		if metric, ok := plugin.(pipeline.MetricInputV1); ok {
			return p.addMetricInput(pluginMeta, metric, config["interval"].(int))
		}
	case pluginServiceInput:
		if service, ok := plugin.(pipeline.ServiceInputV1); ok {
			return p.addServiceInput(pluginMeta, service)
		}
	case pluginProcessor:
		if processor, ok := plugin.(pipeline.ProcessorV1); ok {
			return p.addProcessor(pluginMeta, processor, config["priority"].(int))
		}
	case pluginAggregator:
		if aggregator, ok := plugin.(pipeline.AggregatorV1); ok {
			return p.addAggregator(pluginMeta, aggregator)
		}
	case pluginFlusher:
		if flusher, ok := plugin.(pipeline.FlusherV1); ok {
			return p.addFlusher(pluginMeta, flusher)
		}
	case pluginExtension:
		if extension, ok := plugin.(pipeline.Extension); ok {
			return p.addExtension(pluginMeta.PluginTypeWithID, extension)
		}
	default:
		return pluginCategoryUndefinedError(category)
	}
	return pluginUnImplementError(category, v1, pluginMeta.PluginTypeWithID)
}

func (p *pluginv1Runner) GetExtension(name string) (pipeline.Extension, bool) {
	extension, ok := p.ExtensionPlugins[name]
	return extension, ok
}

func (p *pluginv1Runner) Run() {
	p.runFlusher()
	p.runAggregator()
	p.runProcessor()
	p.runInput()
}

func (p *pluginv1Runner) RunPlugins(category pluginCategory, control *pipeline.AsyncControl) {
	switch category {
	case pluginMetricInput:
		p.runMetricInput(control)
	default:
	}
}

func (p *pluginv1Runner) IsWithInputPlugin() bool {
	return len(p.MetricPlugins) > 0 || len(p.ServicePlugins) > 0
}

func (p *pluginv1Runner) addMetricInput(pluginMeta *pipeline.PluginMeta, input pipeline.MetricInputV1, inputInterval int) error {
	var wrapper MetricWrapperV1
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input

	wrapper.LogsChan = p.LogsChan
	wrapper.LatencyMetric = p.LogstoreConfig.Statistics.CollecLatencytMetric
	p.MetricPlugins = append(p.MetricPlugins, &wrapper)
	return wrapper.Init(pluginMeta, inputInterval)
}

func (p *pluginv1Runner) addServiceInput(pluginMeta *pipeline.PluginMeta, input pipeline.ServiceInputV1) error {
	var wrapper ServiceWrapperV1
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input
	wrapper.LogsChan = p.LogsChan
	p.ServicePlugins = append(p.ServicePlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginv1Runner) addProcessor(pluginMeta *pipeline.PluginMeta, processor pipeline.ProcessorV1, priority int) error {
	var wrapper ProcessorWrapperV1
	wrapper.Config = p.LogstoreConfig
	wrapper.Processor = processor
	wrapper.LogsChan = p.LogsChan
	wrapper.Priority = priority
	p.ProcessorPlugins = append(p.ProcessorPlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginv1Runner) addAggregator(pluginMeta *pipeline.PluginMeta, aggregator pipeline.AggregatorV1) error {
	var wrapper AggregatorWrapperV1
	wrapper.Config = p.LogstoreConfig
	wrapper.Aggregator = aggregator
	wrapper.LogGroupsChan = p.LogGroupsChan
	p.AggregatorPlugins = append(p.AggregatorPlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginv1Runner) addFlusher(pluginMeta *pipeline.PluginMeta, flusher pipeline.FlusherV1) error {
	var wrapper FlusherWrapperV1
	wrapper.Config = p.LogstoreConfig
	wrapper.Flusher = flusher
	wrapper.LogGroupsChan = p.LogGroupsChan
	wrapper.Interval = time.Millisecond * time.Duration(p.LogstoreConfig.GlobalConfig.FlushIntervalMs)
	p.FlusherPlugins = append(p.FlusherPlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginv1Runner) addExtension(name string, extension pipeline.Extension) error {
	p.ExtensionPlugins[name] = extension
	return nil
}

func (p *pluginv1Runner) runInput() {
	p.InputControl.Reset()
	p.runMetricInput(p.InputControl)
	for _, service := range p.ServicePlugins {
		s := service
		p.InputControl.Run(s.Run)
	}
}

func (p *pluginv1Runner) runMetricInput(async *pipeline.AsyncControl) {
	for _, metric := range p.MetricPlugins {
		m := metric
		async.Run(m.Run)
	}
}

func (p *pluginv1Runner) runProcessor() {
	p.ProcessControl.Reset()
	p.ProcessControl.Run(p.runProcessorInternal)
}

// runProcessorInternal is the routine of processors.
// Each LogstoreConfig has its own goroutine for this routine.
// When log is ready (passed through LogsChan), we will try to get
//
//	all available logs from the channel, and pass them together to processors.
//
// All processors of the config share same gogroutine, logs are passed to them
//
//	one by one, just like logs -> p1 -> p2 -> p3 -> logsGoToNextStep.
//
// It returns when processShutdown is closed.
func (p *pluginv1Runner) runProcessorInternal(cc *pipeline.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	var logCtx *pipeline.LogWithContext
	for {
		select {
		case <-cc.CancelToken():
			if len(p.LogsChan) == 0 {
				return
			}
		case logCtx = <-p.LogsChan:
			logs := []*protocol.Log{logCtx.Log}
			p.LogstoreConfig.Statistics.RawLogMetric.Add(int64(len(logs)))
			for _, processor := range p.ProcessorPlugins {
				logs = processor.Process(logs)
				if len(logs) == 0 {
					break
				}
			}
			nowTime := time.Now()

			if len(logs) > 0 {
				p.LogstoreConfig.Statistics.SplitLogMetric.Add(int64(len(logs)))
				for _, aggregator := range p.AggregatorPlugins {
					for _, l := range logs {
						if len(l.Contents) == 0 {
							continue
						}
						if l.Time == uint32(0) {
							protocol.SetLogTime(l, uint32(nowTime.Unix()))
						}
						if !p.LogstoreConfig.GlobalConfig.EnableTimestampNanosecond {
							l.TimeNs = nil
						}
						for tryCount := 1; true; tryCount++ {
							err := aggregator.Aggregator.Add(l, logCtx.Context)
							if err == nil {
								break
							}
							// wait until shutdown is active
							if tryCount%100 == 0 {
								logger.Warning(p.LogstoreConfig.Context.GetRuntimeContext(), "AGGREGATOR_ADD_ALARM", "error", err)
							}
							time.Sleep(time.Millisecond * 10)
						}
					}
				}
			}
		}
	}
}

func (p *pluginv1Runner) runAggregator() {
	p.AggregateControl.Reset()
	for _, aggregator := range p.AggregatorPlugins {
		a := aggregator
		p.AggregateControl.Run(a.Run)
	}
}

func (p *pluginv1Runner) runFlusher() {
	p.FlushControl.Reset()
	p.FlushControl.Run(p.runFlusherInternal)
}

func (p *pluginv1Runner) runFlusherInternal(cc *pipeline.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	var logGroup *protocol.LogGroup
	for {
		select {
		case <-cc.CancelToken():
			if len(p.LogGroupsChan) == 0 {
				return
			}

		case logGroup = <-p.LogGroupsChan:
			if logGroup == nil {
				continue
			}

			listLen := len(p.LogGroupsChan) + 1
			logGroups := make([]*protocol.LogGroup, listLen)
			logGroups[0] = logGroup
			for i := 1; i < listLen; i++ {
				logGroups[i] = <-p.LogGroupsChan
			}
			p.LogstoreConfig.Statistics.FlushLogGroupMetric.Add(int64(len(logGroups)))

			// Add tags for each non-empty LogGroup, includes: default hostname tag,
			// env tags and global tags in config.
			for _, logGroup := range logGroups {
				if len(logGroup.Logs) == 0 {
					continue
				}
				p.LogstoreConfig.Statistics.FlushLogMetric.Add(int64(len(logGroup.Logs)))
				logGroup.Source = util.GetIPAddress()
				for key, value := range loadAdditionalTags(p.LogstoreConfig.GlobalConfig).Iterator() {
					logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{Key: key, Value: value})
				}
			}

			// Flush LogGroups to all flushers.
			// Note: multiple flushers is unrecommended, because all flushers will
			//   be blocked if one of them is unready.
			for {
				allReady := true
				for _, flusher := range p.FlusherPlugins {
					if !flusher.Flusher.IsReady(p.LogstoreConfig.ProjectName,
						p.LogstoreConfig.LogstoreName, p.LogstoreConfig.LogstoreKey) {
						allReady = false
						break
					}
				}
				if allReady {
					for _, flusher := range p.FlusherPlugins {
						p.LogstoreConfig.Statistics.FlushReadyMetric.Add(1)
						begin := time.Now()
						err := flusher.Flush(p.LogstoreConfig.ProjectName,
							p.LogstoreConfig.LogstoreName, p.LogstoreConfig.ConfigName, logGroups)
						p.LogstoreConfig.Statistics.FlushLatencyMetric.Observe(float64(time.Since(begin)))
						if err != nil {
							logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), "FLUSH_DATA_ALARM", "flush data error",
								p.LogstoreConfig.ProjectName, p.LogstoreConfig.LogstoreName, err)
						}
					}
					break
				}
				if !p.LogstoreConfig.FlushOutFlag.Load() {
					time.Sleep(time.Duration(10) * time.Millisecond)
					continue
				}

				// Config is stopping, move unflushed LogGroups to FlushOutLogGroups.
				logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flush loggroup to slice, loggroup count", listLen)
				p.FlushOutStore.Add(logGroups...)
				break
			}
		}
	}
}

func (p *pluginv1Runner) Stop(exit bool) error {
	for _, flusher := range p.FlusherPlugins {
		flusher.Flusher.SetUrgent(exit)
	}
	for _, service := range p.ServicePlugins {
		_ = service.Stop()
	}
	p.InputControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "metric plugins stop", "done", "service plugins stop", "done")

	p.ProcessControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "processor plugins stop", "done")

	p.AggregateControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "aggregator plugins stop", "done")

	p.LogstoreConfig.FlushOutFlag.Store(true)
	p.FlushControl.WaitCancel()

	if exit && p.FlushOutStore.Len() > 0 {
		flushers := make([]pipeline.FlusherV1, len(p.FlusherPlugins))
		for idx, flusher := range p.FlusherPlugins {
			flushers[idx] = flusher.Flusher
		}
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flushout loggroups, count", p.FlushOutStore.Len())
		rst := flushOutStore(p.LogstoreConfig, p.FlushOutStore, p.FlusherPlugins, func(lc *LogstoreConfig, sf *FlusherWrapperV1, store *FlushOutStore[protocol.LogGroup]) error {
			return sf.Flusher.Flush(lc.Context.GetProject(), lc.Context.GetLogstore(), lc.Context.GetConfigName(), store.Get())
		})
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flushout loggroups, result", rst)
	}
	for idx, flusher := range p.FlusherPlugins {
		if err := flusher.Flusher.Stop(); err != nil {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), "STOP_FLUSHER_ALARM",
				"Failed to stop %vth flusher (description: %v): %v",
				idx, flusher.Flusher.Description(), err)
		}
	}
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flusher plugins stop", "done")

	for _, extension := range p.ExtensionPlugins {
		err := extension.Stop()
		if err != nil {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), "STOP_EXTENSION_ALARM",
				"failed to stop extension (description: %v): %v", extension.Description(), err)
		}
	}
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "extension plugins stop", "done")

	return nil
}

func (p *pluginv1Runner) ReceiveRawLog(log *pipeline.LogWithContext) {
	p.LogsChan <- log
}

func (p *pluginv1Runner) ReceiveLogGroup(logGroup pipeline.LogGroupWithContext) {
	topic := logGroup.LogGroup.GetTopic()
	for _, log := range logGroup.LogGroup.Logs {
		if len(topic) > 0 {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: tagKeyLogTopic, Value: topic})
		}
		// When UsingOldContentTag is set to false, the tag is now put into the context during cgo.
		if !p.LogstoreConfig.GlobalConfig.UsingOldContentTag {
			context := map[string]interface{}{}
			for key, value := range logGroup.Context {
				context[key] = value
			}
			context[ctxKeyTopic] = topic
			context[ctxKeyTags] = logGroup.LogGroup.LogTags
			p.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: context})
		} else {
			context := map[string]interface{}{}
			for key, value := range logGroup.Context {
				context[key] = value
			}
			context[ctxKeyTopic] = topic
			for _, tag := range logGroup.LogGroup.LogTags {
				log.Contents = append(log.Contents, &protocol.Log_Content{
					Key:   tagPrefix + tag.GetKey(),
					Value: tag.GetValue(),
				})
			}
			p.ReceiveRawLog(&pipeline.LogWithContext{Log: log, Context: context})
		}
	}
}

func (p *pluginv1Runner) Merge(r PluginRunner) {
	if other, ok := r.(*pluginv1Runner); ok {
		p.FlushOutStore.Merge(other.FlushOutStore)
	}
}
