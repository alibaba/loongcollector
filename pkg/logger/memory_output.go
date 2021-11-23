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

package logger

import (
	"sync"

	"github.com/cihub/seelog"
)

const maxLines = 1000

var cursor int
var memLogs = make([]string, maxLines)
var rwLocker sync.RWMutex

// ReadMemoryLog get log when MemoryWriter working.
func ReadMemoryLog(line int) (msg string, ok bool) {
	rwLocker.RLock()
	defer rwLocker.RUnlock()
	if line > 0 && line <= cursor {
		msg = memLogs[(line-1)%maxLines]
		ok = true
	}
	return
}

// ClearMemoryLog clear the whole memory logs.
func ClearMemoryLog() {
	rwLocker.Lock()
	defer rwLocker.Unlock()
	memLogs = make([]string, maxLines)
	cursor = 0
}

// GetMemoryLogCount returns the stored logs count, but only store the recently maxLines items.
func GetMemoryLogCount() int {
	rwLocker.RLock()
	defer rwLocker.RUnlock()
	return cursor
}

// MemoryWriter provide a easy way to check log in testing.
type MemoryWriter struct {
}

func (m *MemoryWriter) ReceiveMessage(message string, level seelog.LogLevel, context seelog.LogContextInterface) error {
	rwLocker.Lock()
	defer rwLocker.Unlock()
	memLogs[cursor%maxLines] = message
	cursor++
	return nil
}

func (m *MemoryWriter) AfterParse(initArgs seelog.CustomReceiverInitArgs) error {
	return nil
}

func (m *MemoryWriter) Flush() {
}

func (m *MemoryWriter) Close() error {
	rwLocker.Lock()
	defer rwLocker.Unlock()
	memLogs = make([]string, maxLines)
	cursor = 0
	return nil
}

func init() {
	seelog.RegisterReceiver("memory", new(MemoryWriter))
}
