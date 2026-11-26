// Copyright 2021 iLogtail Authors
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

//go:build linux
// +build linux

package systemv2

import (
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"testing"
)

func TestInputSystem_CollectOpenFD(t *testing.T) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs["metric_system_v2"]().(*InputSystem)
	if _, err := p.Init(cxt); err != nil {
		t.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	ctx := helper.NewGroupedPipelineContext()
	p.CollectOpenFD(ctx.Collector())
	groups := ctx.Collector().ToArray()
	require.Len(t, groups, 2)
	m := make(map[string]string)
	for _, group := range groups {
		require.Len(t, group.Events, 1)
		metric, ok := group.Events[0].(*models.Metric)
		require.True(t, ok)
		m[metric.GetName()] = "exist"
	}
	assert.NotEqual(t, "", m["fd_allocated"])
	assert.NotEqual(t, "", m["fd_max"])

}
