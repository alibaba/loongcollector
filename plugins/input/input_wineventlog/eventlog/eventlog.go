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
	"context"
	"encoding/json"
	"fmt"
	"github.com/alibaba/ilogtail/pkg/logger"
	"reflect"
	"strconv"
	"time"

	"github.com/elastic/beats/v7/winlogbeat/sys"
)

// Checkpoint represents the state of an individual event log.
type Checkpoint struct {
	Name         string
	RecordNumber uint64
	Timestamp    time.Time
	Bookmark     string
}

// EventLog is an interface to a Windows Event Log.
type EventLog interface {
	// Open the event log. checkpoint points to the last successfully read event
	// in this event log. Read will resume from the next record. To start reading
	// from the first event specify a zero-valued Checkpoint.
	Open(checkpoint Checkpoint) error

	// Read records from the event log.
	Read() ([]Record, error)

	// Close the event log. It should not be re-opened after closing.
	Close() error

	// Name returns the event log's name.
	Name() string
}

// Record represents a single event from the log.
type Record struct {
	sys.Event
	API    string     // The event log API type used to read the record.
	XML    string     // XML representation of the event.
	Offset Checkpoint // Position of the record within its source stream.
}

// ToEvent convert record to map[string]string.
// @ignoreZero controls if zero value will be kept.
func (e *Record) ToEvent(ignoreZero bool) map[string]string {
	m := map[string]string{
		"type":          e.API,
		"log_name":      e.Channel,
		"source_name":   e.Provider.Name,
		"computer_name": e.Computer,
		"record_number": strconv.FormatUint(e.RecordID, 10),
		"event_id":      strconv.FormatUint(uint64(e.EventIdentifier.ID), 10),
	}

	addValue(m, "xml", e.XML, ignoreZero)
	addValue(m, "provider_guid", e.Provider.GUID, ignoreZero)
	addValue(m, "version", e.Version, ignoreZero)
	addValue(m, "level", e.Level, ignoreZero)
	addValue(m, "task", e.Task, ignoreZero)
	addValue(m, "opcode", e.Opcode, ignoreZero)
	addValue(m, "keywords", e.Keywords, ignoreZero)
	addValue(m, "message", sys.RemoveWindowsLineEndings(e.Message), ignoreZero)
	addValue(m, "message_error", e.RenderErr, ignoreZero)

	// Correlation
	addValue(m, "activity_id", e.Correlation.ActivityID, ignoreZero)
	addValue(m, "related_activity_id", e.Correlation.RelatedActivityID, ignoreZero)

	// Execution
	addValue(m, "process_id", e.Execution.ProcessID, ignoreZero)
	addValue(m, "thread_id", e.Execution.ThreadID, ignoreZero)
	addValue(m, "processor_id", e.Execution.ProcessorID, ignoreZero)
	addValue(m, "session_id", e.Execution.SessionID, ignoreZero)
	addValue(m, "kernel_time", e.Execution.KernelTime, ignoreZero)
	addValue(m, "user_time", e.Execution.UserTime, ignoreZero)
	addValue(m, "processor_time", e.Execution.ProcessorTime, ignoreZero)

	if e.User.Identifier != "" {
		addValue(m, "user_identifier", e.User.Identifier, ignoreZero)
		addValue(m, "user_name", e.User.Name, ignoreZero)
		addValue(m, "user_domain", e.User.Domain, ignoreZero)
		addValue(m, "user_type", e.User.Type.String(), ignoreZero)
	}

	addPairs(m, "event_data", e.EventData.Pairs)
	addPairs(m, "user_data", append(e.UserData.Pairs,
		sys.KeyValue{Key: "xml_name", Value: e.UserData.Name.Local}))
	return m
}

// addPairs adds a @key and value (@pairs) to @m by formating @pairs into JSON.
func addPairs(m map[string]string, key string, pairs []sys.KeyValue) {
	sub := make(map[string]string, len(pairs))
	dataIndex := 1
	for _, p := range pairs {
		if "" == p.Key || "Data" == p.Key {
			sub["Data"+strconv.Itoa(dataIndex)] = p.Value
			dataIndex++
		} else {
			sub[p.Key] = p.Value
		}
	}
	val, err := json.Marshal(sub)
	if err != nil {
		logger.Warningf(context.Background(), "WINEVENTLOG_UTIL_ALARM",
			"Call json.Marshal for %v failed %v", sub, err)
		return
	}
	m[key] = string(val)
}

// addValue adds a key and value to the given MapStr if the value is not the
// zero value for the type of v. It is safe to call the function with a nil
// MapStr.
func addValue(m map[string]string, key string, v interface{}, ignoreZero bool) {
	if m != nil && (!ignoreZero || !isZero(v)) {
		m[key] = fmt.Sprint(v)
	}
}

// isZero return true if the given value is the zero value for its type.
func isZero(i interface{}) bool {
	v := reflect.ValueOf(i)
	switch v.Kind() {
	case reflect.Array, reflect.String:
		return v.Len() == 0
	case reflect.Bool:
		return !v.Bool()
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return v.Int() == 0
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
		return v.Uint() == 0
	case reflect.Float32, reflect.Float64:
		return v.Float() == 0
	case reflect.Interface, reflect.Map, reflect.Ptr, reflect.Slice:
		return v.IsNil()
	}
	return false
}
