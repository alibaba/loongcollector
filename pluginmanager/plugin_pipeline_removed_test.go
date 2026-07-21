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

package pluginmanager

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// fakeRemovableServiceInput is a minimal ServiceInputV1 that also implements
// pipeline.PipelineRemovedCleaner, used to verify removal-hook routing.
type fakeRemovableServiceInput struct {
	removedCount int
}

func (f *fakeRemovableServiceInput) Init(pipeline.Context) (int, error) { return 0, nil }
func (f *fakeRemovableServiceInput) Description() string                { return "fake-removable" }
func (f *fakeRemovableServiceInput) Stop() error                        { return nil }
func (f *fakeRemovableServiceInput) Start(pipeline.Collector) error     { return nil }
func (f *fakeRemovableServiceInput) OnPipelineRemoved()                 { f.removedCount++ }

// fakePlainServiceInput does NOT implement PipelineRemovedCleaner.
type fakePlainServiceInput struct{}

func (f *fakePlainServiceInput) Init(pipeline.Context) (int, error) { return 0, nil }
func (f *fakePlainServiceInput) Description() string                { return "fake-plain" }
func (f *fakePlainServiceInput) Stop() error                        { return nil }
func (f *fakePlainServiceInput) Start(pipeline.Collector) error     { return nil }

func TestNotifyPipelineRemoved(t *testing.T) {
	removable := &fakeRemovableServiceInput{}
	config := &LogstoreConfig{
		PluginRunner: &pluginv1Runner{
			ServicePlugins: []*ServiceWrapperV1{
				{Input: removable},
				{Input: &fakePlainServiceInput{}}, // must be skipped, not panic
			},
		},
	}

	notifyPipelineRemoved(config)
	require.Equal(t, 1, removable.removedCount, "hook should fire exactly once for a removable input")

	// Idempotent-safe on repeated calls (each call notifies once more).
	notifyPipelineRemoved(config)
	require.Equal(t, 2, removable.removedCount)

	// nil-safe: nil config and a config without a runner must be no-ops.
	notifyPipelineRemoved(nil)
	notifyPipelineRemoved(&LogstoreConfig{})
	require.Equal(t, 2, removable.removedCount)
}
