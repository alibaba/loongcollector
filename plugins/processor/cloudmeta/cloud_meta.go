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

package cloudmeta

import (
	"encoding/json"
	"errors"
	"strconv"
	"strings"
	"time"

	"github.com/cespare/xxhash/v2"

	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorCloudMeta struct {
	Platform       platformmeta.Platform
	Metadata       []string
	RenameMetadata map[string]string
	JSONPath       string
	ReadOnce       bool

	jsonKey      string
	jsonPath     []string
	manager      platformmeta.Manager
	context      pipeline.Context
	meta         map[string]string
	logcontents  []*protocol.Log_Content
	metaHash     uint64
	lastReadTime time.Time
}

func (c *ProcessorCloudMeta) Init(context pipeline.Context) error {
	c.context = context
	c.meta = make(map[string]string)
	m := platformmeta.GetManager(c.Platform)
	if m == nil {
		// don't direct return to support still work on unknown host with auto mode.
		logger.Error(c.context.GetRuntimeContext(), "CLOUD_META_ALARM", "not support platform", c.Platform)
	} else {
		c.manager = m
		c.manager.StartCollect()
	}
	if len(c.Metadata) == 0 {
		logger.Error(c.context.GetRuntimeContext(), "CLOUD_META_ALARM", "metadata is required")
		return errors.New("metadata is required")
	}
	c.JSONPath = strings.TrimSpace(c.JSONPath)
	parts := strings.Split(c.JSONPath, ".")
	if len(parts) > 0 {
		c.jsonKey = parts[0]
		c.jsonPath = parts[1:]
	}
	c.readMeta()
	return nil
}

func (c *ProcessorCloudMeta) Description() string {
	return "add cloud meta for log content or json log content"
}
func (c *ProcessorCloudMeta) ProcessLog(log *protocol.Log) {
	if c.isAppendContent() {
		log.Contents = append(log.Contents, c.logcontents...)
		return
	}
	var content *protocol.Log_Content
	for i := range log.Contents {
		if log.Contents[i].Key == c.jsonKey {
			content = log.Contents[i]
			break
		}
	}
	if content == nil {
		content = &protocol.Log_Content{
			Key:   c.jsonKey,
			Value: "{}",
		}
		log.Contents = append(log.Contents, content)
	}
	res := make(map[string]interface{})
	if err := json.Unmarshal(util.ZeroCopyStringToBytes(content.Value), &res); err != nil {
		logger.Warning(c.context.GetRuntimeContext(), "CLOUD_META_ALARM", "json deserialize err", err)
		return
	}
	// append metadata to json
	c.appendMetadata2Json(res, c.jsonPath)
	output, _ := json.Marshal(res)
	content.Value = string(output)
}

func (c *ProcessorCloudMeta) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	c.readMeta()
	if len(c.meta) != 0 {
		logger.Debugf(c.context.GetRuntimeContext(), "meta: %v", c.meta)
		for _, log := range logArray {
			c.ProcessLog(log)
		}
	}
	return logArray
}

func (c *ProcessorCloudMeta) appendMetadata2Json(res map[string]interface{}, path []string) {
	if len(path) == 0 {
		for k, v := range c.meta {
			res[k] = v
		}
		return
	}
	head := path[0]
	child, ok := res[head]
	if !ok {
		child = map[string]interface{}{}
		res[head] = child
	}
	switch t := child.(type) {
	case map[string]interface{}:
		c.appendMetadata2Json(t, path[1:])
	default:
		logger.Debug(c.context.GetRuntimeContext(), "append destination must be object ")
		return
	}
}

func (c *ProcessorCloudMeta) readMeta() {
	now := time.Now()
	if c.manager == nil || now.Sub(c.lastReadTime).Milliseconds() < 1000 || (c.ReadOnce && !c.lastReadTime.IsZero()) {
		return
	}
	var strBuilder strings.Builder
	newMeta := make(map[string]string)
	wrapperFunc := func(name, val string) {
		strBuilder.WriteString(name)
		strBuilder.WriteString(val)
		newName, ok := c.RenameMetadata[name]
		if ok {
			name = newName
		}
		newMeta[name] = val
	}
	for _, k := range c.Metadata {
		switch k {
		case platformmeta.FlagInstanceID:
			wrapperFunc(platformmeta.FlagInstanceID, c.manager.GetInstanceID())
		case platformmeta.FlagInstanceName:
			wrapperFunc(platformmeta.FlagInstanceName, c.manager.GetInstanceName())
		case platformmeta.FlagInstanceZone:
			wrapperFunc(platformmeta.FlagInstanceZone, c.manager.GetInstanceZone())
		case platformmeta.FlagInstanceRegion:
			wrapperFunc(platformmeta.FlagInstanceRegion, c.manager.GetInstanceRegion())
		case platformmeta.FlagInstanceType:
			wrapperFunc(platformmeta.FlagInstanceType, c.manager.GetInstanceType())
		case platformmeta.FlagInstanceVswitchID:
			wrapperFunc(platformmeta.FlagInstanceVswitchID, c.manager.GetInstanceVswitchID())
		case platformmeta.FlagInstanceVpcID:
			wrapperFunc(platformmeta.FlagInstanceVpcID, c.manager.GetInstanceVpcID())
		case platformmeta.FlagInstanceImageID:
			wrapperFunc(platformmeta.FlagInstanceImageID, c.manager.GetInstanceImageID())
		case platformmeta.FlagInstanceMaxIngress:
			wrapperFunc(platformmeta.FlagInstanceMaxIngress, strconv.FormatInt(c.manager.GetInstanceMaxNetIngress(), 10))
		case platformmeta.FlagInstanceMaxEgress:
			wrapperFunc(platformmeta.FlagInstanceMaxEgress, strconv.FormatInt(c.manager.GetInstanceMaxNetEgress(), 10))
		case platformmeta.FlagInstanceTags:
			newName, ok := c.RenameMetadata[platformmeta.FlagInstanceTags]
			if !ok {
				newName = platformmeta.FlagInstanceTags
			}
			for k, v := range c.manager.GetInstanceTags() {
				newK := newName + "_" + k
				newMeta[newK] = v
				strBuilder.WriteString(newK)
				strBuilder.WriteString(v)
			}
		}
	}
	newHash := xxhash.Sum64(util.ZeroCopyStringToBytes(strBuilder.String()))
	if newHash != c.metaHash {
		c.metaHash = newHash
		c.meta = newMeta
		if c.isAppendContent() {
			c.logcontents = c.logcontents[:0]
			for k, v := range c.meta {
				c.logcontents = append(c.logcontents, &protocol.Log_Content{
					Key:   k,
					Value: v,
				})
			}
		}
	}
	c.lastReadTime = now
}

func (c *ProcessorCloudMeta) isAppendContent() bool {
	return c.jsonKey == ""
}

func init() {
	pipeline.Processors["processor_cloud_meta"] = func() pipeline.Processor {
		return &ProcessorCloudMeta{
			Platform:       platformmeta.Auto,
			RenameMetadata: map[string]string{},
		}
	}
}
