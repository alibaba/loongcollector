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

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugin_main/flags"
)

type pluginv1Runner struct {
	// pipeline v1 fields
	LogsChan      chan *ilogtail.LogWithContext
	LogGroupsChan chan *protocol.LogGroup

	MetricPlugins     []*MetricWrapper
	ServicePlugins    []*ServiceWrapper
	ProcessorPlugins  []*ProcessorWrapper
	AggregatorPlugins []*AggregatorWrapper
	FlusherPlugins    []*FlusherWrapper

	FlushOutStore  *FlushOutStore[protocol.LogGroup]
	LogstoreConfig *LogstoreConfig

	InputControl     *ilogtail.AsyncControl
	ProcessControl   *ilogtail.AsyncControl
	AggregateControl *ilogtail.AsyncControl
	FlushControl     *ilogtail.AsyncControl
}

func (p *pluginv1Runner) Init(inputQueueSize int, flushQueueSize int) error {
	p.InputControl = ilogtail.NewAsyncControl()
	p.ProcessControl = ilogtail.NewAsyncControl()
	p.AggregateControl = ilogtail.NewAsyncControl()
	p.FlushControl = ilogtail.NewAsyncControl()
	p.MetricPlugins = make([]*MetricWrapper, 0)
	p.ServicePlugins = make([]*ServiceWrapper, 0)
	p.ProcessorPlugins = make([]*ProcessorWrapper, 0)
	p.AggregatorPlugins = make([]*AggregatorWrapper, 0)
	p.FlusherPlugins = make([]*FlusherWrapper, 0)
	p.LogsChan = make(chan *ilogtail.LogWithContext, inputQueueSize)
	p.LogGroupsChan = make(chan *protocol.LogGroup, helper.Max(flushQueueSize, p.FlushOutStore.Len()))
	p.FlushOutStore.Write(p.LogGroupsChan)
	return nil
}

func (p *pluginv1Runner) Initialized() error {
	if len(p.AggregatorPlugins) == 0 {
		logger.Debug(p.LogstoreConfig.Context.GetRuntimeContext(), "add default aggregator")
		if err := loadAggregator("aggregator_default", p.LogstoreConfig, nil); err != nil {
			return err
		}
	}
	if len(p.FlusherPlugins) == 0 {
		logger.Debug(p.LogstoreConfig.Context.GetRuntimeContext(), "add default flusher")
		category, options := flags.GetFlusherConfiguration()
		if err := loadFlusher(category, p.LogstoreConfig, options); err != nil {
			return err
		}
	}
	return nil
}

func (p *pluginv1Runner) AddPlugin(pluginName string, category pluginCategory, plugin interface{}, config map[string]interface{}) error {
	switch category {
	case pluginMetricInput:
		if metric, ok := plugin.(ilogtail.MetricInputV1); ok {
			return p.addMetricInput(metric, config["interval"].(int))
		}
	case pluginServiceInput:
		if service, ok := plugin.(ilogtail.ServiceInputV1); ok {
			return p.addServiceInput(service)
		}
	case pluginProcessor:
		if processor, ok := plugin.(ilogtail.ProcessorV1); ok {
			return p.addProcessor(processor, config["priority"].(int))
		}
	case pluginAggregator:
		if aggregator, ok := plugin.(ilogtail.AggregatorV1); ok {
			return p.addAggregator(aggregator)
		}
	case pluginFlusher:
		if flusher, ok := plugin.(ilogtail.FlusherV1); ok {
			return p.addFlusher(flusher)
		}
	default:
		return pluginCategoryUndefinedError(category)
	}
	return pluginUnImplementError(category, v1, pluginName)
}

func (p *pluginv1Runner) Run() {
	p.runFlusher()
	p.runAggregator()
	p.runProcessor()
	p.runInput()
}

func (p *pluginv1Runner) RunPlugins(category pluginCategory, control *ilogtail.AsyncControl) {
	switch category {
	case pluginMetricInput:
		p.runMetricInput(control)
	default:
	}
}

func (p *pluginv1Runner) addMetricInput(input ilogtail.MetricInputV1, interval int) error {
	var wrapper MetricWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input
	wrapper.Interval = time.Duration(interval) * time.Millisecond
	wrapper.LogsChan = p.LogsChan
	wrapper.LatencyMetric = p.LogstoreConfig.Statistics.CollecLatencytMetric
	p.MetricPlugins = append(p.MetricPlugins, &wrapper)
	return nil
}

func (p *pluginv1Runner) addServiceInput(input ilogtail.ServiceInputV1) error {
	var wrapper ServiceWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input
	wrapper.LogsChan = p.LogsChan
	p.ServicePlugins = append(p.ServicePlugins, &wrapper)
	return nil
}

