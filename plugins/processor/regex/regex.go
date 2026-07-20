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

package regex

import (
	"errors"
	"fmt"
	"regexp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ProcessorRegex is a processor plugin to process field with regex.
// It uses Regex to parse the field specified by SourceKey, and insert results with Keys.
// If no SourceKey is specified, the first field in log contents will be parsed.
// Note: use `()` to encase values to extract in Regex.
type ProcessorRegex struct {
	Regex                  string
	Keys                   []string
	FullMatch              bool
	NoKeyError             bool
	NoMatchError           bool
	KeepSource             bool
	KeepSourceIfParseError bool
	SourceKey              string

	context       pipeline.Context
	logPairMetric selfmonitor.CounterMetric
	re            *regexp.Regexp
}

var errNoRegexKey = errors.New("no regex key error")

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorRegex) Init(context pipeline.Context) error {
	p.context = context
	if len(p.Keys) == 0 {
		return errNoRegexKey
	}
	var err error
	// `(?s)` change the meaning of `.` in Golang to match the every character, and the default meaning is not match a newline.
	p.re, err = regexp.Compile("(?s)" + p.Regex)
	if err != nil {
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ProcessorInitAlarm, "init regex error", err, "regex", p.Regex)
		return err
	}

	metricsRecord := p.context.GetMetricRecord()
	p.logPairMetric = selfmonitor.NewAverageMetricAndRegister(metricsRecord, selfmonitor.PluginPairsPerLogTotal)
	return nil
}

func (*ProcessorRegex) Description() string {
	return "regex processor for logtail"
}

func (p *ProcessorRegex) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	if p.re == nil {
		return logArray
	}
	for _, log := range logArray {
		p.ProcessLog(log)
	}
	return logArray
}

func (p *ProcessorRegex) ProcessLog(log *protocol.Log) {
	beginLen := len(log.Contents)
	findKey := false
	for i, cont := range log.Contents {
		if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
			findKey = true
			parseResult := p.processRegex(log, &cont.Value)
			if !p.shouldKeepSource(parseResult) {
				log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
			}
			break
		}
	}
	if !findKey && p.NoKeyError {
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexFindAlarm, "anchor cannot find key", p.SourceKey)
	}
	p.logPairMetric.Add(int64(len(log.Contents) - beginLen + 1))
}

func (p *ProcessorRegex) shouldKeepSource(parseResult bool) bool {
	return p.KeepSource || (p.KeepSourceIfParseError && !parseResult)
}

func (p *ProcessorRegex) processRegex(log *protocol.Log, val *string) bool {
	keys, values, ok := p.matchRegex(*val)
	if !ok {
		return false
	}
	for i := range keys {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: keys[i], Value: values[i]})
	}
	return true
}

// matchRegex applies the compiled regex to val and returns the extracted keys
// and their values (in Keys order) plus whether the parse succeeded. It holds
// the shared match + validation logic reused by both the v1 (ProcessLogs) and
// v2 (Process) code paths, so their semantics stay identical.
func (p *ProcessorRegex) matchRegex(val string) (keys []string, values []string, ok bool) {
	indexArray := p.re.FindStringSubmatchIndex(val)
	if len(indexArray) < 2 || (p.FullMatch && (indexArray[0] != 0 || indexArray[1] != len(val))) {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexUnmatchedAlarm, "unmatch this log content", util.CutString(val, 512))
		}
		return nil, nil, false
	}

	// Use bitwise operations to ignore first two values in indexArray.
	if (len(indexArray)>>1 - 1) < len(p.Keys) {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexUnmatchedAlarm, "match result count less than key count, result count", len(indexArray)>>1-1, "key count", len(p.Keys))
		}
		return nil, nil, false
	}
	keys = make([]string, 0, len(p.Keys))
	values = make([]string, 0, len(p.Keys))
	for i := 0; i < len(p.Keys); i++ {
		leftIndex := indexArray[i<<1+2]
		rightIndex := indexArray[i<<1+3]
		if leftIndex >= 0 && rightIndex >= leftIndex {
			keys = append(keys, p.Keys[i])
			values = append(values, val[leftIndex:rightIndex])
		}
	}
	return keys, values, true
}

func init() {
	pipeline.Processors["processor_regex"] = func() pipeline.Processor {
		return &ProcessorRegex{
			FullMatch:              false,
			NoMatchError:           true,
			KeepSourceIfParseError: true,
		}
	}
}

// Process implements the v2 ProcessorV2 interface: it parses the SourceKey
// value of each Log event with the compiled regex and writes the captured
// groups into the configured Keys, faithfully reproducing the v1 transform
// (KeepSource / KeepSourceIfParseError / NoKeyError / NoMatchError). Metric and
// Span events pass through unchanged.
func (p *ProcessorRegex) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)
}

func (p *ProcessorRegex) processLogEvent(log *models.Log) {
	if p.re == nil {
		return
	}
	contents := log.GetIndices()
	beginLen := contents.Len()
	defer func() {
		p.logPairMetric.Add(int64(contents.Len() - beginLen + 1))
	}()

	// v1 parses the field named by SourceKey, or the first field when SourceKey
	// is not configured.
	sourceKey := p.SourceKey
	if sourceKey == "" {
		for key := range contents.Iterator() {
			sourceKey = key
			break
		}
	}
	if sourceKey == "" || !contents.Contains(sourceKey) {
		if p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexFindAlarm, "anchor cannot find key", p.SourceKey)
		}
		return
	}

	val := fmt.Sprintf("%v", contents.Get(sourceKey))
	keys, values, parseResult := p.matchRegex(val)
	sourceOverwritten := false
	for i := range keys {
		contents.Add(keys[i], values[i])
		if keys[i] == sourceKey {
			sourceOverwritten = true
		}
	}
	if !p.shouldKeepSource(parseResult) && !sourceOverwritten {
		contents.Delete(sourceKey)
	}
}
