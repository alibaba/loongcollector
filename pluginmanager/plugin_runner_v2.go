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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
)

type pluginv2Runner struct {
	// pipeline v2 fields
	InputPipeContext     ilogtail.PipelineContext
	ProcessPipeContext   ilogtail.PipelineContext
	AggregatePipeContext ilogtail.PipelineContext
	FlushPipeContext     ilogtail.PipelineContext
	InputControl         *ilogtail.AsyncControl
	ProcessControl       *ilogtail.AsyncControl
	AggregateControl     *ilogtail.AsyncControl
	FlushControl         *ilogtail.AsyncControl

	MetricPlugins     []ilogtail.MetricInputV2
	ServicePlugins    []ilogtail.ServiceInputV2
	ProcessorPlugins  []ilogtail.ProcessorV2
	AggregatorPlugins []ilogtail.AggregatorV2
	FlusherPlugins    []ilogtail.FlusherV2
	TimerRunner       []*timerRunner

	FlushOutStore  *FlushOutStore[models.PipelineGroupEvents]
	LogstoreConfig *LogstoreConfig
}

func (p *pluginv2Runner) Init(inputQueueSize int, flushQueueSize int) error {
	p.InputControl = ilogtail.NewAsyncControl()
	p.ProcessControl = ilogtail.NewAsyncControl()
	p.AggregateControl = ilogtail.NewAsyncControl()
	p.FlushControl = ilogtail.NewAsyncControl()
	p.MetricPlugins = make([]ilogtail.MetricInputV2, 0)
	p.ServicePlugins = make([]ilogtail.ServiceInputV2, 0)
	p.ProcessorPlugins = make([]ilogtail.ProcessorV2, 0)
	p.AggregatorPlugins = make([]ilogtail.AggregatorV2, 0)
	p.FlusherPlugins = make([]ilogtail.FlusherV2, 0)
	p.InputPipeContext = ilogtail.NewObservePipelineConext(inputQueueSize)
	p.ProcessPipeContext = ilogtail.NewGroupedPipelineConext()
	p.AggregatePipeContext = ilogtail.NewObservePipelineConext(flushQueueSize)
	p.FlushPipeContext = ilogtail.NewNoopPipelineConext()
	p.FlushOutStore.Write(p.AggregatePipeContext.Collector().Observe())
	return nil
}

func (p *pluginv2Runner) Initialized() error {
	if len(p.AggregatorPlugins) == 0 {
		logger.Debug(p.LogstoreConfig.Context.GetRuntimeContext(), "add default aggregator")
		if err := loadAggregator("aggregator_default", p.LogstoreConfig, nil); err != nil {
			return err
		}
	}
	// TODO Implement default flusher v2
	return nil
}

func (p *pluginv2Runner) AddPlugin(pluginName string, category pluginCategory, plugin interface{}, config map[string]interface{}) error {
	switch category {
	case pluginMetricInput:
		if metric, ok := plugin.(ilogtail.MetricInputV2); ok {
			return p.addMetricInput(metric, config["interval"].(int))
		}
	case pluginServiceInput:
		if service, ok := plugin.(ilogtail.ServiceInputV2); ok {
			return p.addServiceInput(service)
		}
	case pluginProcessor:
		if processor, ok := plugin.(ilogtail.ProcessorV2); ok {
			return p.addProcessor(processor, config["priority"].(int))
		}
	case pluginAggregator:
		if aggregator, ok := plugin.(ilogtail.AggregatorV2); ok {
			return p.addAggregator(aggregator)
		}
	case pluginFlusher:
		if flusher, ok := plugin.(ilogtail.FlusherV2); ok {
			return p.addFlusher(flusher)
		}
	default:
		return pluginCategoryUndefinedError(category)
	}
	return pluginUnImplementError(category, v2, pluginName)
}

func (p *pluginv2Runner) Run() {
	p.runFlusher()
	p.runAggregator()
	p.runProcessor()
	p.runInput()
}

func (p *pluginv2Runner) RunPlugins(category pluginCategory, control *ilogtail.AsyncControl) {
	switch category {
	case pluginMetricInput:
		p.runMetricInput(control)
	default:
	}
}