func (p *pluginv1Runner) addProcessor(processor ilogtail.ProcessorV1, priority int) error {
	var wrapper ProcessorWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Processor = processor
	wrapper.LogsChan = p.LogsChan
	wrapper.Priority = priority
	p.ProcessorPlugins = append(p.ProcessorPlugins, &wrapper)
	return nil
}

func (p *pluginv1Runner) addAggregator(aggregator ilogtail.AggregatorV1) error {
	var wrapper AggregatorWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Aggregator = aggregator
	wrapper.LogGroupsChan = p.LogGroupsChan
	interval, err := aggregator.Init(p.LogstoreConfig.Context, &wrapper)
	if err != nil {
		logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), "AGGREGATOR_INIT_ERROR", "Aggregator failed to initialize", aggregator.Description(), "error", err)
		return err
	}
	if interval == 0 {
		interval = p.LogstoreConfig.GlobalConfig.AggregatIntervalMs
	}
	wrapper.Interval = time.Millisecond * time.Duration(interval)
	p.AggregatorPlugins = append(p.AggregatorPlugins, &wrapper)
	return nil
}

func (p *pluginv1Runner) addFlusher(flusher ilogtail.FlusherV1) error {
	var wrapper FlusherWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Flusher = flusher
	wrapper.LogGroupsChan = p.LogGroupsChan
	wrapper.Interval = time.Millisecond * time.Duration(p.LogstoreConfig.GlobalConfig.FlushIntervalMs)
	p.FlusherPlugins = append(p.FlusherPlugins, &wrapper)
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

func (p *pluginv1Runner) runMetricInput(async *ilogtail.AsyncControl) {
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
func (p *pluginv1Runner) runProcessorInternal(cc *ilogtail.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	var logCtx *ilogtail.LogWithContext
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
				logs = processor.Processor.ProcessLogs(logs)
				if len(logs) == 0 {
					break
				}
			}
			nowTime := (uint32)(time.Now().Unix())

			if len(logs) > 0 {
				p.LogstoreConfig.Statistics.SplitLogMetric.Add(int64(len(logs)))
				for _, aggregator := range p.AggregatorPlugins {
					for _, l := range logs {
						if len(l.Contents) == 0 {
							continue
						}
						if l.Time == uint32(0) {
							l.Time = nowTime
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

func (p *pluginv1Runner) runFlusherInternal(cc *ilogtail.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	var logGroup *protocol.LogGroup
	for {
		select {
		case <-cc.CancelToken():
			if len(p.LogGroupsChan) == 0 {
				return
			}
		case <-p.LogstoreConfig.pauseChan:
			p.LogstoreConfig.waitForResume()

		case logGroup = <-p.LogGroupsChan:
			if logGroup == nil {
				continue
			}

			// Check pause status if config is still alive, if paused, wait for resume.
			select {
			case <-p.LogstoreConfig.pauseChan:
				p.LogstoreConfig.waitForResume()
			default:
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
						p.LogstoreConfig.Statistics.FlushLatencyMetric.Begin()
						err := flusher.Flusher.Flush(p.LogstoreConfig.ProjectName,
							p.LogstoreConfig.LogstoreName, p.LogstoreConfig.ConfigName, logGroups)
						p.LogstoreConfig.Statistics.FlushLatencyMetric.End()
						if err != nil {
							logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), "FLUSH_DATA_ALARM", "flush data error",
								p.LogstoreConfig.ProjectName, p.LogstoreConfig.LogstoreName, err)
						}
					}
					break
				}
				if !p.LogstoreConfig.FlushOutFlag {
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

	p.LogstoreConfig.FlushOutFlag = true
	p.FlushControl.WaitCancel()

	if exit && p.FlushOutStore.Len() > 0 {
		flushers := make([]ilogtail.FlusherV1, len(p.FlusherPlugins))
		for idx, flusher := range p.FlusherPlugins {
			flushers[idx] = flusher.Flusher
		}
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flushout loggroups, count", p.FlushOutStore.Len())
		rst := flushOutStore(p.LogstoreConfig, p.FlushOutStore, flushers, func(lc *LogstoreConfig, sf ilogtail.FlusherV1, store *FlushOutStore[protocol.LogGroup]) error {
			return sf.Flush(lc.Context.GetProject(), lc.Context.GetLogstore(), lc.Context.GetConfigName(), store.Get())
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

	return nil
}

func (p *pluginv1Runner) ReceiveRawLog(log *ilogtail.LogWithContext) {
	p.LogsChan <- log
}

func (p *pluginv1Runner) Merge(r PluginRunner) {
	if other, ok := r.(*pluginv1Runner); ok {
		p.FlushOutStore.Merge(other.FlushOutStore)
	}
}
