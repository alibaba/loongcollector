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

const (
	hostNameDefaultTagKey      = "host_name"
	hostIPDefaultTagKey        = "host_ip"
	hostIDDefaultTagKey        = "host_id"
	cloudProviderDefaultTagKey = "cloud_provider"
	defaultConfigTagKeyValue   = "__default__"
)

// Processor interface cannot meet the requirements of tag processing, so we need to create a special ProcessorTag struct
type ProcessorTag struct {
	PipelineMetaTagKey     map[string]string
	AppendingAllEnvMetaTag bool
	AgentEnvMetaTagKey     map[string]string
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
	p.addDefaultAddedTag("HOST_NAME", hostNameDefaultTagKey, util.GetHostName(), tagsMap)
	p.addDefaultAddedTag("HOST_IP", hostIPDefaultTagKey, util.GetIPAddress(), tagsMap)
	p.addOptionalTag("HOST_ID", hostIDDefaultTagKey, "", tagsMap)
	p.addOptionalTag("CLOUD_PROVIDER", cloudProviderDefaultTagKey, "", tagsMap)

	for i := 0; i < len(helper.EnvTags); i += 2 {
		tagsMap[helper.EnvTags[i]] = helper.EnvTags[i+1]
	}
}

func (p *ProcessorTag) ProcessV2(in *models.PipelineGroupEvents) {
	tagsMap := make(map[string]string)
	p.addDefaultAddedTag("HOST_NAME", hostNameDefaultTagKey, util.GetHostName(), tagsMap)
	p.addDefaultAddedTag("HOST_IP", hostIPDefaultTagKey, util.GetIPAddress(), tagsMap)
	p.addOptionalTag("HOST_ID", hostIDDefaultTagKey, "", tagsMap)
	p.addOptionalTag("CLOUD_PROVIDER", cloudProviderDefaultTagKey, "", tagsMap)
	for k, v := range tagsMap {
		in.Group.Tags.Add(k, v)
	}

	// env tags
	for i := 0; i < len(helper.EnvTags); i += 2 {
		in.Group.Tags.Add(helper.EnvTags[i], helper.EnvTags[i+1])
	}
}

func (p *ProcessorTag) addDefaultAddedTag(configKey, defaultKey, value string, tags map[string]string) {
	if key, ok := p.PipelineMetaTagKey[configKey]; ok {
		if key != "" {
			if key == defaultConfigTagKeyValue {
				tags[defaultKey] = value
			} else {
				tags[key] = value
			}
		}
		// empty value means delete
	} else {
		tags[defaultKey] = value
	}
}

func (p *ProcessorTag) addOptionalTag(configKey, defaultKey, value string, tags map[string]string) {
	if key, ok := p.PipelineMetaTagKey[configKey]; ok {
		if key != "" {
			if key == defaultConfigTagKeyValue {
				tags[defaultKey] = value
			} else {
				tags[key] = value
			}
		}
		// empty value means delete
	}
}
