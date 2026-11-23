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

package stdout

import (
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/moby/moby/api/types/container"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestLogDriverSupported(t *testing.T) {
	tests := []struct {
		name       string
		hostConfig *container.HostConfig
		expected   bool
	}{
		{name: "nil host config", hostConfig: nil, expected: true},
		{name: "json file", hostConfig: stdoutHostConfigWithLogDriver("json-file"), expected: true},
		{name: "journald", hostConfig: stdoutHostConfigWithLogDriver("journald"), expected: false},
		{name: "unsupported", hostConfig: stdoutHostConfigWithLogDriver("syslog"), expected: false},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			assert.Equal(t, test.expected, logDriverSupported(container.InspectResponse{HostConfig: test.hostConfig}))
		})
	}
}

func TestLogPathEmpty(t *testing.T) {
	assert.True(t, logPathEmpty(container.InspectResponse{}))
	assert.False(t, logPathEmpty(container.InspectResponse{LogPath: "/var/lib/docker/containers/id/id-json.log"}))
}

func TestServiceDockerStdout_Init(t *testing.T) {
	sds := &ServiceDockerStdout{
		IncludeLabel: map[string]string{
			"inlabel":    "label",
			"inlabelreg": "^label$",
		},
		ExcludeLabel: map[string]string{
			"exlabel":    "label",
			"exlabelreg": "^label$",
		},
		IncludeEnv: map[string]string{
			"inenv":    "env",
			"inenvreg": "^env$",
		},
		ExcludeEnv: map[string]string{
			"exenv":    "env",
			"exenvreg": "^env$",
		},
		IncludeContainerLabel: map[string]string{
			"inclabel":    "label",
			"inclabelreg": "^label$",
		},
		ExcludeContainerLabel: map[string]string{
			"exclabel":    "label",
			"exclabelreg": "^label$",
		},
		IncludeK8sLabel: map[string]string{
			"inklabel":    "label",
			"inklabelreg": "^label$",
		},
		ExcludeK8sLabel: map[string]string{
			"exklabel":    "label",
			"exklabelreg": "^label$",
		},
		K8sNamespaceRegex: "1",
		K8sContainerRegex: "2",
		K8sPodRegex:       "3",
	}
	ctx := mock.NewEmptyContext("project", "store", "config")
	_, err := sds.Init(ctx)

	assert.Equal(t, map[string]string{
		"inlabel":  "label",
		"inclabel": "label",
	}, sds.IncludeLabel)
	assert.Equal(t, map[string]string{
		"exlabel":  "label",
		"exclabel": "label",
	}, sds.ExcludeLabel)

	// Verify IncludeLabelRegex contains expected keys with patterns
	assert.Len(t, sds.IncludeLabelRegex, 2)
	assert.NotNil(t, sds.IncludeLabelRegex["inlabelreg"])
	assert.NotNil(t, sds.IncludeLabelRegex["inclabelreg"])
	// Verify ExcludeLabelRegex contains expected keys with patterns
	assert.Len(t, sds.ExcludeLabelRegex, 2)
	assert.NotNil(t, sds.ExcludeLabelRegex["exlabelreg"])
	assert.NotNil(t, sds.ExcludeLabelRegex["exclabelreg"])

	assert.Equal(t, map[string]string{
		"inenv": "env",
	}, sds.IncludeEnv)
	assert.Equal(t, map[string]string{
		"exenv": "env",
	}, sds.ExcludeEnv)

	// Verify IncludeEnvRegex contains expected keys with patterns
	assert.Len(t, sds.IncludeEnvRegex, 1)
	assert.NotNil(t, sds.IncludeEnvRegex["inenvreg"])

	// Verify ExcludeEnvRegex contains expected keys with patterns
	assert.Len(t, sds.ExcludeEnvRegex, 1)
	assert.NotNil(t, sds.ExcludeEnvRegex["exenvreg"])

	assert.Equal(t, map[string]string{
		"inklabel": "label",
	}, sds.K8sFilter.IncludeLabels)

	assert.Equal(t, map[string]string{
		"exklabel": "label",
	}, sds.K8sFilter.ExcludeLabels)

	// Verify K8sFilter IncludeLabelRegs contains expected keys
	assert.Len(t, sds.K8sFilter.IncludeLabelRegs, 1)
	assert.NotNil(t, sds.K8sFilter.IncludeLabelRegs["inklabelreg"])

	// Verify K8sFilter ExcludeLabelRegs contains expected keys
	assert.Len(t, sds.K8sFilter.ExcludeLabelRegs, 1)
	assert.NotNil(t, sds.K8sFilter.ExcludeLabelRegs["exklabelreg"])

	// Verify K8sFilter regex patterns are set
	assert.NotNil(t, sds.K8sFilter.PodReg)
	assert.NotNil(t, sds.K8sFilter.NamespaceReg)
	assert.NotNil(t, sds.K8sFilter.ContainerReg)

	// Verify regex patterns work correctly
	matched, _ := sds.K8sFilter.PodReg.MatchString("3")
	assert.True(t, matched)
	matched, _ = sds.K8sFilter.NamespaceReg.MatchString("1")
	assert.True(t, matched)
	matched, _ = sds.K8sFilter.ContainerReg.MatchString("2")
	assert.True(t, matched)

	assert.NoError(t, err)
}

