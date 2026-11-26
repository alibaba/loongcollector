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

//go:build windows

package input_wineventlog

import (
	"testing"
	"time"

	"github.com/elastic/beats/v7/winlogbeat/sys"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/input/input_wineventlog/eventlog"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type singleRecordEventLog struct {
	record   eventlog.Record
	shutdown chan struct{}
}

func (*singleRecordEventLog) Open(eventlog.Checkpoint) error { return nil }
func (*singleRecordEventLog) Close() error                   { return nil }
func (*singleRecordEventLog) Name() string                   { return "Application" }

func (log *singleRecordEventLog) Read() ([]eventlog.Record, error) {
	close(log.shutdown)
	return []eventlog.Record{log.record}, nil
}

func TestWinEventLogRecordContents(t *testing.T) {
	ctx := helper.NewObservePipelineContext(1)
	timestamp := time.Unix(1_750_000_000, 123_456_789)
	shutdown := make(chan struct{})
	source := &singleRecordEventLog{
		record: eventlog.Record{Event: sys.Event{
			EventIdentifier: sys.EventIdentifier{ID: 4624},
			TimeCreated:     sys.TimeCreated{SystemTime: timestamp},
			Message:         "Windows event message",
		}},
		shutdown: shutdown,
	}
	input := &WinEventLog{
		context:     mock.NewEmptyContext("project", "logstore", "wineventlog"),
		collector:   ctx.Collector(),
		eventLogger: source,
		shutdown:    shutdown,
	}

	require.False(t, input.run())

	groups := ctx.Collector().ToArray()
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 1)
	log := groups[0].Events[0].(*models.Log)
	assert.Equal(t, "4624", log.Contents.Get("event_id"))
	assert.Equal(t, "Windows event message", log.Contents.Get("message"))
	assert.Zero(t, log.Tags.Len())
	assert.Equal(t, uint64(timestamp.UnixNano()), log.Timestamp)
}
