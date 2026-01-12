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

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/plugins/test/mock"
)

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
