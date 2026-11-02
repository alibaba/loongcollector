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
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

var (
	tagPrefix     = "__tag__:"
	fileOffsetKey = tagPrefix + "__file_offset__"
	contentKey    = "content"
)

type pluginRunner struct {
	// Event pipeline contexts and lifecycle controls.
	InputPipeContext     pipeline.PipelineContext
	ProcessPipeContext   pipeline.PipelineContext
	AggregatePipeContext pipeline.PipelineContext
	FlushPipeContext     pipeline.PipelineContext
	InputControl         *pipeline.AsyncControl
	ProcessControl       *pipeline.AsyncControl
	AggregateControl     *pipeline.AsyncControl
	FlushControl         *pipeline.AsyncControl

	MetricPlugins     []*MetricInputWrapper
	ServicePlugins    []*ServiceInputWrapper
	ProcessorPlugins  []*ProcessorPluginWrapper
	AggregatorPlugins []*AggregatorPluginWrapper
	FlusherPlugins    []*FlusherPluginWrapper
	ExtensionPlugins  map[string]pipeline.Extension
	TimerRunner       []*timerRunner

	FlushOutStore  *FlushOutStore[models.PipelineGroupEvents]
	LogstoreConfig *LogstoreConfig
}

func (p *pluginRunner) Init(inputQueueSize int, flushQueueSize int) error {
	p.InputControl = pipeline.NewAsyncControl()
	p.ProcessControl = pipeline.NewAsyncControl()
	p.AggregateControl = pipeline.NewAsyncControl()
	p.FlushControl = pipeline.NewAsyncControl()
	p.MetricPlugins = make([]*MetricInputWrapper, 0)
	p.ServicePlugins = make([]*ServiceInputWrapper, 0)
	p.ProcessorPlugins = make([]*ProcessorPluginWrapper, 0)
	p.AggregatorPlugins = make([]*AggregatorPluginWrapper, 0)
	p.FlusherPlugins = make([]*FlusherPluginWrapper, 0)
	p.ExtensionPlugins = make(map[string]pipeline.Extension, 0)
	p.InputPipeContext = helper.NewObservePipelineContext(inputQueueSize)
	p.ProcessPipeContext = helper.NewGroupedPipelineContext()
	// Restored groups must fit before any flusher goroutine starts.
	if pending := p.FlushOutStore.Len(); pending > flushQueueSize {
		flushQueueSize = pending
	}
	p.AggregatePipeContext = helper.NewObservePipelineContext(flushQueueSize)
	p.FlushPipeContext = helper.NewNoopPipelineContext()
	p.FlushOutStore.Write(p.AggregatePipeContext.Collector().Observe())
	return nil
}

func (p *pluginRunner) AddDefaultFlusherIfEmpty() error {
	if len(p.FlusherPlugins) == 0 {
		category, options := flags.GetFlusherConfiguration()
		return loadFlusher(p.LogstoreConfig.genPluginMeta(category), p.LogstoreConfig, options)
	}
	return nil
}

func (p *pluginRunner) AddPlugin(pluginMeta *pipeline.PluginMeta, category pluginCategory, plugin interface{}, config map[string]interface{}) error {
	switch category {
	case pluginMetricInput:
		if metric, ok := plugin.(pipeline.MetricInput); ok {
			return p.addMetricInput(pluginMeta, metric, config["interval"].(int))
		}
	case pluginServiceInput:
		if service, ok := plugin.(pipeline.ServiceInput); ok {
			return p.addServiceInput(pluginMeta, service)
		}
	case pluginProcessor:
		if processor, ok := plugin.(pipeline.ProcessorV2); ok {
			return p.addProcessor(pluginMeta, processor, config["priority"].(int))
		}
	case pluginAggregator:
		if aggregator, ok := plugin.(pipeline.AggregatorV2); ok {
			return p.addAggregator(pluginMeta, aggregator)
		}
	case pluginFlusher:
		if flusher, ok := plugin.(pipeline.FlusherV2); ok {
			return p.addFlusher(pluginMeta, flusher)
		}
	case pluginExtension:
		if extension, ok := plugin.(pipeline.Extension); ok {
			return p.addExtension(pluginMeta.PluginTypeWithID, extension)
		}
	default:
		return pluginCategoryUndefinedError(category)
	}
	return pluginUnImplementError(category, pluginMeta.PluginTypeWithID)
}

