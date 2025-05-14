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

package addfieldsv2

import (
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/google/uuid"
)

// ProcessorAddFields struct implement the Processor interface.
// The plugin would append the field to the input data.
type ProcessorAddFields struct {
	Fields        map[string]interface{} // the appending fields
	IgnoreIfExist bool                   // Whether to ignore when the same key exists
	innerFuncMap  map[string]func(params string) string
	context       pipeline.Context
}

var (
	fieldRefRegex = regexp.MustCompile(`\$\{([^}]+)\}`)
	funcRegex     = regexp.MustCompile(`(\w+)\((.*?)\)`)
)

const (
	pluginType      = "processor_add_fields_v2"
	uuidFunc        = "uuid"
	envFunc         = "env"
	timestampMsFunc = "timestamp_ms"
	timestampNsFunc = "timestamp_ns"
)

// Init method would be triggered before working for init some system resources,
// like socket, mutex. In this plugin, it verifies Fields must not be empty.
func (p *ProcessorAddFields) Init(context pipeline.Context) error {
	if len(p.Fields) == 0 {
		return fmt.Errorf("must specify Fields for plugin %v", pluginType)
	}
	p.context = context

	p.innerFuncMap = map[string]func(param string) string{
		uuidFunc:        p.generateUUID,
		envFunc:         p.getEnvValue,
		timestampMsFunc: p.generateTimestampMS,
		timestampNsFunc: p.generateTimestampNS,
	}

	return nil
}

// Description ...
func (*ProcessorAddFields) Description() string {
	return "add fields v2 processor for ilogtail"
}

// ProcessLogs append Fields to each log.
func (p *ProcessorAddFields) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorAddFields) processLog(log *protocol.Log) {
	if p.IgnoreIfExist && len(p.Fields) > 1 {
		dict := make(map[string]bool)
		for idx := range log.Contents {
			dict[log.Contents[idx].Key] = true
		}
		for k, v := range p.Fields {
			if _, exists := dict[k]; exists {
				continue
			}
			newContent := &protocol.Log_Content{
				Key:   k,
				Value: p.processValue(v),
			}
			log.Contents = append(log.Contents, newContent)
		}
	} else {
		for k, v := range p.Fields {
			if p.IgnoreIfExist && p.isExist(log, k) {
				continue
			}
			newContent := &protocol.Log_Content{
				Key:   k,
				Value: p.processValue(v),
			}
			log.Contents = append(log.Contents, newContent)
		}
	}
}

func (p *ProcessorAddFields) processValue(value interface{}) string {
	switch v := value.(type) {
	case string:
		return p.processInnerFunc(v)
	case []string, map[string]interface{}:
		if b, err := json.Marshal(v); err == nil {
			return string(b)
		}
		return ""
	default:
		return fmt.Sprintf("%v", v)
	}
}

func (p *ProcessorAddFields) isExist(log *protocol.Log, key string) bool {
	for idx := range log.Contents {
		if log.Contents[idx].Key == key {
			return true
		}
	}
	return false
}

func (p *ProcessorAddFields) processInnerFunc(value string) string {
	if !strings.HasPrefix(value, "${") {
		return value
	}

	return fieldRefRegex.ReplaceAllStringFunc(value, func(match string) string {
		parts := fieldRefRegex.FindStringSubmatch(match)
		if len(parts) < 2 {
			return match
		}

		// call func
		if innerFunc := funcRegex.FindStringSubmatch(parts[1]); len(innerFunc) > 1 {
			if fn, exists := p.innerFuncMap[innerFunc[1]]; exists {
				return fmt.Sprintf("%v", fn(innerFunc[2]))
			}

			return match
		}

		return match
	})
}

// inner func
func (p *ProcessorAddFields) generateUUID(_ string) string {
	return uuid.New().String()
}

func (p *ProcessorAddFields) getEnvValue(param string) string {
	return os.Getenv(param)
}

func (p *ProcessorAddFields) generateTimestampMS(_ string) string {
	return fmt.Sprintf("%d", time.Now().UnixNano()/1e6)
}

func (p *ProcessorAddFields) generateTimestampNS(_ string) string {
	return fmt.Sprintf("%d", time.Now().UnixNano())
}

// Register the plugin to the Processors array.
func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorAddFields{
			Fields:        nil,
			IgnoreIfExist: false,
		}
	}
}
