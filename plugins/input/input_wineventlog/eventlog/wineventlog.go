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
	"github.com/alibaba/ilogtail/pkg/logger"
	"io"
	"syscall"

	"github.com/elastic/beats/v7/winlogbeat/sys"
	win "github.com/elastic/beats/v7/winlogbeat/sys/wineventlog"
	"golang.org/x/sys/windows"
)

const (
	winEventLogAPIName = "wineventlog"
	batchReadSize      = 100
	// renderBufferSize is the size in bytes of the buffer used to render events.
	renderBufferSize = 1 << 14
)

type winEventLog struct {
	config       EventLogConfig
	query        string
	subscription win.EvtHandle
	checkpoint   Checkpoint
	logPrefix    string

	render      func(event win.EvtHandle, out io.Writer) error // Function for rendering the event to XML.
	renderBuf   []byte                                         // Buffer used for rendering event.
	outputBuf   *sys.ByteBuffer                                // Buffer for receiving XML.
	cache       *messageFilesCache                             // Cached mapping of source name to event message file handles.
	signalEvent windows.Handle
}

func (w *winEventLog) Open(checkpoint Checkpoint) error {
	var bookmark win.EvtHandle
	var err error
	if len(checkpoint.Bookmark) > 0 {
		bookmark, err = win.CreateBookmarkFromXML(checkpoint.Bookmark)
	} else {
		bookmark, err = win.CreateBookmarkFromRecordID(w.config.Name, checkpoint.RecordNumber)
	}
	if err != nil {
		return err
	}
	defer win.Close(bookmark)

	// Using a pull subscription to receive events. See:
	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa385771(v=vs.85).aspx#pull
	signalEvent, err := windows.CreateEvent(nil, 0, 0, nil)
	if err != nil {
		logger.Warningf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
			"%s Create event error: %v", w.logPrefix, err)
		return err
	}

	logger.Debugf(w.config.Context.GetRuntimeContext(), "%s using subscription query=%s", w.logPrefix, w.query)
	subscriptionHandle, err := win.Subscribe(
		0, // Session - nil for localhost
		signalEvent,
		"",       // Channel - empty b/c channel is in the query
		w.query,  // Query - nil means all events
		bookmark, // Bookmark - for resuming from a specific event
		win.EvtSubscribeStartAfterBookmark)
	if err != nil {
		windows.Close(signalEvent)
		return err
	}

	w.signalEvent = signalEvent
	w.subscription = subscriptionHandle
	return nil
}

func (w *winEventLog) Read() ([]Record, error) {
	handles, _, err := w.eventHandles(batchReadSize)
	if err != nil || len(handles) == 0 {
		return nil, err
	}
	defer func() {
		for _, h := range handles {
			win.Close(h)
		}
	}()
	logger.Debugf(w.config.Context.GetRuntimeContext(), "%s EventHandles returned %d handles", w.logPrefix, len(handles))

	var records []Record
	for _, h := range handles {
		w.outputBuf.Reset()
		err := w.render(h, w.outputBuf)
		if bufErr, ok := err.(sys.InsufficientBufferError); ok {
			logger.Debugf(w.config.Context.GetRuntimeContext(), "%s Increasing render buffer size to %d", w.logPrefix,
				bufErr.RequiredSize)
			w.renderBuf = make([]byte, bufErr.RequiredSize)
			w.outputBuf.Reset()
			err = w.render(h, w.outputBuf)
		}
		if err != nil && w.outputBuf.Len() == 0 {
			logger.Errorf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
				"%s Dropping event with rendering error. %v", w.logPrefix, err)
			continue
		}

		r, err := w.buildRecordFromXML(w.outputBuf.Bytes(), err)
		if err != nil {
			logger.Errorf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
				"%s Dropping event. %v", w.logPrefix, err)
			continue
		}

		r.Offset = Checkpoint{
			Name:         w.config.Name,
			RecordNumber: r.RecordID,
			Timestamp:    r.TimeCreated.SystemTime,
		}
		if r.Offset.Bookmark, err = w.createBookmarkFromEvent(h); err != nil {
			logger.Warningf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
				"%s failed creating bookmark: %v", w.logPrefix, err)
		}
		records = append(records, r)
		w.checkpoint = r.Offset
	}

	logger.Debugf(w.config.Context.GetRuntimeContext(), "%s Read() is returning %d records", w.logPrefix, len(records))
	return records, nil
}

func (w *winEventLog) Close() error {
	if err := windows.Close(w.signalEvent); err != nil {
		logger.Warningf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
			"%s Close signal event error: %v", w.logPrefix, err)
	}
	w.signalEvent = 0
	return win.Close(w.subscription)
}

func (w *winEventLog) Name() string {
	return w.config.Name
}