func (p *pluginRunner) GetExtension(name string) (pipeline.Extension, bool) {
	extension, ok := p.ExtensionPlugins[name]
	return extension, ok
}

func (p *pluginRunner) Run() {
	p.runFlusher()
	p.runAggregator()
	p.runProcessor()
	p.runInput()
}

func (p *pluginRunner) RunPlugins(category pluginCategory, control *pipeline.AsyncControl) {
	switch category {
	case pluginMetricInput:
		p.runMetricInput(control)
	default:
	}
}

func (p *pluginRunner) IsWithInputPlugin() bool {
	return len(p.MetricPlugins) > 0 || len(p.ServicePlugins) > 0
}

func (p *pluginRunner) addMetricInput(pluginMeta *pipeline.PluginMeta, input pipeline.MetricInput, inputInterval int) error {
	var wrapper MetricInputWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input
	err := wrapper.Init(pluginMeta, inputInterval)
	if err != nil {
		return err
	}
	p.MetricPlugins = append(p.MetricPlugins, &wrapper)
	p.TimerRunner = append(p.TimerRunner, &timerRunner{
		initialMaxDelay: time.Duration(p.LogstoreConfig.GlobalConfig.InputMaxFirstCollectDelayMs) * time.Millisecond,
		interval:        wrapper.Interval,
		state:           &wrapper,
		context:         p.LogstoreConfig.Context,
	})
	return err
}

func (p *pluginRunner) addServiceInput(pluginMeta *pipeline.PluginMeta, input pipeline.ServiceInput) error {
	var wrapper ServiceInputWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Input = input
	p.ServicePlugins = append(p.ServicePlugins, &wrapper)
	err := wrapper.Init(pluginMeta)
	return err
}

func (p *pluginRunner) addProcessor(pluginMeta *pipeline.PluginMeta, processor pipeline.ProcessorV2, _ int) error {
	var wrapper ProcessorPluginWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Processor = processor
	p.ProcessorPlugins = append(p.ProcessorPlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginRunner) addAggregator(pluginMeta *pipeline.PluginMeta, aggregator pipeline.AggregatorV2) error {
	var wrapper AggregatorPluginWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Aggregator = aggregator
	err := wrapper.Init(pluginMeta)
	if err != nil {
		logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.AggregatorInitErrorAlarm, "Aggregator failed to initialize", aggregator.Description(), "error", err)
		return err
	}
	p.AggregatorPlugins = append(p.AggregatorPlugins, &wrapper)
	p.TimerRunner = append(p.TimerRunner, &timerRunner{
		state:           aggregator,
		initialMaxDelay: wrapper.Interval,
		interval:        wrapper.Interval,
		context:         p.LogstoreConfig.Context,
	})
	return nil
}

func (p *pluginRunner) addFlusher(pluginMeta *pipeline.PluginMeta, flusher pipeline.FlusherV2) error {
	var wrapper FlusherPluginWrapper
	wrapper.Config = p.LogstoreConfig
	wrapper.Flusher = flusher
	wrapper.Interval = time.Millisecond * time.Duration(p.LogstoreConfig.GlobalConfig.FlushIntervalMs)
	p.FlusherPlugins = append(p.FlusherPlugins, &wrapper)
	return wrapper.Init(pluginMeta)
}

func (p *pluginRunner) addExtension(name string, extension pipeline.Extension) error {
	p.ExtensionPlugins[name] = extension
	return nil
}

func (p *pluginRunner) runInput() {
	p.InputControl.Reset()
	p.runMetricInput(p.InputControl)
	for _, input := range p.ServicePlugins {
		service := input
		p.InputControl.Run(func(c *pipeline.AsyncControl) {
			logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "start run service", service)
			defer panicRecover(service.Input.Description())
			if err := service.StartService(p.InputPipeContext); err != nil {
				logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.PluginAlarm, "start service error, err", err)
			}
			logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "service done", service.Input.Description())
		})
	}
}

