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

package json

import (
	"fmt"

	"github.com/buger/jsonparser"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorJSON struct {
	SourceKey              string
	NoKeyError             bool
	ExpandDepth            int    // 0是不限制，1是当前层
	ExpandConnector        string // 展开时的连接符，可以为空，默认为_
	Prefix                 string // 默认为空，json解析出Key附加的前缀
	KeepSource             bool   // 是否保留源字段
	KeepSourceIfParseError bool
	UseSourceKeyAsPrefix   bool // Should SourceKey be used as prefix for all extracted keys.
	IgnoreFirstConnector   bool // 是否忽略第一个Connector
	ExpandArray            bool // 是否展开数组类型

	context              pipeline.Context
	procParseInSizeBytes pipeline.CounterMetric
}

const pluginType = "processor_json"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorJSON) Init(context pipeline.Context) error {
	if p.SourceKey == "" {
		return fmt.Errorf("must specify SourceKey for plugin %v", pluginType)
	}
	p.context = context
	metricsRecord := p.context.GetMetricRecord()
	p.procParseInSizeBytes = helper.NewCounterMetricAndRegister(metricsRecord, "proc_parse_in_size_bytes")
	return nil
}

func (*ProcessorJSON) Description() string {
	return "json processor for logtail"
}

func (p *ProcessorJSON) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorJSON) processLog(log *protocol.Log) {
	findKey := false
	p.procParseInSizeBytes.Add(int64(log.Size()))
	for idx := range log.Contents {
		if log.Contents[idx].Key == p.SourceKey {
			objectVal := log.Contents[idx].Value
			param := ExpandParam{
				sourceKey:            p.SourceKey,
				log:                  log,
				nowDepth:             0,
				maxDepth:             p.ExpandDepth,
				connector:            p.ExpandConnector,
				prefix:               p.Prefix,
				ignoreFirstConnector: p.IgnoreFirstConnector,
				expandArray:          p.ExpandArray,
			}
			if p.UseSourceKeyAsPrefix {
				param.preKey = p.SourceKey
			}
			err := jsonparser.ObjectEach([]byte(objectVal), param.ExpandJSONCallBack)
			if err != nil {
				logger.Errorf(p.context.GetRuntimeContext(), "PROCESSOR_JSON_PARSER_ALARM", "parser json error %v", err)
			}
			if !p.shouldKeepSource(err) {
				log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
			}
			findKey = true
			break
		}
	}
	if !findKey && p.NoKeyError {
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_JSON_FIND_ALARM", "cannot find key %v", p.SourceKey)
	}
}

func (p *ProcessorJSON) shouldKeepSource(err error) bool {
	return p.KeepSource || (p.KeepSourceIfParseError && err != nil)
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorJSON{
			SourceKey:              "",
			NoKeyError:             true,
			ExpandDepth:            0,
			ExpandConnector:        "_",
			Prefix:                 "",
			KeepSource:             true,
			KeepSourceIfParseError: true,
			UseSourceKeyAsPrefix:   false,
			ExpandArray:            false,
		}
	}
}

type ExpandParam struct {
	sourceKey              string
	log                    *protocol.Log
	contents               models.LogContents
	preKey                 string
	nowDepth               int
	maxDepth               int
	connector              string
	prefix                 string
	ignoreFirstConnector   bool
	isSourceKeyOverwritten bool
	expandArray            bool
}

func (p *ExpandParam) getConnector(depth int) string {
	if depth == 1 && p.ignoreFirstConnector {
		return ""
	}
	return p.connector
}

func (p *ExpandParam) ExpandJSONCallBack(key []byte, value []byte, dataType jsonparser.ValueType, _ int) error {
	p.nowDepth++

	switch dataType {
	case jsonparser.Object:
		p.flattenObject(key, value)
	case jsonparser.Array:
		p.flattenArray(key, value)
	default:
		p.flattenValue(key, value)
	}

	p.nowDepth--
	return nil
}

func (p *ExpandParam) flattenObject(key []byte, value []byte) {
	if p.nowDepth == p.maxDepth {
		// If reach max depth, add it directly to the result
		newKey := p.preKey + p.getConnector(p.nowDepth) + (string)(key)
		p.appendNewContent(newKey, (string)(value))
		return
	}

	backKey := p.preKey
	p.preKey = p.preKey + p.getConnector(p.nowDepth) + (string)(key)
	_ = jsonparser.ObjectEach(value, p.ExpandJSONCallBack)
	p.preKey = backKey
}