func (w *winEventLog) eventHandles(maxRead int) ([]win.EvtHandle, int, error) {
	handles, err := win.EventHandles(w.subscription, batchReadSize)
	switch err {
	case nil:
		if batchReadSize > maxRead {
			logger.Debugf(w.config.Context.GetRuntimeContext(), "%s Recovered from RPC_S_INVALID_BOUND error (errno 1734) "+
				"by decreasing batch_read_size to %v", w.logPrefix, maxRead)
		}
		return handles, maxRead, nil
	case win.ERROR_NO_MORE_ITEMS:
		logger.Debugf(w.config.Context.GetRuntimeContext(), "%s No more events", w.logPrefix)
		return nil, maxRead, nil
	case win.RPC_S_INVALID_BOUND:
		if err := w.Close(); err != nil {
			return nil, 0, fmt.Errorf("failed to recover from RPC_S_INVALID_BOUND: %v", err)
		}
		if err := w.Open(w.checkpoint); err != nil {
			return nil, 0, fmt.Errorf("failed to recover from RPC_S_INVALID_BOUND: %v", err)
		}
		return w.eventHandles(maxRead / 2)
	default:
		logger.Warningf(w.config.Context.GetRuntimeContext(), "WINEVENTLOG_API_ALARM",
			"%s EventHandles returned error %v", w.logPrefix, err)
		return nil, 0, err
	}
}

func (w *winEventLog) buildRecordFromXML(x []byte, recoveredErr error) (Record, error) {
	e, err := sys.UnmarshalEventXML(x)
	if err != nil {
		return Record{}, fmt.Errorf("Failed to unmarshal XML='%s'. %v", x, err)
	}

	err = sys.PopulateAccount(&e.User)
	if err != nil {
		logger.Debugf(w.config.Context.GetRuntimeContext(), "%s SID %s account lookup failed. %v", w.logPrefix,
			e.User.Identifier, err)
	}

	if e.RenderErrorCode != 0 {
		// Convert the render error code to an error message that can be
		// included in the "message_error" field.
		e.RenderErr = append(e.RenderErr, syscall.Errno(e.RenderErrorCode).Error())
	} else if recoveredErr != nil {
		e.RenderErr = append(e.RenderErr, recoveredErr.Error())
	}

	if e.Level == "" {
		// Fallback on LevelRaw if the Level is not set in the RenderingInfo.
		e.Level = win.EventLevel(e.LevelRaw).String()
	}

	logger.Debugf(w.config.Context.GetRuntimeContext(), "%s XML=%s Event=%+v", w.logPrefix, string(x), e)

	r := Record{
		API:   winEventLogAPIName,
		Event: e,
		XML:   string(x),
	}
	return r, nil
}

func (w *winEventLog) createBookmarkFromEvent(evtHandle win.EvtHandle) (string, error) {
	bmHandle, err := win.CreateBookmarkFromEvent(evtHandle)
	if err != nil {
		return "", err
	}
	w.outputBuf.Reset()
	err = win.RenderBookmarkXML(bmHandle, w.renderBuf, w.outputBuf)
	win.Close(bmHandle)
	return string(w.outputBuf.Bytes()), err
}

func newWinEventLog(config EventLogConfig) (EventLog, error) {
	query, err := win.Query{
		Log:         config.Name,
		IgnoreOlder: config.IgnoreOlder,
		Level:       config.Level,
		EventID:     config.EventID,
		Provider:    config.Provider,
	}.Build()
	if err != nil {
		return nil, err
	}

	eventMetadataHandle := func(providerName, sourceName string) sys.MessageFiles {
		mf := sys.MessageFiles{SourceName: sourceName}
		h, err := win.OpenPublisherMetadata(0, sourceName, 0)
		if err != nil {
			mf.Err = err
			return mf
		}

		mf.Handles = []sys.FileHandle{{Handle: uintptr(h)}}
		return mf
	}
	freeHandle := func(handle uintptr) error {
		return win.Close(win.EvtHandle(handle))
	}

	w := &winEventLog{
		config:    config,
		query:     query,
		renderBuf: make([]byte, renderBufferSize),
		outputBuf: sys.NewByteBuffer(renderBufferSize),
		cache:     newMessageFilesCache(config.Name, eventMetadataHandle, freeHandle),
		logPrefix: fmt.Sprintf("WinEventLog[%s]", config.Name),
	}

	// Forwarded events should be rendered using RenderEventXML. It is more
	// efficient and does not attempt to use local message files for rendering
	// the event's message.
	switch {
	case "ForwardedEvents" == config.Name:
		logger.Infof(w.config.Context.GetRuntimeContext(), "%s Use RenderEventXML.", w.logPrefix)
		w.render = func(event win.EvtHandle, out io.Writer) error {
			return win.RenderEventXML(event, w.renderBuf, out)
		}
	default:
		logger.Infof(w.config.Context.GetRuntimeContext(), "%s Use RenderEvent.", w.logPrefix)
		w.render = func(event win.EvtHandle, out io.Writer) error {
			return win.RenderEvent(event, 0, w.renderBuf, w.cache.get, out)
		}
	}

	return w, nil
}

func init() {
	if available, _ := win.IsAvailable(); available {
		Register(winEventLogAPIName, newWinEventLog)
	}
}
