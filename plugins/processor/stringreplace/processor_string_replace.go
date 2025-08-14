// Copyright 2023 iLogtail Authors
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

package stringreplace

import (
	"errors"
	"strconv"
	"strings"
	"time"

	"github.com/dlclark/regexp2"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	PluginName = "processor_string_replace"

	MethodRegex   = "regex"
	MethodConst   = "const"
	MethodUnquote = "unquote"
	// Defaults
	defaultRegexTimeoutMs = 100
)

type ProcessorStringReplace struct {
	SourceKey     string // Target field; required
	Method        string // One of: "regex", "const", "unquote"; required
	Match         string // When Method=="const": substring to replace; when Method=="regex": regex pattern (RE2 syntax)
	ReplaceString string // Replacement string used by "regex" and "const" methods
	DestKey       string // Optional destination field; if empty, replaces in place

	// Safety controls for regex processing
	RegexTimeoutMs int // Per-regex match timeout in milliseconds; default is 100

	re            *regexp2.Regexp
	context       pipeline.Context
	logPairMetric selfmonitor.CounterMetric
}

var errNoMethod = errors.New("no method error")
var errNoMatch = errors.New("no match error")
var errNoSourceKey = errors.New("no source key error")

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorStringReplace) Init(context pipeline.Context) error {
	p.context = context
	if len(p.SourceKey) == 0 {
		return errNoSourceKey
	}
	var err error
	// default safety settings
	if p.RegexTimeoutMs <= 0 {
		p.RegexTimeoutMs = defaultRegexTimeoutMs
	}
	switch p.Method {
	case MethodConst:
		if len(p.Match) == 0 {
			return errNoMatch
		}
	case MethodRegex:
		p.re, err = regexp2.Compile(p.Match, regexp2.RE2)
		if err != nil {
			logger.Warning(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init regex error", err, "regex", p.Match)
			return err
		}
		p.re.MatchTimeout = time.Duration(p.RegexTimeoutMs) * time.Millisecond
		// warn about zero-width regex which may cause performance issues
		if ok, _ := p.re.MatchString(""); ok {
			logger.Warning(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "regex pattern is zero-width (matching empty string), may cause performance issues", "regex", p.Match)
		}
	case MethodUnquote:
	default:
		return errNoMethod
	}

	metricsRecord := p.context.GetMetricRecord()
	p.logPairMetric = selfmonitor.NewAverageMetricAndRegister(metricsRecord, selfmonitor.PluginPairsPerLogTotal)
	return nil
}

func (*ProcessorStringReplace) Description() string {
	return "regex replace processor for logtail"
}

func (p *ProcessorStringReplace) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	replaceCount := 0
	for _, log := range logArray {
		for _, cont := range log.Contents {
			if p.SourceKey != cont.Key {
				continue
			}
			var newContVal string
			var err error
			switch p.Method {
			case MethodConst:
				newContVal = strings.ReplaceAll(cont.Value, p.Match, p.ReplaceString)
			case MethodRegex:
				// directly replace with unlimited count (guarded by regex MatchTimeout); startAt=0
				newContVal, err = p.re.Replace(cont.Value, p.ReplaceString, 0, -1)
			case MethodUnquote:
				if strings.HasPrefix(cont.Value, "\"") && strings.HasSuffix(cont.Value, "\"") {
					newContVal, err = strconv.Unquote(cont.Value)
				} else {
					newContVal, err = strconv.Unquote("\"" + strings.ReplaceAll(cont.Value, "\"", "\\x22") + "\"")
				}
			default:
				newContVal = cont.Value
			}
			if err != nil {
				logger.Warning(p.context.GetRuntimeContext(), "PROCESSOR_STRING_REPLACE_ALARM", "process log error", err)
				newContVal = cont.Value
			}
			if len(p.DestKey) > 0 {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: p.DestKey, Value: newContVal})
			} else {
				cont.Value = newContVal
			}
			replaceCount++
		}
	}
	p.logPairMetric.Add(int64(replaceCount))
	return logArray
}

func init() {
	pipeline.Processors[PluginName] = func() pipeline.Processor {
		return &ProcessorStringReplace{}
	}
}
