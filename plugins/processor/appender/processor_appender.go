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

package appender

import (
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorAppender struct {
	Key        string
	Value      string
	SortLabels bool
	Platform   platformmeta.Platform
	ReadOnce   bool

	cloudmetaManager platformmeta.Manager
	realValue        string
	replaceFuncs     []func(val string) string
	context          pipeline.Context
}

const pluginType = "processor_appender"

var replaceReg = regexp.MustCompile(`{{[^}]+}}`)

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorAppender) Init(context pipeline.Context) error {
	if len(p.Key) == 0 || len(p.Value) == 0 {
		return fmt.Errorf("must specify Key and Value for plugin %v", pluginType)
	}
	p.context = context
	manager := platformmeta.GetManager(p.Platform)
	if manager != nil {
		p.cloudmetaManager = manager
		p.cloudmetaManager.StartCollect()
	}
	p.realValue = replaceReg.ReplaceAllStringFunc(p.Value, func(s string) string {
		return p.ParseVariableValue(s[2 : len(s)-2])
	})
	return nil
}

func (*ProcessorAppender) Description() string {
	return "processor to append some value for specific key"
}

func (p *ProcessorAppender) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorAppender) processLog(log *protocol.Log) {
	c := p.find(log, p.Key)
	if c == nil {
		c = &protocol.Log_Content{
			Key: p.Key,
		}
		log.Contents = append(log.Contents, c)
	}
	p.processField(c)
}

func (p *ProcessorAppender) processField(c *protocol.Log_Content) {
	r := p.realValue
	for _, replaceFunc := range p.replaceFuncs {
		r = replaceFunc(r)
	}
	c.Value += r
	if p.SortLabels {
		labels := strings.Split(c.Value, "|")
		var keyValue helper.MetricLabels
		for _, labelStr := range labels {
			kv := strings.SplitN(labelStr, "#$#", 2)
			if len(kv) == 2 {
				keyValue.Append(kv[0], kv[1])
			}
		}
		if keyValue.Len() > 0 {
			c.Value = keyValue.String()
		}
	}
}

func (p *ProcessorAppender) find(log *protocol.Log, key string) *protocol.Log_Content {
	for idx := range log.Contents {
		if log.Contents[idx].Key == key {
			return log.Contents[idx]
		}
	}
	return nil
}

// ParseVariableValue parse specific key with logic:
//  1. if key start with '$', the get from env
//  2. if key == __ip__, return local ip address
//  3. if key == __host__, return hostName
//     others return key
func (p *ProcessorAppender) ParseVariableValue(key string) string {
	if len(key) == 0 {
		return key
	}
	if key[0] == '$' {
		return os.Getenv(key[1:])
	}
	switch key {
	case "__ip__":
		return util.GetIPAddress()
	case "__host__":
		return util.GetHostName()
	}
	if p.cloudmetaManager == nil {
		return key
	}
	switch key {
	case platformmeta.FlagInstanceID:
		return p.cloudmetaManager.GetInstanceID()
	case platformmeta.FlagInstanceName:
		if p.ReadOnce {
			return p.cloudmetaManager.GetInstanceName()
		}
		p.replaceFuncs = append(p.replaceFuncs, func(val string) string {
			return strings.ReplaceAll(val, platformmeta.FlagInstanceNameWrapper, p.cloudmetaManager.GetInstanceName())
		})
		return platformmeta.FlagInstanceNameWrapper
	case platformmeta.FlagInstanceImageID:
		return p.cloudmetaManager.GetInstanceImageID()
	case platformmeta.FlagInstanceRegion:
		return p.cloudmetaManager.GetInstanceRegion()
	case platformmeta.FlagInstanceType:
		return p.cloudmetaManager.GetInstanceType()
	case platformmeta.FlagInstanceZone:
		return p.cloudmetaManager.GetInstanceZone()
	case platformmeta.FlagInstanceVpcID:
		if p.ReadOnce {
			return p.cloudmetaManager.GetInstanceVpcID()
		}
		p.replaceFuncs = append(p.replaceFuncs, func(val string) string {
			return strings.ReplaceAll(val, platformmeta.FlagInstanceVpcIDWrapper, p.cloudmetaManager.GetInstanceVpcID())
		})
		return platformmeta.FlagInstanceVpcIDWrapper
	case platformmeta.FlagInstanceMaxEgress:
		if p.ReadOnce {
			return strconv.FormatInt(p.cloudmetaManager.GetInstanceMaxNetEgress(), 10)
		}
		p.replaceFuncs = append(p.replaceFuncs, func(val string) string {
			return strings.ReplaceAll(val, platformmeta.FlagInstanceMaxEgressWrapper, strconv.FormatInt(p.cloudmetaManager.GetInstanceMaxNetEgress(), 10))
		})
		return platformmeta.FlagInstanceMaxEgressWrapper
	case platformmeta.FlagInstanceMaxIngress:
		if p.ReadOnce {
			return strconv.FormatInt(p.cloudmetaManager.GetInstanceMaxNetIngress(), 10)
		}
		p.replaceFuncs = append(p.replaceFuncs, func(val string) string {
			return strings.ReplaceAll(val, platformmeta.FlagInstanceMaxIngressWrapper, strconv.FormatInt(p.cloudmetaManager.GetInstanceMaxNetIngress(), 10))
		})
		return platformmeta.FlagInstanceMaxIngressWrapper
	case platformmeta.FlagInstanceVswitchID:
		if p.ReadOnce {
			return p.cloudmetaManager.GetInstanceVswitchID()
		}
		p.replaceFuncs = append(p.replaceFuncs, func(val string) string {
			return strings.ReplaceAll(val, platformmeta.FlagInstanceVswitchIDWrapper, p.cloudmetaManager.GetInstanceVswitchID())
		})
		return platformmeta.FlagInstanceVswitchIDWrapper
	}
	return key
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorAppender{
			Platform: platformmeta.Mock,
		}
	}
}