func (p *ExpandParam) flattenArray(key []byte, value []byte) {
	defaultKey := p.preKey + p.getConnector(p.nowDepth) + (string)(key)
	if !p.expandArray || p.nowDepth == p.maxDepth {
		// If get value error, or not expand array, or reach max depth, add it directly to the result
		p.appendNewContent(defaultKey, (string)(value))
		return
	}

	index := 0
	_, _ = jsonparser.ArrayEach(value, func(val []byte, dataType jsonparser.ValueType, offset int, err error) {
		newKey := make([]byte, len(key), len(key)+10)
		copy(newKey, key)
		newKey = append(newKey, fmt.Sprintf("[%d]", index)...)
		if dataType == jsonparser.Object {
			p.flattenObject(newKey, val)
		} else {
			p.flattenValue(newKey, val)
		}
		index++
	})
}

func (p *ExpandParam) flattenValue(key []byte, value []byte) {
	// If the current value is not a JSON object, nor a JSON array, add it directly to the result
	newKey := p.preKey + p.getConnector(p.nowDepth) + (string)(key)
	if strValue, err := jsonparser.ParseString(value); err == nil {
		p.appendNewContent(newKey, strValue)
	} else {
		p.appendNewContent(newKey, (string)(value))
	}
}

func (p *ExpandParam) appendNewContent(key string, value string) {
	if p.log != nil {
		p.appendNewContentV1(key, value)
	} else {
		p.appendNewContentV2(key, value)
	}
}

func (p *ExpandParam) appendNewContentV1(key string, value string) {
	if len(p.prefix) > 0 {
		newContent := &protocol.Log_Content{
			Key:   p.prefix + key,
			Value: value,
		}
		p.log.Contents = append(p.log.Contents, newContent)
	} else {
		newContent := &protocol.Log_Content{
			Key:   key,
			Value: value,
		}
		p.log.Contents = append(p.log.Contents, newContent)
	}
}

func (p *ExpandParam) appendNewContentV2(key string, value string) {
	if len(p.prefix) > 0 {
		key = p.prefix + key
	}
	p.contents.Add(key, value)
	if key == p.sourceKey {
		p.isSourceKeyOverwritten = true
	}
}

func (p *ProcessorJSON) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	for _, event := range in.Events {
		p.processEvent(event)
	}
	context.Collector().Collect(in.Group, in.Events...)
}

func (p *ProcessorJSON) processEvent(event models.PipelineEvent) {
	if event.GetType() != models.EventTypeLogging {
		return
	}
	contents := event.(*models.Log).GetIndices()
	if !contents.Contains(p.SourceKey) {
		if p.NoKeyError {
			logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_JSON_FIND_ALARM", "cannot find key %v", p.SourceKey)
		}
		return
	}
	objectVal := contents.Get(p.SourceKey)
	bytesVal, ok := objectVal.([]byte)
	if !ok {
		var stringVal string
		stringVal, ok = objectVal.(string)
		bytesVal = util.ZeroCopyStringToBytes(stringVal)
	}
	if !ok {
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_JSON_FIND_ALARM", "key %v is not string", p.SourceKey)
		return
	}
	param := ExpandParam{
		sourceKey:            p.SourceKey,
		contents:             contents,
		nowDepth:             0,
		maxDepth:             p.ExpandDepth,
		connector:            p.ExpandConnector,
		prefix:               p.Prefix,
		ignoreFirstConnector: p.IgnoreFirstConnector,
		expandArray:          p.ExpandArray,
	}
	if p.UseSourceKeyAsPrefix {
		param.preKey = p.SourceKey
	}
	err := jsonparser.ObjectEach(bytesVal, param.ExpandJSONCallBack)
	if err != nil {
		logger.Errorf(p.context.GetRuntimeContext(), "PROCESSOR_JSON_PARSER_ALARM", "parser json error %v", err)
	}
	if !p.shouldKeepSource(err) && !param.isSourceKeyOverwritten {
		contents.Delete(p.SourceKey)
	}
}
