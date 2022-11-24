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

package desensitize

import (
	"crypto/md5" //nolint:gosec
	"errors"
	"fmt"

	"github.com/dlclark/regexp2"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorDesensitize struct {
	SourceKey    string
	Method       string
	RegexBegin   string
	RegexContent string
	ReplaceAll   bool
	ConstString  string

	context      ilogtail.Context
	regexBegin   *regexp2.Regexp
	regexContent *regexp2.Regexp
}

const pluginName = "processor_desensitize"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorDesensitize) Init(context ilogtail.Context) error {
	p.context = context

	var err error

	// check Method
	if p.Method != "const" && p.Method != "md5" {
		err = errors.New("parameter Method should be \"const\" or \"md5\"")
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}

	// check RegexBegin
	if p.RegexBegin == "" {
		err = errors.New("need parameter RegexBegin")
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}
	p.regexBegin, err = regexp2.Compile(p.RegexBegin, regexp2.RE2)
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}

	// check RegexContent
	if p.RegexContent == "" {
		err = errors.New("need parameter RegexContent")
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}
	p.regexContent, err = regexp2.Compile(p.RegexContent, regexp2.RE2)
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}

	// check Method
	if p.Method == "const" && p.ConstString == "" {
		err = errors.New("parameter ConstString should not be empty when Method is \"const\"")
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init processor_desensitize error", err)
		return err
	}

	return nil
}

func (*ProcessorDesensitize) Description() string {
	return "desensitize processor for logtail"
}

func (p *ProcessorDesensitize) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		for i, content := range log.Contents {
			if p.SourceKey == content.Key {
				newVal := p.desensitize(content.Value)
				newContent := &protocol.Log_Content{
					Key:   content.Key,
					Value: newVal,
				}
				log.Contents[i] = newContent
				break
			}
		}
	}
	return logArray
}

func (p *ProcessorDesensitize) desensitize(val string) string {
	var pos = 0
	match, _ := p.regexBegin.FindStringMatchStartingAt(val, pos)
	for match != nil {
		pos = match.Index + match.Length
		content, _ := p.regexContent.FindStringMatchStartingAt(val, pos)
		if content != nil {
			if p.Method == "const" {
				val, _ = p.regexContent.Replace(val, p.ConstString, pos, 1)
				pos = content.Index + len(p.ConstString)
			}
			if p.Method == "md5" {
				has := md5.Sum([]byte(content.String())) //nolint:gosec
				md5str := fmt.Sprintf("%x", has)
				val, _ = p.regexContent.Replace(val, md5str, pos, 1)
				pos = content.Index + len(md5str)
			}
			if !p.ReplaceAll {
				break
			}
		}
		match, _ = p.regexBegin.FindStringMatchStartingAt(val, pos)
	}
	return val
}

func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return &ProcessorDesensitize{
			SourceKey:    "content",
			Method:       "",
			RegexBegin:   "",
			RegexContent: "",
			ReplaceAll:   true,
			ConstString:  "",
		}
	}
}
