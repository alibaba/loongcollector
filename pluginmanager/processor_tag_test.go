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
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

func TestTagDefault(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(make(map[string]string), false, make(map[string]string), "", "")
	logCtx := &pipeline.LogWithContext{
		Context: map[string]interface{}{
			"tags": map[string]string{},
		},
	}
	processorTag.ProcessV1(logCtx)
	tagsMap := logCtx.Context["tags"].(map[string]string)
	assert.Equal(t, 3, len(tagsMap))
	assert.Equal(t, util.GetHostName(), tagsMap[hostNameDefaultTagKey])
	assert.Equal(t, util.GetIPAddress(), tagsMap[hostIPDefaultTagKey])
	assert.Equal(t, "test_env_tag_value", tagsMap["test_env_tag"])

	pipelineMetaTagKeyDefault := make(map[string]string)
	pipelineMetaTagKeyDefault["HOST_NAME"] = defaultConfigTagKeyValue
	pipelineMetaTagKeyDefault["HOST_IP"] = defaultConfigTagKeyValue
	processorTag = NewProcessorTag(pipelineMetaTagKeyDefault, false, make(map[string]string), "", "")
	logCtx = &pipeline.LogWithContext{
		Context: map[string]interface{}{
			"tags": map[string]string{},
		},
	}
	processorTag.ProcessV1(logCtx)
	tagsMap = logCtx.Context["tags"].(map[string]string)
	assert.Equal(t, 3, len(tagsMap))
	assert.Equal(t, util.GetHostName(), tagsMap[hostNameDefaultTagKey])
	assert.Equal(t, util.GetIPAddress(), tagsMap[hostIPDefaultTagKey])
	assert.Equal(t, "test_env_tag_value", tagsMap["test_env_tag"])
}

func TestTagDefaultV2(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(make(map[string]string), false, make(map[string]string), "", "")
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.ProcessV2(in)
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get(hostNameDefaultTagKey))
	assert.Equal(t, util.GetIPAddress(), in.Group.Tags.Get(hostIPDefaultTagKey))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag"))

	pipelineMetaTagKeyDefault := make(map[string]string)
	pipelineMetaTagKeyDefault["HOST_NAME"] = defaultConfigTagKeyValue
	pipelineMetaTagKeyDefault["HOST_IP"] = defaultConfigTagKeyValue
	processorTag = NewProcessorTag(pipelineMetaTagKeyDefault, false, make(map[string]string), "", "")
	in = &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.ProcessV2(in)
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get(hostNameDefaultTagKey))
	assert.Equal(t, util.GetIPAddress(), in.Group.Tags.Get(hostIPDefaultTagKey))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag"))
}

func TestTagRename(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(map[string]string{
		"HOST_NAME": "test_host_name",
		"HOST_IP":   "test_host_ip",
	}, false, make(map[string]string), "", "")
	logCtx := &pipeline.LogWithContext{
		Context: map[string]interface{}{
			"tags": map[string]string{},
		},
	}
	processorTag.ProcessV1(logCtx)
	tagsMap := logCtx.Context["tags"].(map[string]string)
	assert.Equal(t, 3, len(tagsMap))
	assert.Equal(t, util.GetHostName(), tagsMap["test_host_name"])
	assert.Equal(t, util.GetIPAddress(), tagsMap["test_host_ip"])
	assert.Equal(t, "test_env_tag_value", tagsMap["test_env_tag"])
}

func TestTagRenameV2(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(map[string]string{
		"HOST_NAME": "test_host_name",
		"HOST_IP":   "test_host_ip",
	}, false, make(map[string]string), "", "")
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.ProcessV2(in)
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get("test_host_name"))
	assert.Equal(t, util.GetIPAddress(), in.Group.Tags.Get("test_host_ip"))
}

func TestTagDelete(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(map[string]string{
		"HOST_NAME": "",
		"HOST_IP":   "",
	}, false, make(map[string]string), "", "")
	logCtx := &pipeline.LogWithContext{
		Context: map[string]interface{}{
			"tags": make([]*protocol.LogTag, 0),
		},
	}
	processorTag.ProcessV1(logCtx)
	tagsMap := logCtx.Context["tags"].([]*protocol.LogTag)
	assert.Equal(t, 1, len(tagsMap))
	assert.Equal(t, "test_env_tag", tagsMap[0].Key)
	assert.Equal(t, "test_env_tag_value", tagsMap[0].Value)
}

func TestTagDeleteV2(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(map[string]string{
		"HOST_NAME": "",
		"HOST_IP":   "",
	}, false, make(map[string]string), "", "")
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.ProcessV2(in)
	assert.Equal(t, "", in.Group.Tags.Get("HOST_NAME"))
	assert.Equal(t, "", in.Group.Tags.Get("HOST_IP"))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag"))
}