func (p *pluginRunner) runMetricInput(control *pipeline.AsyncControl) {
	for _, t := range p.TimerRunner {
		// Aggregator timers share this list and are handled by runAggregator.
		if plugin, ok := t.state.(*MetricInputWrapper); ok {
			metric := plugin
			timer := t
			control.Run(func(cc *pipeline.AsyncControl) {
				timer.Run(func(state interface{}) error {
					return metric.Read(p.InputPipeContext)
				}, cc)
			})
		}
	}
}

func (p *pluginRunner) runProcessor() {
	p.ProcessControl.Reset()
	p.ProcessControl.Run(p.runProcessorInternal)
}

func (p *pluginRunner) runProcessorInternal(cc *pipeline.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	pipeContext := p.ProcessPipeContext
	pipeChan := p.InputPipeContext.Collector().Observe()
	var processorTag *ProcessorTag
	if globalConfig := p.LogstoreConfig.GlobalConfig; globalConfig.EnableProcessorTag {
		processorTag = NewProcessorTag(globalConfig.PipelineMetaTagKey, globalConfig.AppendingAllEnvMetaTag, globalConfig.AgentEnvMetaTagKey)
	}
	for {
		select {
		case <-cc.CancelToken():
			if len(pipeChan) == 0 {
				return
			}
		case group := <-pipeChan:
			if processorTag != nil {
				processorTag.Process(group)
			}
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
			if len(p.AggregatorPlugins) == 0 {
				// Input groups are already formed. Deliver directly without a
				// default pass-through aggregator or its idle flush timer.
				for _, group := range pipeEvents {
					if len(group.Events) > 0 {
						p.AggregatePipeContext.Collector().CollectList(group)
					}
				}
				continue
			}
			for _, aggregator := range p.AggregatorPlugins {
				for _, pipeEvent := range pipeEvents {
					if len(pipeEvent.Events) == 0 {
						continue
					}
					for tryCount := 1; true; tryCount++ {
						err := aggregator.Record(pipeEvent, p.AggregatePipeContext)
						if err == nil {
							break
						}
						// wait until shutdown is active
						if tryCount%100 == 0 {
							logger.Warning(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.AggregatorAddAlarm, "error", err)
						}
						time.Sleep(time.Millisecond * 10)
					}
				}
			}
		}
	}
}

func (p *pluginRunner) runAggregator() {
	p.AggregateControl.Reset()
	for _, t := range p.TimerRunner {
		if plugin, ok := t.state.(pipeline.AggregatorV2); ok {
			aggregator := plugin
			timer := t
			p.AggregateControl.Run(func(cc *pipeline.AsyncControl) {
				timer.Run(func(state interface{}) error {
					return aggregator.GetResult(p.AggregatePipeContext)
				}, cc)
			})
		}
	}
}

func (p *pluginRunner) runFlusher() {
	p.FlushControl.Reset()
	p.FlushControl.Run(p.runFlusherInternal)
}

func (p *pluginRunner) runFlusherInternal(cc *pipeline.AsyncControl) {
	defer panicRecover(p.LogstoreConfig.ConfigName)
	pipeChan := p.AggregatePipeContext.Collector().Observe()
	for {
		select {
		case <-cc.CancelToken():
			if len(pipeChan) == 0 {
				return
			}

		case event := <-pipeChan:
			if event == nil {
				continue
			}

			dataSize := len(pipeChan) + 1
			data := make([]*models.PipelineGroupEvents, dataSize)
			data[0] = event
			for i := 1; i < dataSize; i++ {
				data[i] = <-pipeChan
			}

			// Flush event groups to all flushers.
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
						err := flusher.Export(data, p.FlushPipeContext)
						if err != nil {
							logger.Error(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.FlushDataAlarm, "flush data error",
								p.LogstoreConfig.ProjectName, p.LogstoreConfig.LogstoreName, err)
						}
					}
					break
				}
				if !p.LogstoreConfig.FlushOutFlag.Load() {
					time.Sleep(time.Duration(10) * time.Millisecond)
					continue
				}

				// Config is stopping; retain unflushed event groups for the next instance.
				logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "retain unflushed event groups, count", dataSize)
				p.FlushOutStore.Add(data...)
				break
			}
		}
	}
}

