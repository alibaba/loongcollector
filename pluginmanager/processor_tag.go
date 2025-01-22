// Copyright 2024 iLogtail Authors
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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type TagKey int

const (
	TagKeyHostName TagKey = iota
	TagKeyHostIP
	TagKeyHostID
	TagKeyCloudProvider
)

const (
	hostNameDefaultTagKey      = "__hostname__"
	hostIPDefaultTagKey        = "__host_ip__"
	hostIDDefaultTagKey        = "__host_id__"
	cloudProviderDefaultTagKey = "__cloud_provider__"
	defaultConfigTagKeyValue   = "__default__"
)

// Processor interface cannot meet the requirements of tag processing, so we need to create a special ProcessorTag struct
type ProcessorTag struct {
	pipelineMetaTagKey     map[TagKey]string
	appendingAllEnvMetaTag bool
	agentEnvMetaTagKey     map[string]string

	// TODO: file tags, read in background with double buffer
	fileTagsPath string
}

func NewProcessorTag(pipelineMetaTagKey map[string]string, appendingAllEnvMetaTag bool, agentEnvMetaTagKey map[string]string, fileTagsPath string) *ProcessorTag {
	processorTag := &ProcessorTag{
		pipelineMetaTagKey:     make(map[TagKey]string),
		appendingAllEnvMetaTag: appendingAllEnvMetaTag,
		agentEnvMetaTagKey:     agentEnvMetaTagKey,
		fileTagsPath:           fileTagsPath,
	}
	processorTag.parseAllConfigurableTags(pipelineMetaTagKey)
	return processorTag
}

func (p *ProcessorTag) ProcessV1(logCtx *pipeline.LogWithContext) {
	tagsMap := make(map[string]string)
	if logCtx.Context == nil {
		logCtx.Context = make(map[string]interface{})
	}
	if tags, ok := logCtx.Context["tags"]; ok {
		tagsArray, ok := tags.([]*protocol.LogTag)
		if !ok {
			return
		}
		for _, tag := range tagsArray {
			tagsMap[tag.Key] = tag.Value
		}
	}
	p.addAllConfigurableTags(tagsMap)
	// TODO: file tags, read in background with double buffer
	for i := 0; i < len(helper.EnvTags); i += 2 {
		if len(p.agentEnvMetaTagKey) == 0 && p.appendingAllEnvMetaTag {
			tagsMap[helper.EnvTags[i]] = helper.EnvTags[i+1]
		} else {
			if customKey, ok := p.agentEnvMetaTagKey[helper.EnvTags[i]]; ok {
				if customKey != "" {
					tagsMap[customKey] = helper.EnvTags[i+1]
				}
			}
		}
	}
	newTags := make([]*protocol.LogTag, len(tagsMap))
	i := 0
	for key, value := range tagsMap {
		newTags[i] = &protocol.LogTag{Key: key, Value: value}
		i++
	}
	logCtx.Context["tags"] = newTags
}

func (p *ProcessorTag) ProcessV2(in *models.PipelineGroupEvents) {
	tagsMap := make(map[string]string)
	p.addAllConfigurableTags(tagsMap)
	for k, v := range tagsMap {
		in.Group.Tags.Add(k, v)
	}

	// env tags
	for i := 0; i < len(helper.EnvTags); i += 2 {
		if len(p.agentEnvMetaTagKey) == 0 && p.appendingAllEnvMetaTag {
			in.Group.Tags.Add(helper.EnvTags[i], helper.EnvTags[i+1])
		} else {
			if customKey, ok := p.agentEnvMetaTagKey[helper.EnvTags[i]]; ok {
				if customKey != "" {
					in.Group.Tags.Add(customKey, helper.EnvTags[i+1])
				}
			}
		}
	}
}

func (p *ProcessorTag) parseDefaultAddedTag(configKey string, tagKey TagKey, defaultKey string, config map[string]string) {
	if customKey, ok := config[configKey]; ok {
		if customKey != "" {
			if customKey == defaultConfigTagKeyValue {
				p.pipelineMetaTagKey[tagKey] = defaultKey
			} else {
				p.pipelineMetaTagKey[tagKey] = customKey
			}
		}
		// empty value means delete
	} else {
		p.pipelineMetaTagKey[tagKey] = defaultKey
	}
}

func (p *ProcessorTag) parseOptionalTag(configKey string, tagKey TagKey, defaultKey string, config map[string]string) {
	if customKey, ok := config[configKey]; ok {
		if customKey != "" {
			if customKey == defaultConfigTagKeyValue {
				p.pipelineMetaTagKey[tagKey] = defaultKey
			} else {
				p.pipelineMetaTagKey[tagKey] = customKey
			}
		}
		// empty value means delete
	}
}

func (p *ProcessorTag) addTag(tagKey TagKey, value string, tags map[string]string) {
	if key, ok := p.pipelineMetaTagKey[tagKey]; ok {
		if key != "" {
			tags[key] = value
		}
	}
}
