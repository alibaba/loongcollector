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

package anchor

import (
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/buger/jsonparser"
)

const (
	StringType    = 1
	JSONType      = 2
	StringtypeStr = "string"
	JsontypeStr   = "json"
)

// Anchor is used to locate a substring with Start and Stop specified.
// The substring will be inserted into log with FieldName and FieldType.
// ExpondJSON indicates if the substring is JSON type, if this flag is
// set, the substring will be expanded to multiple key/value pairs, and
// values of pairs will be inserted into log with key prepended FieldName
// and ExpondConnector, such as fn_key1, fn_key2.
// ExpondJSON is used to specify depth to expand, 0 means no limit.
//
// Note: Expond is a typo, but because this feature has already published,
// keep this typo for compatibility.
type Anchor struct {
	Start           string
	Stop            string
	FieldName       string
	FieldType       string
	ExpondJSON      bool
	IgnoreJSONError bool
	ExpondConnecter string
	MaxExpondDepth  int

	innerType int
}

// ProcessorAnchor is a processor plugin to process field with anchors.
// Field specified by SourceKey will be processed by all Anchors.
// If no SourceKey is specified, the first field in log contents will be processed.
type ProcessorAnchor struct {
	Anchors       []Anchor
	NoKeyError    bool
	NoAnchorError bool
	SourceKey     string
	KeepSource    bool

	context       pipeline.Context
	logPairMetric pipeline.CounterMetric
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorAnchor) Init(context pipeline.Context) error {
	p.context = context
	for i := range p.Anchors {
		switch p.Anchors[i].FieldType {
		case StringtypeStr:
			p.Anchors[i].innerType = StringType
		case JsontypeStr:
			p.Anchors[i].innerType = JSONType
			if len(p.Anchors[i].ExpondConnecter) == 0 {
				p.Anchors[i].ExpondConnecter = "_"
			}
			// if max expond depth is 1, this is no expond
			if p.Anchors[i].MaxExpondDepth == 1 {
				p.Anchors[i].ExpondJSON = false
			} else if p.Anchors[i].MaxExpondDepth == 0 {
				p.Anchors[i].MaxExpondDepth = 100
			}
		default:
			p.Anchors[i].innerType = StringType
		}
	}

	metricsRecord := p.context.GetMetricRecord()
	p.logPairMetric = helper.NewAverageMetricAndRegister(metricsRecord, "anchor_pairs_per_log")
	return nil
}

func (*ProcessorAnchor) Description() string {
	return "anchor processor for logtail"
}

func (p *ProcessorAnchor) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.ProcessLog(log)
	}
	return logArray
}

func (p *ProcessorAnchor) ProcessLog(log *protocol.Log) {
	beginLen := len(log.Contents)
	findKey := false
	for i, cont := range log.Contents {
		if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
			findKey = true
			if !p.KeepSource {
				log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
			}
			p.ProcessAnchor(log, &cont.Value)
			break
		}
	}
	if !findKey && p.NoKeyError {
		logger.Warning(p.context.GetRuntimeContext(), "ANCHOR_FIND_ALARM", "anchor cannot find key", p.SourceKey)
	}
	p.logPairMetric.Add(int64(len(log.Contents) - beginLen + 1))
}

type ExpondParam struct {
	log       *protocol.Log
	preKey    string
	nowDepth  int
	maxDepth  int
	connector string
}

func (p *ExpondParam) ExpondJSONCallBack(key []byte, value []byte, dataType jsonparser.ValueType, offset int) error {
	p.nowDepth++
	if p.nowDepth == p.maxDepth || dataType != jsonparser.Object {
		if dataType == jsonparser.String {
			// unescape string
			if strValue, err := jsonparser.ParseString(value); err == nil {
				p.log.Contents = append(p.log.Contents, &protocol.Log_Content{
					Key:   p.preKey + p.connector + (string)(key),
					Value: strValue,
				})
			} else {
				p.log.Contents = append(p.log.Contents, &protocol.Log_Content{
					Key:   p.preKey + p.connector + (string)(key),
					Value: (string)(value),
				})
			}

		} else {
			p.log.Contents = append(p.log.Contents, &protocol.Log_Content{
				Key:   p.preKey + p.connector + (string)(key),
				Value: (string)(value),
			})
		}
	} else {
		backKey := p.preKey
		p.preKey = p.preKey + p.connector + (string)(key)
		_ = jsonparser.ObjectEach(value, p.ExpondJSONCallBack)
		p.preKey = backKey
	}
	p.nowDepth--
	return nil
}

func (p *ProcessorAnchor) ProcessAnchor(log *protocol.Log, val *string) {
	for _, anchor := range p.Anchors {
		// Start is "", startIndex is 0
		startIndex := strings.Index(*val, anchor.Start)
		if startIndex < 0 {
			if p.NoAnchorError {
				logger.Warning(p.context.GetRuntimeContext(), "ANCHOR_FIND_ALARM", "anchor no start", anchor.Start, "content", util.CutString(*val, 1024))
			}
			continue
		}
		startIndex += len(anchor.Start)
		stopIndex := strings.Index((*val)[startIndex:], anchor.Stop)
		if stopIndex < 0 {
			if p.NoAnchorError {
				logger.Warning(p.context.GetRuntimeContext(), "ANCHOR_FIND_ALARM", "anchor no stop", anchor.Stop, "content", util.CutString(*val, 1024))
			}
			continue
		} else {
			if len(anchor.Stop) == 0 {
				stopIndex = len(*val)
			} else {
				stopIndex += startIndex
			}
		}

		switch anchor.innerType {
		case StringType:
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: anchor.FieldName, Value: (*val)[startIndex:stopIndex]})
		case JSONType:
			var err error
			if anchor.ExpondJSON {
				param := ExpondParam{
					log:       log,
					preKey:    anchor.FieldName,
					nowDepth:  0,
					maxDepth:  anchor.MaxExpondDepth,
					connector: anchor.ExpondConnecter,
				}
				err = jsonparser.ObjectEach(([]byte)((*val)[startIndex:stopIndex]), param.ExpondJSONCallBack)
			} else {
				err = jsonparser.ObjectEach(([]byte)((*val)[startIndex:stopIndex]), func(key []byte, value []byte, dataType jsonparser.ValueType, offset int) error {
					log.Contents = append(log.Contents, &protocol.Log_Content{
						Key:   anchor.FieldName + anchor.ExpondConnecter + (string)(key),
						Value: (string)(value),
					})
					return nil
				})
			}
			if err != nil && !anchor.IgnoreJSONError {
				logger.Warning(p.context.GetRuntimeContext(), "ANCHOR_JSON_ALARM", "process json error", err, "content", (*val)[startIndex:stopIndex])
			}
		}
	}
}

func init() {
	pipeline.Processors["processor_anchor"] = func() pipeline.Processor {
		return &ProcessorAnchor{}
	}
}
