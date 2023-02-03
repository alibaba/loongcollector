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

//go:build windows
// +build windows

package eventlog

import (
	"fmt"
	"syscall"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/elastic/beats/v7/winlogbeat/sys"
	win "github.com/elastic/beats/v7/winlogbeat/sys/eventlogging"
)

const (
	// Since Windows 2000.
	eventLoggingAPIName = "eventlogging"
)

type eventLogging struct {
	context     pipeline.Context
	name        string // Name of the log that is opened.
	ignoreOlder time.Duration
	handle      win.Handle         // Handle to the event log.
	readBuf     []byte             // Buffer for reading in events.
	formatBuf   []byte             // Buffer for formatting messages.
	insertBuf   win.StringInserts  // Buffer for parsing insert strings.
	handles     *messageFilesCache // Cached mapping of source name to event message file handles.
	logPrefix   string             // Prefix to add to all log entries.

	recordNumber uint32 // First record number to read.
	seek         bool   // Read should use seek.
	ignoreFirst  bool   // Ignore first message returned from a read.
}

// Open ...
func (e *eventLogging) Open(checkpoint Checkpoint) error {
	handle, err := win.OpenEventLog("", e.name)
	if err != nil {
		return err
	}

	numRecords, err := win.GetNumberOfEventLogRecords(handle)
	if err != nil {
		return err
	}

	var oldestRecord, newestRecord uint32
	if numRecords > 0 {
		e.recordNumber = uint32(checkpoint.RecordNumber)
		e.seek = true
		e.ignoreFirst = true

		oldestRecord, err = win.GetOldestEventLogRecord(handle)
		if err != nil {
			return err
		}
		newestRecord = oldestRecord + numRecords - 1

		if e.recordNumber < oldestRecord || e.recordNumber > newestRecord {
			e.recordNumber = oldestRecord
			e.ignoreFirst = false
		}
	} else {
		e.recordNumber = 0
		e.seek = false
		e.ignoreFirst = false
	}

	logger.Infof(e.context.GetRuntimeContext(), "%v contains %v records. Record number range [%v, %v]. Starting "+
		"at %v (ignoringFirst=%v)", e.logPrefix, numRecords, oldestRecord,
		newestRecord, e.recordNumber, e.ignoreFirst)
	e.handle = handle
	return nil
}

// Read ...
func (e *eventLogging) Read() ([]Record, error) {
	flags := win.EVENTLOG_SEQUENTIAL_READ | win.EVENTLOG_FORWARDS_READ
	if e.seek {
		flags = win.EVENTLOG_SEEK_READ | win.EVENTLOG_FORWARDS_READ
		e.seek = false
	}

	var numBytesRead int
	err := retry(
		func() error {
			e.readBuf = e.readBuf[0:cap(e.readBuf)]
			// TODO: Use number of bytes to grow the buffer size as needed.
			var err error
			numBytesRead, err = win.ReadEventLog(
				e.handle,
				flags,
				e.recordNumber,
				e.readBuf)
			return err
		},
		e.readRetryErrorHandler)
	if err != nil {
		logger.Debugf(e.context.GetRuntimeContext(), "%v ReadEventLog returned error %v", e.logPrefix, err)
		return readErrorHandler(err)
	}

	e.readBuf = e.readBuf[0:numBytesRead]
	events, _, err := win.RenderEvents(
		e.readBuf[:numBytesRead], 0, e.formatBuf, &e.insertBuf, e.handles.get)
	if err != nil {
		return nil, err
	}

	records := make([]Record, 0, len(events))
	for _, evt := range events {
		// The events do not contain the name of the event log so we must add
		// the name of the log from which we are reading.
		evt.Channel = e.name

		err = sys.PopulateAccount(&evt.User)
		if err != nil {
			logger.Debugf(e.context.GetRuntimeContext(), "%v SID %v account lookup failed: %v", e.logPrefix,
				evt.User.Identifier, err)
		}

		records = append(records, Record{
			API:   eventLoggingAPIName,
			Event: evt,
			Offset: Checkpoint{
				Name:         e.name,
				RecordNumber: evt.RecordID,
				Timestamp:    evt.TimeCreated.SystemTime,
			},
		})
	}

	if e.ignoreFirst && len(records) > 0 {
		logger.Debugf(e.context.GetRuntimeContext(), "%v Ignoring first event with record ID %v", e.logPrefix,
			records[0].RecordID)
		records = records[1:]
		e.ignoreFirst = false
	}

	records = filter(records, func(r *Record) bool {
		if e.ignoreOlder != 0 && time.Since(r.TimeCreated.SystemTime) > e.ignoreOlder {
			return false
		}
		return true
	})
	logger.Debugf(e.context.GetRuntimeContext(), "%v Read() is returning %v records", e.logPrefix, len(records))
	return records, nil
}

func (e *eventLogging) Close() error {
	logger.Debugf(e.context.GetRuntimeContext(), "%v Closing handle", e.logPrefix)
	return win.CloseEventLog(e.handle)
}

func (e *eventLogging) Name() string {
	return e.name
}

// readRetryErrorHandler handles errors returned from the readEventLog function
// by attempting to correct the error through closing and reopening the event
// log.
func (e *eventLogging) readRetryErrorHandler(err error) error {
	if errno, ok := err.(syscall.Errno); ok {
		var reopen bool

		switch errno {
		case win.ERROR_EVENTLOG_FILE_CHANGED:
			logger.Debug(e.context.GetRuntimeContext(), "Re-opening event log because event log file was changed")
			reopen = true
		case win.ERROR_EVENTLOG_FILE_CORRUPT:
			logger.Debug(e.context.GetRuntimeContext(), "Re-opening event log because event log file is corrupt")
			reopen = true
		}

		if reopen {
			e.Close()
			return e.Open(Checkpoint{
				Name:         e.name,
				RecordNumber: uint64(e.recordNumber),
			})
		}
	}
	return err
}

// Filter returns a new slice holding only the elements of s that satisfy the
// predicate fn().
func filter(in []Record, fn func(*Record) bool) []Record {
	var out []Record
	for _, r := range in {
		if fn(&r) {
			out = append(out, r)
		}
	}
	return out
}

// readErrorHandler handles errors returned by the readEventLog function.
func readErrorHandler(err error) ([]Record, error) {
	switch err {
	case syscall.ERROR_HANDLE_EOF,
		win.ERROR_EVENTLOG_FILE_CHANGED,
		win.ERROR_EVENTLOG_FILE_CORRUPT:
		return []Record{}, nil
	}
	return nil, err
}

func newEventLogging(config EventLogConfig) (EventLog, error) {
	return &eventLogging{
		context:     config.Context,
		name:        config.Name,
		ignoreOlder: config.IgnoreOlder,
		readBuf:     make([]byte, 0, win.MaxEventBufferSize),
		formatBuf:   make([]byte, win.MaxFormatMessageBufferSize),
		handles: newMessageFilesCache(config.Name, win.QueryEventMessageFiles,
			win.FreeLibrary),
		logPrefix: fmt.Sprintf("EventLogging[%s]", config.Name),
	}, nil
}

func init() {
	if available, _ := win.IsAvailable(); available {
		Register(eventLoggingAPIName, newEventLogging)
	}
}
