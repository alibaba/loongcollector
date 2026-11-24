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
	"github.com/alibaba/ilogtail/pkg/util"
)

func TestTagDefault(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(make(map[string]string), true, make(map[string]string))
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.Process(in)
	assert.Equal(t, 3, in.Group.Tags.Len())
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get(hostNameDefaultTagKey))
	assert.Equal(t, util.GetIPAddress(), in.Group.Tags.Get(hostIPDefaultTagKey))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag"))

	pipelineMetaTagKeyDefault := make(map[string]string)
	pipelineMetaTagKeyDefault["HOST_NAME"] = defaultConfigTagKeyValue
	pipelineMetaTagKeyDefault["HOST_IP"] = defaultConfigTagKeyValue
	processorTag = NewProcessorTag(pipelineMetaTagKeyDefault, true, make(map[string]string))
	in = &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.Process(in)
	assert.Equal(t, 3, in.Group.Tags.Len())
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
	}, false, map[string]string{
		"test_env_tag": "test_env_tag_2",
	})
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.Process(in)
	assert.Equal(t, 3, in.Group.Tags.Len())
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get("test_host_name"))
	assert.Equal(t, util.GetIPAddress(), in.Group.Tags.Get("test_host_ip"))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag_2"))
}

func TestTagDelete(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(map[string]string{
		"HOST_NAME": "",
		"HOST_IP":   "",
	}, false, make(map[string]string))
	in := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags: models.NewTags(),
		},
	}
	processorTag.Process(in)
	assert.Equal(t, 0, in.Group.Tags.Len())
}

func TestTagNilDefense(t *testing.T) {
	helper.EnvTags = []string{
		"test_env_tag",
		"test_env_tag_value",
	}
	processorTag := NewProcessorTag(make(map[string]string), true, make(map[string]string))

	// nil in / nil Group must not panic and must be a no-op.
	assert.NotPanics(t, func() { processorTag.Process(nil) })
	assert.NotPanics(t, func() { processorTag.Process(&models.PipelineGroupEvents{}) })

	// A bare GroupInfo has a nil Tags interface; Process must not panic and
	// must materialize a writable Tags so configured tags actually land.
	in := &models.PipelineGroupEvents{Group: &models.GroupInfo{}}
	assert.NotPanics(t, func() { processorTag.Process(in) })
	assert.NotNil(t, in.Group.Tags)
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get(hostNameDefaultTagKey))
	assert.Equal(t, "test_env_tag_value", in.Group.Tags.Get("test_env_tag"))

	// The shared Nil sentinel silently drops on Add; it must be replaced too.
	in = &models.PipelineGroupEvents{Group: &models.GroupInfo{Tags: models.NilStringValues}}
	assert.NotPanics(t, func() { processorTag.Process(in) })
	assert.False(t, in.Group.Tags.IsNil())
	assert.Equal(t, util.GetHostName(), in.Group.Tags.Get(hostNameDefaultTagKey))
}

func TestProcessorTagRefreshesContainerDefaults(t *testing.T) {
	previous := helper.EnvTags
	t.Cleanup(func() { helper.EnvTags = previous })
	helper.EnvTags = []string{"env", "first"}
	tag := NewProcessorTag(nil, false, map[string]string{"env": hostNameDefaultTagKey})
	template := models.NewGroupInfoTemplate(nil, map[string]string{
		hostNameDefaultTagKey: "container", "same": "original",
	})

	first := template.NewSingleEventGroup(new(models.Log))
	tag.Process(first)
	second := template.NewSingleEventGroup(new(models.Log))
	tag.Process(second)
	assert.True(t, models.EqualStringValues(first.Group.Tags, second.Group.Tags))
	assert.Equal(t, "first", first.Group.Tags.Get(hostNameDefaultTagKey))

	helper.EnvTags = []string{"env", "second"}
	third := template.NewSingleEventGroup(new(models.Log))
	tag.Process(third)
	assert.Equal(t, "second", third.Group.Tags.Get(hostNameDefaultTagKey))
	assert.Equal(t, "first", first.Group.Tags.Get(hostNameDefaultTagKey))
	first.Group.Tags.Iterator()["same"] = "changed"
	assert.Equal(t, "original", second.Group.Tags.Get("same"))

	fourth := template.NewSingleEventGroup(new(models.Log))
	fourth.Group.Tags.Add("custom", "kept")
	tag.Process(fourth)
	assert.Equal(t, "kept", fourth.Group.Tags.Get("custom"))
	assert.Equal(t, "second", fourth.Group.Tags.Get(hostNameDefaultTagKey))
}
