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

//go:build linux || windows
// +build linux windows

package logmeta

import (
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/assert"

	"regexp"
	"testing"
)

func TestServiceDockerStdout_Init(t *testing.T) {
	sds := &InputDockerFile{
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
		LogPath:           "tets",
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

	assert.Equal(t, map[string]*regexp.Regexp{
		"inlabelreg":  regexp.MustCompile("^label$"),
		"inclabelreg": regexp.MustCompile("^label$"),
	}, sds.IncludeLabelRegex)
	assert.Equal(t, map[string]*regexp.Regexp{
		"exlabelreg":  regexp.MustCompile("^label$"),
		"exclabelreg": regexp.MustCompile("^label$"),
	},
		sds.ExcludeLabelRegex)

	assert.Equal(t, map[string]string{
		"inenv": "env",
	}, sds.IncludeEnv)
	assert.Equal(t, map[string]string{
		"exenv": "env",
	}, sds.ExcludeEnv)

	assert.Equal(t, map[string]*regexp.Regexp{
		"inenvreg": regexp.MustCompile("^env$"),
	}, sds.IncludeEnvRegex)

	assert.Equal(t, map[string]*regexp.Regexp{
		"exenvreg": regexp.MustCompile("^env$"),
	}, sds.ExcludeEnvRegex)

	assert.Equal(t, map[string]string{
		"inklabel": "label",
	}, sds.K8sFilter.IncludeLabels)

	assert.Equal(t, map[string]string{
		"exklabel": "label",
	}, sds.K8sFilter.ExcludeLabels)

	assert.Equal(t, map[string]*regexp.Regexp{
		"inklabelreg": regexp.MustCompile("^label$"),
	}, sds.K8sFilter.IncludeLabelRegs)

	assert.Equal(t, map[string]*regexp.Regexp{
		"exklabelreg": regexp.MustCompile("^label$"),
	}, sds.K8sFilter.ExcludeLabelRegs)

	assert.Equal(t, regexp.MustCompile("3"), sds.K8sFilter.PodReg)
	assert.Equal(t, regexp.MustCompile("1"), sds.K8sFilter.NamespaceReg)
	assert.Equal(t, regexp.MustCompile("2"), sds.K8sFilter.ContainerReg)

	assert.NoError(t, err)
}
