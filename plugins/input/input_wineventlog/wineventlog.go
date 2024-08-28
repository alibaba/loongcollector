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

package input_wineventlog

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/input_wineventlog/eventlog"
)

const (
	pluginType = "service_wineventlog"
)

// WinEventLog represents the plugin to collect Windows event logs.
type WinEventLog struct {
	// The name of the event log to monitor.
	// Channel names can also be specifid if running on Windows Vista or newer.
	// By default, the value of Name is Application.
	Name string
	// If this option is specified, plugin filters events that are older than the
	// specified amount of time (in second).
	// This option is useful when you are beginning to monitor an event log that contains
	// older records that you would like to ignore. This field is optional.
	// 0 by default, which means collecting all available logs.
	IgnoreOlder uint
	// A whitelist and blacklist of event IDs.
	// The value is a comma-separated list. The accepted values are single event IDs
	// to include (e.g. 4624), a range of event IDs to include (e.g. 4700-4800), and
	// single event IDs to exclude (e.g. -4735).
	// This option is only available on operating systems supporting the Windows Event
	// Log API (Microsoft Windows Vista and newer).
	// Empty by default, do not filter by event IDs.
	EventID string
	// A list of event levels to include. The value is a comma-separated list of levels.
	// This option is only available on operating systems supporting the Windows Event Log
	// API (Microsoft Windows Vista and newer).
	// Empty by default, which is equivalent to "info,warning,error,critical".
	Level string
	// A list of providers (source names) to include.
	// This option is only available on operating systems supporting the Windows Event Log
	// API (Microsoft Windows Vista and newer).
	// Nil by default, do not filter by provider name.
	Provider []string
	// Ignore zero value, for example, "" for string type, 0 for integer type.
	// False by default.
	IgnoreZeroValue bool
	// Interval (seconds) to wait if Read returns empty. 1 by Default
	WaitInterval uint

	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   pipeline.Context
	collector pipeline.Collector

	checkpoint         eventlog.Checkpoint
	lastCheckpointTime time.Time
	eventLogger        eventlog.EventLog
	logPrefix          string
}

// Init ...
func (w *WinEventLog) Init(context pipeline.Context) (int, error) {
	w.context = context

	if "" == w.Name {
		w.Name = "Application"
	}
	if "" == w.Level {
		w.Level = "information,warning,error,critical"
	}
	w.logPrefix = fmt.Sprintf("WinEventLog[%s]", w.Name)
	var err error
	config := eventlog.EventLogConfig{
		Context:     w.context,
		Name:        w.Name,
		IgnoreOlder: time.Duration(w.IgnoreOlder) * time.Second,
		EventID:     w.EventID,
		Level:       strings.ToLower(w.Level),
		Provider:    w.Provider,
	}
	w.eventLogger, err = eventlog.NewEventLog(config)
	if err != nil {
		return 0, fmt.Errorf("NewEventLog with config %v failed %v",
			config, err)
	}
	return 0, nil
}

// Description ...
func (w *WinEventLog) Description() string {
	return "A service input plugin which collects Windows event logs of " + w.Name
}

// Collect ...
func (w *WinEventLog) Collect(collector pipeline.Collector) error {
	return nil
}

// Start ...
func (w *WinEventLog) Start(collector pipeline.Collector) error {
	w.collector = collector
	w.initCheckpoint()
	w.shutdown = make(chan struct{}, 1)
	w.waitGroup.Add(1)
	defer w.waitGroup.Done()

	for {
		doNotShutdown := w.run()
		if !doNotShutdown {
			break
		}
		logger.Infof(w.context.GetRuntimeContext(), "%s rerun", w.logPrefix)
	}
	w.saveCheckpoint()
	logger.Infof(w.context.GetRuntimeContext(), "%s Quit Start routine, saved checkpoint %v", w.logPrefix, w.checkpoint)
	return nil
}

// Stop ...
func (w *WinEventLog) Stop() error {
	close(w.shutdown)
	logger.Infof(w.context.GetRuntimeContext(), "%s WinEventLog stop wait", w.logPrefix)
	w.waitGroup.Wait()
	logger.Infof(w.context.GetRuntimeContext(), "%s WinEventLog stop done", w.logPrefix)
	return nil
}

func (w *WinEventLog) run() bool {
	err := w.eventLogger.Open(w.checkpoint)
	if err != nil {
		logger.Errorf(w.context.GetRuntimeContext(),
			"WINEVENTLOG_MAIN_ALARM", "%s Open() error: %v, retry this after 60s, checkpoint: %v",
			w.logPrefix, err, w.checkpoint)
		if util.RandomSleep(time.Duration(60)*time.Second, 0.1, w.shutdown) {
			logger.Infof(w.context.GetRuntimeContext(), "%s Break because shutdown was signalled.", w.logPrefix)
			return false
		}
		return true
	}
	defer func() {
		logger.Infof(w.context.GetRuntimeContext(), "%s Stopping %v", w.logPrefix, pluginType)
		err := w.eventLogger.Close()
		if err != nil {
			logger.Warningf(w.context.GetRuntimeContext(), "WINEVENTLOG_MAIN_ALARM", "%s Close() error", w.logPrefix, err)
		}
	}()

	for {
		select {
		case <-w.shutdown:
			logger.Infof(w.context.GetRuntimeContext(), "%s Break from read because shutdown was signalled.", w.logPrefix)
			return false
		default:
		}

		// If the event logger can recovery the error, nil will be returned, so we have
		// to reopen the event logger once Read returns error.
		records, err := w.eventLogger.Read()
		if err != nil {
			logger.Warningf(w.context.GetRuntimeContext(), "WINEVENTLOG_MAIN_ALARM", "%s Read() error: %v, reopen after 60s",
				w.logPrefix, err)
			if util.RandomSleep(time.Duration(60)*time.Second, 0.1, w.shutdown) {
				logger.Infof(w.context.GetRuntimeContext(), "%s Break because shutdown was signalled.", w.logPrefix)
				return false
			}
			return true
		}

		if len(records) == 0 {
			if util.RandomSleep(time.Duration(w.WaitInterval)*time.Second, 0.1, w.shutdown) {
				return false
			}
			continue
		}

		for _, r := range records {
			values := r.ToEvent(w.IgnoreZeroValue)
			w.collector.AddData(values, nil, r.TimeCreated.SystemTime)
			w.checkpoint = r.Offset
			curTime := time.Now()
			if curTime.Sub(w.lastCheckpointTime) > time.Duration(3)*time.Second {
				w.saveCheckpoint()
				w.lastCheckpointTime = curTime
			}
		}
	}
}

func (w *WinEventLog) initCheckpoint() {
	checkpointKey := pluginType + "_" + w.Name
	ok := w.context.GetCheckPointObject(checkpointKey, &w.checkpoint)
	if ok {
		logger.Infof(w.context.GetRuntimeContext(), "%s Checkpoint loaded: %v", w.logPrefix, w.checkpoint)
		return
	}
}

func (w *WinEventLog) saveCheckpoint() {
	checkpointKey := pluginType + "_" + w.Name
	w.context.SaveCheckPointObject(checkpointKey, &w.checkpoint)
}

func newWinEventLog() *WinEventLog {
	return &WinEventLog{
		IgnoreOlder:     0,
		IgnoreZeroValue: false,
		WaitInterval:    1,
	}
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return newWinEventLog()
	}
}
