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
	"io"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/coreos/go-systemd/sdjournal"
)

// SD_JOURNAL_FIELD_CATALOG_ENTRY stores the name of the JournalEntry field to export Catalog entry to.
const SD_JOURNAL_FIELD_CATALOG_ENTRY = "CATALOG_ENTRY"

// Follow follows the journald and writes the entries to the output channel
// It is a slightly reworked version of sdjournal.Follow to fit our needs.
func (sj *ServiceJournal) Follow(journal *sdjournal.Journal, stop <-chan struct{}) <-chan *sdjournal.JournalEntry {
	readEntry := func(journal *sdjournal.Journal) (*sdjournal.JournalEntry, error) {
		c, err := journal.Next()
		if err != nil {
			return nil, err
		}

		if c == 0 {
			return nil, io.EOF
		}

		entry, err := journal.GetEntry()
		if err != nil {
			return nil, err
		}
		return entry, nil
	}

	out := make(chan *sdjournal.JournalEntry, 10)

	go func(journal *sdjournal.Journal, stop <-chan struct{}, out chan<- *sdjournal.JournalEntry) {
		defer func() {
			logger.Info(sj.context.GetRuntimeContext(), "receive stop signal", "return journal follow")
			close(out)
		}()
		eventWaitCh := make(chan int)

	process:
		for {
			select {
			case <-stop:
				return
			default:
			}

			entry, err := readEntry(journal)
			if err != nil && err != io.EOF {
				if cursor, cerr := journal.GetCursor(); cerr != nil {
					logger.Warningf(sj.context.GetRuntimeContext(), "JOURNAL_READ_ALARM", "Received unknown error when reading a new entry: %v, cursor read error: %v", err, cerr)
				} else {
					logger.Warningf(sj.context.GetRuntimeContext(), "JOURNAL_READ_ALARM", "Received unknown error when reading a new entry: %v, cursor: %s", err, cursor)
				}
				util.RandomSleep(time.Second*5, 0.1, stop)
				continue
			}

			if entry != nil {
				if _, ok := entry.Fields[sdjournal.SD_JOURNAL_FIELD_MESSAGE_ID]; ok {
					if catalogEntry, err := journal.GetCatalog(); err == nil {
						entry.Fields[SD_JOURNAL_FIELD_CATALOG_ENTRY] = catalogEntry
					}
				}
				// non-blocking return
				select {
				case <-stop:
					return
				case out <- entry:
					continue process
				}
			}

			// We're at the tail, so wait for new events or time out.
			// Holds journal events to process. Tightly bounded for now unless there's a
			// reason to unblock the journal watch routine more quickly.
			for {
				go func() {
					select {
					case <-stop:
					case eventWaitCh <- journal.Wait(300 * time.Millisecond):
					}
				}()

				select {
				case <-stop:
					return
				case e := <-eventWaitCh:
					switch e {
					case sdjournal.SD_JOURNAL_NOP:
						// the journal did not change since the last invocation
					case sdjournal.SD_JOURNAL_APPEND, sdjournal.SD_JOURNAL_INVALIDATE:
						continue process
					case -9:
						continue process
					default:
						logger.Warningf(sj.context.GetRuntimeContext(), "JOURNAL_READ_ALARM", "Received unknown event: %d", e)
						util.RandomSleep(time.Second*5, 0.1, stop)
					}
				}
			}
		}
	}(journal, stop, out)

	return out
}
