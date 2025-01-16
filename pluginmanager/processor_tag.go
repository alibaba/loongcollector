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

//go:build !enterprise

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

type TagKey int

const (
	TagKeyHostName TagKey = iota
	TagKeyHostIP
	TagKeyHostID
	TagKeyCloudProvider
)

const (
	hostNameDefaultTagKey      = "host_name"
	hostIPDefaultTagKey        = "host_ip"
	hostIDDefaultTagKey        = "host_id"
	cloudProviderDefaultTagKey = "cloud_provider"
	machineUUIDDefaultTagKey   = "__machine_uuid__"
	defaultConfigTagKeyValue   = "__default__"
)

// Processor interface cannot meet the requirements of tag processing, so we need to create a special ProcessorTag struct
type ProcessorTag struct {
	PipelineMetaTagKey     map[TagKey]string
	AppendingAllEnvMetaTag bool
	AgentEnvMetaTagKey     map[string]string

	LogFileTagsPath string
	MachineUUID     string
}

func NewProcessorTag(pipelineMetaTagKey map[string]string, appendingAllEnvMetaTag bool, agentEnvMetaTagKey map[string]string, logFileTagsPath, machineUUID string) *ProcessorTag {
	processorTag := &ProcessorTag{
		PipelineMetaTagKey:     make(map[TagKey]string),
		AppendingAllEnvMetaTag: appendingAllEnvMetaTag,
		AgentEnvMetaTagKey:     agentEnvMetaTagKey,
		LogFileTagsPath:        logFileTagsPath,
		MachineUUID:            machineUUID,
	}
	processorTag.parseDefaultAddedTag("HOST_NAME", TagKeyHostName, hostNameDefaultTagKey, pipelineMetaTagKey)
	processorTag.parseDefaultAddedTag("HOST_IP", TagKeyHostIP, hostIPDefaultTagKey, pipelineMetaTagKey)
	processorTag.parseOptionalTag("HOST_ID", TagKeyHostID, hostIDDefaultTagKey, pipelineMetaTagKey)
	processorTag.parseOptionalTag("CLOUD_PROVIDER", TagKeyCloudProvider, cloudProviderDefaultTagKey, pipelineMetaTagKey)
	return processorTag
}

func (p *ProcessorTag) ProcessV1(logCtx *pipeline.LogWithContext) {
	tags, ok := logCtx.Context["tags"]
	if !ok {
		return
	}
	tagsMap, ok := tags.(map[string]string)
	if !ok {
		return
	}
	p.addTag(TagKeyHostName, util.GetHostName(), tagsMap)
	p.addTag(TagKeyHostIP, util.GetIPAddress(), tagsMap)
	// TODO: add host id and cloud provider
	p.addTag(TagKeyHostID, "host id", tagsMap)
	p.addTag(TagKeyCloudProvider, "cloud provider", tagsMap)

	// TODO: file tags, read in background with double buffer

	for i := 0; i < len(helper.EnvTags); i += 2 {
		tagsMap[helper.EnvTags[i]] = helper.EnvTags[i+1]
	}
	tagsMap[machineUUIDDefaultTagKey] = p.MachineUUID
}

func (p *ProcessorTag) ProcessV2(in *models.PipelineGroupEvents) {
	tagsMap := make(map[string]string)
	p.addTag(TagKeyHostName, util.GetHostName(), tagsMap)
	p.addTag(TagKeyHostIP, util.GetIPAddress(), tagsMap)
	// TODO: add host id and cloud provider
	p.addTag(TagKeyHostID, "host id", tagsMap)
	p.addTag(TagKeyCloudProvider, "cloud provider", tagsMap)
	for k, v := range tagsMap {
		in.Group.Tags.Add(k, v)
	}

	// env tags
	for i := 0; i < len(helper.EnvTags); i += 2 {
		in.Group.Tags.Add(helper.EnvTags[i], helper.EnvTags[i+1])
	}
}

func (p *ProcessorTag) parseDefaultAddedTag(configKey string, tagKey TagKey, defaultKey string, config map[string]string) {
	if customKey, ok := config[configKey]; ok {
		if customKey != "" {
			if customKey == defaultConfigTagKeyValue {
				p.PipelineMetaTagKey[tagKey] = defaultKey
			} else {
				p.PipelineMetaTagKey[tagKey] = customKey
			}
		}
		// empty value means delete
	} else {
		p.PipelineMetaTagKey[tagKey] = defaultKey
	}
}

func (p *ProcessorTag) parseOptionalTag(configKey string, tagKey TagKey, defaultKey string, config map[string]string) {
	if customKey, ok := config[configKey]; ok {
		if customKey != "" {
			if customKey == defaultConfigTagKeyValue {
				p.PipelineMetaTagKey[tagKey] = defaultKey
			} else {
				p.PipelineMetaTagKey[tagKey] = customKey
			}
		}
		// empty value means delete
	}
}

func (p *ProcessorTag) addTag(tagKey TagKey, value string, tags map[string]string) {
	if key, ok := p.PipelineMetaTagKey[tagKey]; ok {
		if key != "" {
			tags[key] = value
		}
	}
}
