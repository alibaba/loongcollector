//go:build linux
// +build linux

// Copyright 2017 Marcus Heese
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This code based on logic from journalctl unit filter. i.e. journalctl -u in
// the systemd source code.
// See: https://github.com/systemd/systemd/blob/master/src/journal/journalctl.c#L1410
// and https://github.com/systemd/systemd/blob/master/src/basic/unit-name.c

package journal

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/coreos/go-systemd/sdjournal"
)

// Named constants for the journal cursor placement positions
const (
	SeekPositionCursor  = "cursor"
	SeekPositionHead    = "head"
	SeekPositionTail    = "tail"
	SeekPositionDefault = "none"
)

const (
	checkPointKey        = "journal_v1"
	defaultResetInterval = 60 * 60
)

// SyslogFacilityString is a map containing the textual equivalence of a given facility number
var SyslogFacilityString = map[string]string{
	"0":  "kernel",
	"1":  "user",
	"2":  "mail",
	"3":  "daemon",
	"4":  "auth",
	"5":  "syslog",
	"6":  "line printer",
	"7":  "network news",
	"8":  "uucp",
	"9":  "clock daemon",
	"10": "security/auth",
	"11": "ftp",
	"12": "ntp",
	"13": "log audit",
	"14": "log alert",
	"15": "clock daemon",
	"16": "local0",
	"17": "local1",
	"18": "local2",
	"19": "local3",
	"20": "local4",
	"21": "local5",
	"22": "local6",
	"23": "local7",
}

// PriorityConversionMap is a map containing the textual equivalence of a given priority string number
var PriorityConversionMap = map[string]string{
	"0": "emergency",
	"1": "alert",
	"2": "critical",
	"3": "error",
	"4": "warning",
	"5": "notice",
	"6": "informational",
	"7": "debug",
}

type ServiceJournal struct {
	SeekPosition        string
	CursorFlushPeriodMs int
	CursorSeekFallback  string
	Units               []string
	Kernel              bool
	Identifiers         []string
	JournalPaths        []string
	MatchPatterns       []string
	ParseSyslogFacility bool
	ParsePriority       bool
	UseJournalEventTime bool
	ResetIntervalSecond int

	journal        *sdjournal.Journal
	lastSaveCPTime time.Time
	lastCPCursor   string

	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   ilogtail.Context
}

func (sj *ServiceJournal) SaveCheckpoint(forceFlag bool) {
	if !forceFlag {
		nowTime := time.Now()
		if nowTime.Sub(sj.lastSaveCPTime).Nanoseconds() < int64(sj.CursorFlushPeriodMs)*1000000 {
			return
		}
		sj.lastSaveCPTime = nowTime
	}
	if sj.journal == nil {
		return
	}
	if sj.ResetIntervalSecond <= 0 {
		sj.ResetIntervalSecond = defaultResetInterval
	}
	if cursor, err := sj.journal.GetCursor(); err != nil {
		logger.Error(sj.context.GetRuntimeContext(), "SAVE_CHECKPOINT_ALARM", "get cursor error", err)
	} else if err := sj.context.SaveCheckPoint(checkPointKey, []byte(cursor)); err != nil {
		logger.Error(sj.context.GetRuntimeContext(), "SAVE_CHECKPOINT_ALARM", "save checkpoint error", err)
	}
}

func (sj *ServiceJournal) LoadCheckpoint() {
	if val, ok := sj.context.GetCheckPoint(checkPointKey); ok {
		sj.lastCPCursor = string(val)
	}
}

func (sj *ServiceJournal) Init(context ilogtail.Context) (int, error) {
	sj.context = context
	return 0, nil
}

func (sj *ServiceJournal) addKernel() error {
	if len(sj.Units) > 0 && sj.Kernel {
		err := sj.addMatchesForKernel()
		if err != nil {
			return fmt.Errorf("Adding filter for kernel failed: %v", err)
		}
	}
	return nil
}

func (sj *ServiceJournal) addMatchesForKernel() error {
	err := sj.journal.AddMatch("_TRANSPORT=kernel")
	if err != nil {
		return err
	}
	return sj.journal.AddDisjunction()
}

// Add syslog identifiers to monitor
func (sj *ServiceJournal) addSyslogIdentifiers() error {
	var err error

	for _, identifier := range sj.Identifiers {
		if err = sj.journal.AddMatch(sdjournal.SD_JOURNAL_FIELD_SYSLOG_IDENTIFIER + "=" + identifier); err != nil {
			return fmt.Errorf("Filtering syslog identifier %s failed: %v", identifier, err)
		}

		if err = sj.journal.AddDisjunction(); err != nil {
			return fmt.Errorf("Filtering syslog identifier %s failed: %v", identifier, err)
		}
	}

	return nil
}

func (sj *ServiceJournal) Description() string {
	return "journal input plugin for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (sj *ServiceJournal) Collect(ilogtail.Collector) error {
	return nil
}