func (p *pluginv2Runner) addMetricInput(input ilogtail.MetricInputV2, interval int) error {
	p.MetricPlugins = append(p.MetricPlugins, input)
	p.TimerRunner = append(p.TimerRunner, &timerRunner{
		state:         input,
		interval:      time.Duration(interval) * time.Millisecond,
		context:       p.LogstoreConfig.Context,
		latencyMetric: p.LogstoreConfig.Statistics.CollecLatencytMetric,
	})
	return nil
}

func (p *pluginv2Runner) addServiceInput(input ilogtail.ServiceInputV2) error {
	p.ServicePlugins = append(p.ServicePlugins, input)
	return nil
}

func (p *pluginv2Runner) addProcessor(processor ilogtail.ProcessorV2, _ int) error {
	p.ProcessorPlugins = append(p.ProcessorPlugins, processor)
	return nil
}

func (p *pluginv2Runner) addAggregator(aggregator ilogtail.AggregatorV2) error {
	p.AggregatorPlugins = append(p.AggregatorPlugins, aggregator)
	interval, err := aggregator.Init(p.LogstoreConfig.Context, &AggregatorWrapper{})
	if err != nil {
		logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), "AGGREGATOR_INIT_ERROR", "Aggregator failed to initialize", aggregator.Description(), "error", err)
		return err
	}
	if interval == 0 {
		interval = p.LogstoreConfig.GlobalConfig.AggregatIntervalMs
	}
	p.TimerRunner = append(p.TimerRunner, &timerRunner{
		state:         aggregator,
		interval:      time.Millisecond * time.Duration(interval),
		context:       p.LogstoreConfig.Context,
		latencyMetric: p.LogstoreConfig.Statistics.CollecLatencytMetric,
	})
	return nil
}

func (p *pluginv2Runner) addFlusher(flusher ilogtail.FlusherV2) error {
	p.FlusherPlugins = append(p.FlusherPlugins, flusher)
	return nil
}

func (p *pluginv2Runner) runInput() {
	p.InputControl.Reset()
	p.runMetricInput(p.InputControl)
	for _, input := range p.ServicePlugins {
		service := input
		p.InputControl.Run(func(c *ilogtail.AsyncControl) {
			logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "start run service", service)
			defer panicRecover(service.Description())
			if err := service.StartService(p.InputPipeContext); err != nil {
				logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), "PLUGIN_ALARM", "start service error, err", err)
			}
			logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "service done", service.Description())
		})
	}
}

func (p *pluginv2Runner) runMetricInput(control *ilogtail.AsyncControl) {
	for _, t := range p.TimerRunner {
		if plugin, ok := t.state.(ilogtail.MetricInputV2); ok {
			metric := plugin
			timer := t
			control.Run(func(cc *ilogtail.AsyncControl) {
				timer.Run(func(state interface{}) error {
					return metric.Read(p.InputPipeContext)
				}, cc)
			})
		}
	}
}

func (p *pluginv2Runner) runProcessor() {
	p.ProcessControl.Reset()
	p.ProcessControl.Run(p.runProcessorInternal)
}

