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
	"errors"
	"strconv"
	"strings"
	"time"

	"github.com/cespare/xxhash/v2"

	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

type ProcessorCloudMeta struct {
	Platform       platformmeta.Platform
	Metadata       []string
	RenameMetadata map[string]string
	JSONPath       string // Retained for configuration compatibility.
	ReadOnce       bool

	manager      platformmeta.Manager
	context      pipeline.Context
	meta         map[string]string
	metaHash     uint64
	lastReadTime time.Time
}

func (c *ProcessorCloudMeta) Init(context pipeline.Context) error {
	c.context = context
	c.meta = make(map[string]string)
	m := platformmeta.GetManager(c.Platform)
	if m == nil {
		// don't direct return to support still work on unknown host with auto mode.
		logger.Warning(c.context.GetRuntimeContext(), selfmonitor.CloudMetaAlarm, "not support platform", c.Platform)
	} else {
		c.manager = m
		c.manager.StartCollect()
	}
	if len(c.Metadata) == 0 {
		logger.Warning(c.context.GetRuntimeContext(), selfmonitor.CloudMetaAlarm, "metadata is required")
		return errors.New("metadata is required")
	}
	c.readMeta()
	return nil
}

func (c *ProcessorCloudMeta) Description() string {
	return "add cloud metadata to event group tags"
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
	}
	c.lastReadTime = now
}

func init() {
	pipeline.Processors["processor_cloud_meta"] = func() pipeline.Processor {
		return &ProcessorCloudMeta{
			Platform:       platformmeta.Auto,
			RenameMetadata: map[string]string{},
		}
	}
}

// Process adds cloud metadata to the group tags and passes all events through.
func (c *ProcessorCloudMeta) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	c.readMeta()
	if len(c.meta) != 0 {
		logger.Debugf(c.context.GetRuntimeContext(), "meta: %v", c.meta)
		// Ensure the group carries a writable Tags before adding cloud metadata.
		// An input may emit Group == nil, Group.Tags == nil, or Group.Tags set to
		// the shared immutable NilStringValues sentinel (e.g. a bare
		// &models.GroupInfo{} or a ByteArray event). Adding to the sentinel is a
		// silent no-op, so in all these cases install a fresh mutable Tags.
		if in.Group == nil {
			in.Group = &models.GroupInfo{}
		}
		if in.Group.Tags == nil || in.Group.Tags.IsNil() {
			in.Group.Tags = models.NewTags()
		}
		tags := in.Group.Tags
		for k, v := range c.meta {
			tags.Add(k, v)
		}
	}
	pipeline.CollectGroupEvents(context, in)
}