func (sj *ServiceJournal) initJournal() error {
	var err error
	seekToHelper := func(position string, err error) error {
		if err == nil {
			logger.Infof(sj.context.GetRuntimeContext(), "Seek to [%s] successful", position)
		} else {
			logger.Warningf(sj.context.GetRuntimeContext(), "JOURNAL_SEEK_ALARM", "Could not seek to %s: %v", position, err)
		}
		return err
	}

	// connect to the Systemd Journal
	switch len(sj.JournalPaths) {
	case 0:
		if sj.journal, err = sdjournal.NewJournal(); err != nil {
			return err
		}
	case 1:
		fi, errStat := os.Stat(sj.JournalPaths[0])
		if errStat != nil {
			return errStat
		}
		if fi.IsDir() {
			if sj.journal, err = sdjournal.NewJournalFromDir(sj.JournalPaths[0]); err != nil {
				return err
			}
		} else {
			if sj.journal, err = sdjournal.NewJournalFromFiles(sj.JournalPaths...); err != nil {
				return err
			}
		}
	default:
		if sj.journal, err = sdjournal.NewJournalFromFiles(sj.JournalPaths...); err != nil {
			return err
		}
	}
	logger.Info(sj.context.GetRuntimeContext(), "journal", "open success")

	// add specific units to monitor if any
	if err = sj.addUnits(); err != nil {
		return err
	}

	// add specific patterns to monitor if any
	for _, pattern := range sj.MatchPatterns {
		err = sj.journal.AddMatch(pattern)
		if err == nil {
			err = sj.journal.AddDisjunction()
		}

		if err != nil {
			return fmt.Errorf("Filtering pattern %s failed: %v", pattern, err)
		}
	}

	// add kernel logs
	if err = sj.addKernel(); err != nil {
		return err
	}

	// add syslog identifiers to monitor if any
	if err = sj.addSyslogIdentifiers(); err != nil {
		return err
	}

	sj.LoadCheckpoint()
	if len(sj.lastCPCursor) == 0 {
		switch sj.SeekPosition {
		case SeekPositionHead:
			err = seekToHelper(SeekPositionHead, sj.journal.SeekHead())
		default:
			err = seekToHelper(SeekPositionTail, sj.journal.SeekTail())
		}
	} else {
		_ = seekToHelper(sj.lastCPCursor, sj.journal.SeekCursor(sj.lastCPCursor))
		_, err = sj.journal.Next()
		if err != nil {
			logger.Warning(sj.context.GetRuntimeContext(), "JOURNAL_READ_ALARM", "call next when init error", err)
			err = nil
		}
	}
	if err != nil {
		return fmt.Errorf("Seeking to a good position in journal failed: %v", err)
	}
	return nil
}

// Start starts the ServiceInput's service, whatever that may be
func (sj *ServiceJournal) Start(c ilogtail.Collector) error {
	sj.shutdown = make(chan struct{})
	sj.waitGroup.Add(1)
	defer sj.waitGroup.Done()

RunLoop:
	for {
		if err := sj.initJournal(); err != nil {
			return err
		}

		runShutdown := make(chan struct{})
		var runWaitGroup sync.WaitGroup
		runWaitGroup.Add(1)
		go sj.run(c, runShutdown, &runWaitGroup)

		t := time.NewTimer(time.Second * time.Duration(sj.ResetIntervalSecond))
		select {
		case <-t.C:
		case <-sj.shutdown:
		}
		close(runShutdown)
		runWaitGroup.Wait()
		t.Stop()

		select {
		case <-sj.shutdown:
			break RunLoop
		default:
			logger.Infof(sj.context.GetRuntimeContext(), "restart journal to release memory, interval: %vs", sj.ResetIntervalSecond)
		}
	}

	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (sj *ServiceJournal) Stop() error {
	close(sj.shutdown)
	sj.waitGroup.Wait()
	return nil
}

func (sj *ServiceJournal) run(c ilogtail.Collector, shutdown chan struct{}, wg *sync.WaitGroup) {
	defer func() {
		sj.SaveCheckpoint(true)
		logger.Info(sj.context.GetRuntimeContext(), "journal", "start close")
		_ = sj.journal.Close()
		logger.Info(sj.context.GetRuntimeContext(), "journal", "close success")
		sj.journal = nil
		wg.Done()
	}()

	columns := []string{"_realtime_timestamp_", "_monotonic_timestamp_"}
	values := []string{"", ""}
	for rawEvent := range sj.Follow(sj.journal, shutdown) {
		// type JournalEntry struct {
		// 	Fields             map[string]string
		// 	Cursor             string
		// 	RealtimeTimestamp  uint64
		// 	MonotonicTimestamp uint64
		// }
		// logger.Debug("on journal event", *rawEvent)

		if sj.ParsePriority {
			if val, ok := rawEvent.Fields[sdjournal.SD_JOURNAL_FIELD_PRIORITY]; ok {
				rawEvent.Fields[sdjournal.SD_JOURNAL_FIELD_PRIORITY] = PriorityConversionMap[val]
			}
		}
		if sj.ParseSyslogFacility {
			if val, ok := rawEvent.Fields[sdjournal.SD_JOURNAL_FIELD_SYSLOG_FACILITY]; ok {
				rawEvent.Fields[sdjournal.SD_JOURNAL_FIELD_SYSLOG_FACILITY] = SyslogFacilityString[val]
			}
		}

		var eventTime time.Time
		if sj.UseJournalEventTime {
			eventTime = time.Unix(0, int64(rawEvent.RealtimeTimestamp)*1000)
		} else {
			eventTime = time.Now()
		}
		values[0] = strconv.FormatUint(rawEvent.RealtimeTimestamp, 10)
		values[1] = strconv.FormatUint(rawEvent.MonotonicTimestamp, 10)
		c.AddDataArray(rawEvent.Fields, columns, values, eventTime)
		sj.SaveCheckpoint(false)
	}
	logger.Info(sj.context.GetRuntimeContext(), "service journal sync", "done")
}

func init() {
	ilogtail.ServiceInputs["service_journal"] = func() ilogtail.ServiceInput {
		return &ServiceJournal{
			SeekPosition:        SeekPositionTail,
			CursorFlushPeriodMs: 5000,
			CursorSeekFallback:  SeekPositionTail,
			Kernel:              true,
			ResetIntervalSecond: defaultResetInterval,
		}
	}
}
