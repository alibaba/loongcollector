// Copyright 2025 iLogtail Authors
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

package odps

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/require"

	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherOdpsExportLogs(t *testing.T) {
	sender := &mockTunnelSender{}
	flusher := newTestFlusherOdps(sender)
	first := models.NewLog("", []byte("first"), "", "", "", models.NewTags(), 1733205626123456789)
	second := models.NewLog("", []byte("second"), "", "", "", models.NewTags(), 1733205627987654321)
	groups := []*models.PipelineGroupEvents{
		{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("__path__", "/tmp/first.log")),
			Events: []models.PipelineEvent{first},
		},
		nil,
		{},
		{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("__path__", "/tmp/second.log")),
			Events: []models.PipelineEvent{second},
		},
	}

	require.NoError(t, flusher.Export(groups, nil))
	require.Len(t, sender.rows, 2)
	require.Equal(t, first.GetTimestamp(), sender.rows[0].Timestamp)
	require.Equal(t, second.GetTimestamp(), sender.rows[1].Timestamp)
	require.Equal(t, map[string]string{"content": "first"}, sender.rows[0].Fields)
	require.Equal(t, map[string]string{"content": "second"}, sender.rows[1].Fields)
	require.Equal(t, 1, sender.flushCalls)
}

func TestFlusherOdpsExportEmptyGroups(t *testing.T) {
	for name, groups := range map[string][]*models.PipelineGroupEvents{
		"nil":   nil,
		"empty": {nil, {}},
	} {
		t.Run(name, func(t *testing.T) {
			sender := &mockTunnelSender{}
			require.NoError(t, newTestFlusherOdps(sender).Export(groups, nil))
			require.Empty(t, sender.rows)
			require.Equal(t, 1, sender.flushCalls)
		})
	}
}

func TestFlusherOdpsExportConversionError(t *testing.T) {
	sender := &mockTunnelSender{}
	valid := models.NewLog("", []byte("valid"), "", "", "", models.NewTags(), 1e9)
	invalid := models.NewLog("", nil, "", "", "", models.NewTags(), 1e9)
	invalid.GetIndices().Add("unsupported", 123)
	groups := []*models.PipelineGroupEvents{
		{Events: []models.PipelineEvent{valid, invalid}},
		{Events: []models.PipelineEvent{valid}},
	}

	err := newTestFlusherOdps(sender).Export(groups, nil)
	require.ErrorContains(t, err, "unsupported log content type for key unsupported")
	require.Empty(t, sender.rows)
	require.Zero(t, sender.flushCalls)
}

func TestFlusherOdpsExportSenderErrors(t *testing.T) {
	sendErr := errors.New("send failed")
	flushErr := errors.New("flush failed")
	for _, tt := range []struct {
		name        string
		sender      *mockTunnelSender
		wantErr     error
		wantSends   int
		wantFlushes int
	}{
		{name: "send", sender: &mockTunnelSender{sendErr: sendErr}, wantErr: sendErr, wantSends: 1},
		{name: "flush", sender: &mockTunnelSender{flushErr: flushErr}, wantErr: flushErr, wantSends: 3, wantFlushes: 1},
	} {
		t.Run(tt.name, func(t *testing.T) {
			log := models.NewLog("", []byte("message"), "", "", "", models.NewTags(), 1e9)
			groups := []*models.PipelineGroupEvents{
				{Events: []models.PipelineEvent{log, log}},
				{Events: []models.PipelineEvent{log}},
			}
			require.ErrorIs(t, newTestFlusherOdps(tt.sender).Export(groups, nil), tt.wantErr)
			require.Len(t, tt.sender.rows, tt.wantSends)
			require.Equal(t, tt.wantFlushes, tt.sender.flushCalls)
		})
	}
}

func TestFlusherOdpsExportMetricAndSpan(t *testing.T) {
	sender := &mockTunnelSender{}
	groups := []*models.PipelineGroupEvents{{Events: []models.PipelineEvent{
		models.NewSingleValueMetric("cpu.usage", models.MetricTypeGauge, models.NewTags(), 2e9, 0.5),
		models.NewSpan("operation", "trace-id", "span-id", models.SpanKindServer, 3e9, 4e9, models.NewTags(), nil, nil),
	}}}

	require.NoError(t, newTestFlusherOdps(sender).Export(groups, nil))
	require.Len(t, sender.rows, 2)
	require.Equal(t, "cpu.usage", sender.rows[0].Fields["__name__"])
	require.Equal(t, "0.5", sender.rows[0].Fields["__value__"])
	require.Equal(t, "trace-id", sender.rows[1].Fields["traceID"])
	require.Equal(t, "span-id", sender.rows[1].Fields["spanID"])
	require.Equal(t, 1, sender.flushCalls)
}

func newTestFlusherOdps(sender TunnelSender) *FlusherOdps {
	return &FlusherOdps{
		sender:  sender,
		context: mock.NewEmptyContext("", "", ""),
	}
}

type mockTunnelSender struct {
	rows       []converter.EventRecord
	flushCalls int
	sendErr    error
	flushErr   error
}

func (*mockTunnelSender) Init() error { return nil }

func (s *mockTunnelSender) Send(row converter.EventRecord) error {
	s.rows = append(s.rows, row)
	return s.sendErr
}

func (s *mockTunnelSender) Flush() error {
	s.flushCalls++
	return s.flushErr
}