func (p *pluginv2Runner) runProcessorInternal(cc *ilogtail.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	pipeContext := p.ProcessPipeContext
	pipeChan := p.InputPipeContext.Collector().Observe()
	for {
		select {
		case <-cc.CancelToken():
			if len(pipeChan) == 0 {
				return
			}
		case group := <-pipeChan:
			p.LogstoreConfig.Statistics.RawLogMetric.Add(int64(len(group.Events)))
			pipeEvents := []*models.PipelineGroupEvents{group}
			for _, processor := range p.ProcessorPlugins {
				for _, in := range pipeEvents {
					processor.Process(in, pipeContext)
				}
				pipeEvents = pipeContext.Collector().ToArray()
				if len(pipeEvents) == 0 {
					break
				}
			}
			if len(pipeEvents) == 0 {
				break
			}
			for _, aggregator := range p.AggregatorPlugins {
				for _, pipeEvent := range pipeEvents {
					if len(pipeEvent.Events) == 0 {
						continue
					}
					p.LogstoreConfig.Statistics.SplitLogMetric.Add(int64(len(pipeEvent.Events)))
					for tryCount := 1; true; tryCount++ {
						err := aggregator.Record(pipeEvent, p.AggregatePipeContext)
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

func (p *pluginv2Runner) runAggregator() {
	p.AggregateControl.Reset()
	for _, t := range p.TimerRunner {
		if plugin, ok := t.state.(ilogtail.AggregatorV2); ok {
			aggregator := plugin
			timer := t
			p.AggregateControl.Run(func(cc *ilogtail.AsyncControl) {
				timer.Run(func(state interface{}) error {
					return aggregator.GetResult(p.AggregatePipeContext)
				}, cc)
			})
		}
	}
}

func (p *pluginv2Runner) runFlusher() {
	p.FlushControl.Reset()
	p.FlushControl.Run(p.runFlusherInternal)
}

func (p *pluginv2Runner) runFlusherInternal(cc *ilogtail.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	pipeChan := p.AggregatePipeContext.Collector().Observe()
	for {
		select {
		case <-cc.CancelToken():
			if len(pipeChan) == 0 {
				return
			}
		case <-p.LogstoreConfig.pauseChan:
			p.LogstoreConfig.waitForResume()

		case event := <-pipeChan:
			if event == nil {
				continue
			}

			// Check pause status if config is still alive, if paused, wait for resume.
			select {
			case <-p.LogstoreConfig.pauseChan:
				p.LogstoreConfig.waitForResume()
			default:
			}

			dataSize := len(pipeChan) + 1
			data := make([]*models.PipelineGroupEvents, dataSize)
			data[0] = event
			for i := 1; i < dataSize; i++ {
				data[i] = <-pipeChan
			}
			p.LogstoreConfig.Statistics.FlushLogGroupMetric.Add(int64(len(data)))

			// Add tags for each non-empty LogGroup, includes: default hostname tag,
			// env tags and global tags in config.
			for _, item := range data {
				if len(item.Events) == 0 {
					continue
				}
				p.LogstoreConfig.Statistics.FlushLogMetric.Add(int64(len(item.Events)))
				item.Group.Tags.Merge(loadAdditionalTags(p.LogstoreConfig.GlobalConfig))
			}

			// Flush LogGroups to all flushers.
			// Note: multiple flushers is unrecommended, because all flushers will
			//   be blocked if one of them is unready.
			for {
				allReady := true
				for _, flusher := range p.FlusherPlugins {
					if !flusher.IsReady(p.LogstoreConfig.ProjectName,
						p.LogstoreConfig.LogstoreName, p.LogstoreConfig.LogstoreKey) {
						allReady = false
						break
					}
				}
				if allReady {
					for _, flusher := range p.FlusherPlugins {
						p.LogstoreConfig.Statistics.FlushReadyMetric.Add(1)
						p.LogstoreConfig.Statistics.FlushLatencyMetric.Begin()
						err := flusher.Export(data, p.FlushPipeContext)
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
				logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "flush loggroup to slice, loggroup count", dataSize)
				p.FlushOutStore.Add(data...)
				break
			}
		}
	}
}

func (p *pluginv2Runner) Stop(exit bool) error {
	for _, flusher := range p.FlusherPlugins {
		flusher.SetUrgent(exit)
	}
	for _, serviceInput := range p.ServicePlugins {
		_ = serviceInput.Stop()
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
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flushout group events, count", p.FlushOutStore.Len())
		rst := flushOutStore(p.LogstoreConfig, p.FlushOutStore, p.FlusherPlugins, func(lc *LogstoreConfig, pf ilogtail.FlusherV2, store *FlushOutStore[models.PipelineGroupEvents]) error {
			return pf.Export(store.Get(), p.FlushPipeContext)
		})
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flushout group events, result", rst)
	}
	for idx, flusher := range p.FlusherPlugins {
		if err := flusher.Stop(); err != nil {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), "STOP_FLUSHER_ALARM",
				"Failed to stop %vth flusher (description: %v): %v",
				idx, flusher.Description(), err)
		}
	}
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flusher plugins stop", "done")
	return nil
}

func (p *pluginv2Runner) ReceiveRawLog(log *ilogtail.LogWithContext) {
	// TODO
}

func (p *pluginv2Runner) Merge(r PluginRunner) {
	if other, ok := r.(*pluginv2Runner); ok {
		p.FlushOutStore.Merge(other.FlushOutStore)
	}
}
