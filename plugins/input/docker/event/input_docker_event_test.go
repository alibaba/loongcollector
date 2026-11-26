// Copyright 2026 iLogtail Authors
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

package event

import (
	"testing"

	"github.com/alibaba/ilogtail/pkg/models"

	"github.com/moby/moby/api/types/events"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
)

func TestServiceDockerEventsClampsQueueSize(t *testing.T) {
	tests := []struct {
		name     string
		input    int
		expected int
	}{
		{name: "below minimum", input: 0, expected: 4},
		{name: "at minimum", input: 4, expected: 4},
		{name: "within range", input: 512, expected: 512},
		{name: "at maximum", input: 10000, expected: 10000},
		{name: "above maximum", input: 10001, expected: 10000},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			input := &ServiceDockerEvents{EventQueueSize: test.input}

			input.clampEventQueueSize()

			assert.Equal(t, test.expected, input.EventQueueSize)
		})
	}
}

func TestServiceDockerEventsFire(t *testing.T) {
	const eventTimeNano = int64(1700000000123456789)
	message := events.Message{
		Type:     events.ContainerEventType,
		Action:   events.ActionStart,
		TimeNano: eventTimeNano,
		Actor: events.Actor{
			ID: "container-id",
			Attributes: map[string]string{
				"image": "example/image:latest",
				"name":  "example-container",
			},
		},
	}

	t.Run("includes attributes", func(t *testing.T) {
		ctx := helper.NewObservePipelineContext(1)
		input := &ServiceDockerEvents{}

		input.fire(ctx.Collector(), message)

		groups := ctx.Collector().ToArray()
		require.Len(t, groups, 1)
		require.Len(t, groups[0].Events, 1)
		log, ok := groups[0].Events[0].(*models.Log)
		require.True(t, ok)
		fields := log.GetIndices().Iterator()
		assert.Equal(t, "1700000000123456789", fields["_time_nano_"])
		assert.Equal(t, "start", fields["_action_"])
		assert.Equal(t, "container", fields["_type_"])
		assert.Equal(t, "container-id", fields["_id_"])
		assert.Equal(t, "example/image:latest", fields["image"])
		assert.Equal(t, "example-container", fields["name"])
		assert.Equal(t, uint64(eventTimeNano), log.GetTimestamp())
	})

	t.Run("ignores attributes", func(t *testing.T) {
		ctx := helper.NewObservePipelineContext(1)
		input := &ServiceDockerEvents{IgnoreAttributes: true}

		input.fire(ctx.Collector(), message)

		groups := ctx.Collector().ToArray()
		require.Len(t, groups, 1)
		require.Len(t, groups[0].Events, 1)
		log, ok := groups[0].Events[0].(*models.Log)
		require.True(t, ok)
		fields := log.GetIndices().Iterator()
		assert.Equal(t, "1700000000123456789", fields["_time_nano_"])
		assert.Equal(t, "start", fields["_action_"])
		assert.Equal(t, "container", fields["_type_"])
		assert.Equal(t, "container-id", fields["_id_"])
		assert.NotContains(t, fields, "image")
		assert.NotContains(t, fields, "name")
		assert.NotContains(t, fields, "")
		assert.Len(t, fields, 4)
	})
}