func (p *pluginRunner) Stop(exit bool) error {
	for _, flusher := range p.FlusherPlugins {
		flusher.Flusher.SetUrgent(exit)
	}
	p.LogstoreConfig.FlushOutFlag.Store(true)

	for _, serviceInput := range p.ServicePlugins {
		_ = serviceInput.Input.Stop()
	}
	p.InputControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "metric plugins stop", "done", "service plugins stop", "done")

	p.ProcessControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "processor plugins stop", "done")

	p.AggregateControl.WaitCancel()
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "aggregator plugins stop", "done")

	p.FlushControl.WaitCancel()

	if exit && p.FlushOutStore.Len() > 0 {
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flushout group events, count", p.FlushOutStore.Len())
		rst := flushOutStore(p.LogstoreConfig, p.FlushOutStore, p.FlusherPlugins, func(lc *LogstoreConfig, pf *FlusherPluginWrapper, store *FlushOutStore[models.PipelineGroupEvents]) error {
			return pf.Export(store.Get(), p.FlushPipeContext)
		})
		logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flushout group events, result", rst)
	}
	for idx, flusher := range p.FlusherPlugins {
		if err := flusher.Flusher.Stop(); err != nil {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.StopFlusherAlarm, "Failed to stop %vth flusher (description: %v): %v",
				idx, flusher.Flusher.Description(), err)
		}
	}
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "Flusher plugins stop", "done")

	for _, extension := range p.ExtensionPlugins {
		err := extension.Stop()
		if err != nil {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.StopExtensionAlarm, "failed to stop extension (description: %v): %v", extension.Description(), err)
		}
	}
	logger.Info(p.LogstoreConfig.Context.GetRuntimeContext(), "extension plugins stop", "done")

	return nil
}

func (p *pluginRunner) ReceivePipelineEventGroup(pbGroup *protocol.PipelineEventGroup, ctx map[string]interface{}) {
	groupEvents, err := helper.TransferPBToPipelineGroupEvents(pbGroup)
	if err != nil {
		logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.ReceiveLogGroupAlarm, "transfer pipeline event group failed, err: %s", err.Error())
		return
	}
	for k, v := range ctx {
		value, ok := v.(string)
		if !ok {
			logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.ReceiveLogGroupAlarm, "unknown values found in context, type is %T", v)
			continue
		}
		groupEvents.Group.Metadata.Add(k, value)
	}
	if p.LogstoreConfig != nil && p.LogstoreConfig.GlobalConfig != nil && p.LogstoreConfig.GlobalConfig.UsingOldContentTag {
		// Preserve the explicit legacy tag-content option in the event model.
		for _, event := range groupEvents.Events {
			if log, ok := event.(*models.Log); ok {
				for key, value := range groupEvents.Group.Tags.Iterator() {
					log.Contents.Add(tagPrefix+key, value)
				}
			}
		}
	}
	p.InputPipeContext.Collector().Collect(groupEvents.Group, groupEvents.Events...)
}

// ReceiveRawLog adapts the public raw-log input API to the event queue.
func (p *pluginRunner) ReceiveRawLog(in *pipeline.LogWithContext) {
	md := models.NewMetadata()
	tags := models.NewTags()
	if in.Context != nil {
		for k, v := range in.Context {
			switch value := v.(type) {
			case string:
				md.Add(k, value)
			case []*protocol.LogTag:
				for _, tag := range value {
					tags.Add(tag.GetKey(), tag.GetValue())
				}
			default:
				logger.Warningf(p.LogstoreConfig.Context.GetRuntimeContext(), selfmonitor.ReceiveRawLogAlarm, "unknown values found in context, type is %T", v)
			}
		}
	}
	log := convertInputLog(in.Log)
	group := models.NewGroup(md, tags)
	p.InputPipeContext.Collector().Collect(group, log)
}

func (p *pluginRunner) Merge(r PluginRunner) {
	if other, ok := r.(*pluginRunner); ok {
		p.FlushOutStore.Merge(other.FlushOutStore)
	}
}
