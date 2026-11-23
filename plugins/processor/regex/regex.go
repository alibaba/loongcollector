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
	"regexp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
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

func (p *ProcessorRegex) shouldKeepSource(parseResult bool) bool {
	return p.KeepSource || (p.KeepSourceIfParseError && !parseResult)
}

// configuredCaptures returns the offsets for the configured captures or nil on failure.
func (p *ProcessorRegex) configuredCaptures(indices []int, size int, preview func() string) []int {
	if len(indices) < 2 || (p.FullMatch && (indices[0] != 0 || indices[1] != size)) {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexUnmatchedAlarm, "unmatch this log content", preview())
		}
		return nil
	}

	const matchOffsets = 2
	captureCount := (len(indices) - matchOffsets) / 2
	if captureCount < len(p.Keys) {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.RegexUnmatchedAlarm, "match result count less than key count, result count", captureCount, "key count", len(p.Keys))
		}
		return nil
	}
	return indices[matchOffsets : matchOffsets+2*len(p.Keys)]
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

// Process writes regex captures into each log. Other event types pass through unchanged.
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

	// Parse the field named by SourceKey, or the first field when SourceKey
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

	value := contents.Get(sourceKey)
	var val string
	var captures []int
	if body, ok := value.([]byte); ok {
		captures = p.configuredCaptures(p.re.FindSubmatchIndex(body), len(body), func() string {
			return string(body[:min(len(body), 512)])
		})
		val = copyCapturedText(body, captures)
	} else {
		val = pipeline.GetStringValue(value)
		captures = p.configuredCaptures(p.re.FindStringSubmatchIndex(val), len(val), func() string {
			return util.CutString(val, 512)
		})
	}
	sourceOverwritten := false
	for i := 0; i < len(captures); i += 2 {
		start, end := captures[i], captures[i+1]
		if start < 0 || end < start {
			continue
		}
		key := p.Keys[i/2]
		contents.Add(key, val[start:end])
		if key == sourceKey {
			sourceOverwritten = true
		}
	}
	if !p.shouldKeepSource(captures != nil) && !sourceOverwritten {
		contents.Delete(sourceKey)
	}
}

// copyCapturedText gives byte-backed logs one owned string for the matched
// fields. Capture offsets are rebased to the compacted string.
func copyCapturedText(body []byte, captures []int) string {
	size := 0
	for i := 0; i < len(captures); i += 2 {
		start, end := captures[i], captures[i+1]
		if start >= 0 && end >= start {
			// Nested captures can overlap. Copy the original body if that
			// would use less storage and preserve the original offsets.
			if end-start >= len(body)-size {
				return string(body)
			}
			size += end - start
		}
	}
	text := make([]byte, size)
	offset := 0
	for i := 0; i < len(captures); i += 2 {
		start, end := captures[i], captures[i+1]
		if start < 0 || end < start {
			continue
		}
		captures[i] = offset
		offset += copy(text[offset:], body[start:end])
		captures[i+1] = offset
	}
	// This allocation is owned by the capture strings and never mutated.
	return util.ZeroCopyBytesToString(text)
}
