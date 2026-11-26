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

//go:build linux

package journal

import (
	"testing"
	"time"

	"github.com/coreos/go-systemd/sdjournal"
	"github.com/stretchr/testify/assert"
)

func TestJournalEntryLogContents(t *testing.T) {
	const realtime = 1_750_000_000_123_456
	entry := &sdjournal.JournalEntry{
		Fields: map[string]string{
			"MESSAGE":              "journal message",
			"_SYSTEMD_UNIT":        "example.service",
			"PRIORITY":             "6",
			"SYSLOG_FACILITY":      "3",
			"_realtime_timestamp_": "old value",
		},
		RealtimeTimestamp:  realtime,
		MonotonicTimestamp: 1234,
	}
	input := &ServiceJournal{UseJournalEventTime: true, ParsePriority: true, ParseSyslogFacility: true}

	log := input.newLogEvent(entry)

	assert.Equal(t, map[string]any{
		"MESSAGE":               "journal message",
		"_SYSTEMD_UNIT":         "example.service",
		"PRIORITY":              "informational",
		"SYSLOG_FACILITY":       "daemon",
		"_realtime_timestamp_":  "1750000000123456",
		"_monotonic_timestamp_": "1234",
	}, log.Contents.Iterator())
	assert.Zero(t, log.Tags.Len())
	assert.Equal(t, uint64(time.UnixMicro(realtime).UnixNano()), log.Timestamp)
	assert.Equal(t, "6", entry.Fields["PRIORITY"])
	assert.Equal(t, "3", entry.Fields["SYSLOG_FACILITY"])
	assert.Equal(t, "old value", entry.Fields["_realtime_timestamp_"])
}