func stdoutHostConfigWithLogDriver(driver string) *container.HostConfig {
	return &container.HostConfig{
		LogConfig: container.LogConfig{Type: driver},
	}
}

type stdoutTestContext struct {
	pipeline.Context
	oldContentTags bool
}

func (c stdoutTestContext) GetPipelineScopeConfig() *config.GlobalConfig {
	return &config.GlobalConfig{UsingOldContentTag: c.oldContentTags}
}

func TestDockerFileSynerPreservesTagModeAndSource(t *testing.T) {
	for _, mode := range []struct {
		name           string
		oldContentTags bool
	}{
		{"group tags", false},
		{"content tags", true},
	} {
		t.Run(mode.name, func(t *testing.T) {
			ctx := helper.NewObservePipelineContext(4)
			input := &ServiceDockerStdout{
				context:    stdoutTestContext{Context: mock.NewEmptyContext("project", "logstore", "config"), oldContentTags: mode.oldContentTags},
				collector:  ctx.Collector(),
				Stdout:     true,
				Stderr:     true,
				MaxLogSize: 4096,
			}
			info := &containercenter.DockerInfoDetail{
				ContainerNameTag: map[string]string{"container": "test-container"},
			}
			info.ContainerInfo.ID = "test-id"
			syner := NewDockerFileSyner(input, info, map[string]helper.LogFileReaderCheckPoint{
				info.ContainerInfo.ID: {Path: "unused.log"},
			})
			const line = `{"log":"test line\n","time":"2025-01-01T00:00:00.123456789Z","stream":"stdout"}` + "\n"
			processor := syner.dockerFileProcessor
			processor.Process([]byte(line+line), 0)
			groups := ctx.Collector().ToArray()
			require.Len(t, groups, 2)
			for _, group := range groups {
				require.Len(t, group.Events, 1)
				log, ok := group.Events[0].(*models.Log)
				require.True(t, ok)
				require.Equal(t, "test line", string(log.GetBody()))
				require.Equal(t, "2025-01-01T00:00:00.123456789Z", log.GetIndices().Get("_time_"))
				require.Equal(t, "stdout", log.GetIndices().Get("_source_"))
				require.Equal(t, uint64(0), log.GetTimestamp()%uint64(time.Second))
				require.NotEmpty(t, group.Group.GetMetadata().Get("source"))
				require.False(t, log.GetTags().Contains("container"))
				if mode.oldContentTags {
					require.Equal(t, "test-container", log.GetIndices().Get("container"))
					require.False(t, group.Group.GetTags().Contains("container"))
				} else {
					require.False(t, log.GetIndices().Contains("container"))
					require.Equal(t, "test-container", group.Group.GetTags().Get("container"))
				}
			}
			require.Equal(t, groups[0].Group.GetMetadata().Get("source"), groups[1].Group.GetMetadata().Get("source"))
			groups[0].Group.GetTags().Add("local", "changed")
			require.False(t, groups[1].Group.GetTags().Contains("local"))
		})
	}
}
