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

package stdout

import (
	"io"
	"sync"
	"sync/atomic"
	"time"

	"github.com/cihub/seelog"
)

const (
	stdoutBatchLines    = 64
	maxStdoutBatchBytes = 64 << 10
	stdoutPrefixBytes   = len("2006-01-02 15:04:05 ")
)

// stdoutFileBatch owns complete formatted lines. Keeping their boundaries
// lets the rolling writer decide whether to rotate before each line.
type stdoutFileBatch struct {
	data  []byte
	ends  [stdoutBatchLines]int
	lines int
}

func (b *stdoutFileBatch) beginLine() int {
	start := len(b.data)
	var prefix [stdoutPrefixBytes]byte
	b.data = append(b.data, prefix[:]...)
	return start
}

type stdoutFileOutput struct {
	writeError func(error)
	queue      chan *stdoutFileBatch
	done       chan struct{}
	pool       sync.Pool
	mu         sync.RWMutex
	closed     bool
	timestamp  atomic.Pointer[timestampCacheEntry]
}

func newStdoutFileOutput(path string, maxSize, maxRolls int, writeError func(error)) (*stdoutFileOutput, error) {
	// Keep seelog's size-based file writer and its defaults (no archive,
	// numbered postfixes). Only the redundant message formatting is bypassed.
	writer, err := seelog.NewRollingFileWriterSize(path, 0, "", int64(maxSize), maxRolls, 0, false)
	if err != nil {
		return nil, err
	}
	out := &stdoutFileOutput{
		writeError: writeError,
		queue:      make(chan *stdoutFileBatch, seelog.MaxQueueSize/stdoutBatchLines),
		done:       make(chan struct{}),
		pool:       sync.Pool{New: func() any { return new(stdoutFileBatch) }},
	}
	go out.writeQueued(writer)
	return out, nil
}

func (out *stdoutFileOutput) getBatch() *stdoutFileBatch {
	return out.pool.Get().(*stdoutFileBatch)
}

func (out *stdoutFileOutput) recycle(batch *stdoutFileBatch) {
	batch.lines = 0
	if cap(batch.data) > 2*maxStdoutBatchBytes {
		batch.data = nil
	} else {
		batch.data = batch.data[:0]
	}
	out.pool.Put(batch)
}

func (out *stdoutFileOutput) finishLine(batch *stdoutFileBatch, start int) *stdoutFileBatch {
	text := stdoutTimestamp(&out.timestamp, time.Now()).text
	// Normal wall-clock years use exactly 19 bytes. Keep the formatter's
	// extended-year spelling too, without penalizing the common path.
	prefixBytes := len(text) + 1
	if prefixBytes != stdoutPrefixBytes {
		oldEnd := len(batch.data)
		if prefixBytes > stdoutPrefixBytes {
			batch.data = append(batch.data, text[:prefixBytes-stdoutPrefixBytes]...)
		}
		copy(batch.data[start+prefixBytes:], batch.data[start+stdoutPrefixBytes:oldEnd])
		batch.data = batch.data[:oldEnd+prefixBytes-stdoutPrefixBytes]
	}
	copy(batch.data[start:], text)
	batch.data[start+len(text)] = ' '
	batch.data = append(batch.data, '\n')
	batch.ends[batch.lines] = len(batch.data)
	batch.lines++
	if batch.lines == stdoutBatchLines || len(batch.data) >= maxStdoutBatchBytes {
		out.enqueue(batch)
		return out.getBatch()
	}
	return batch
}

func (out *stdoutFileOutput) enqueue(batch *stdoutFileBatch) {
	if batch.lines == 0 {
		out.recycle(batch)
		return
	}
	out.mu.RLock()
	defer out.mu.RUnlock()
	if out.closed {
		out.recycle(batch)
		return
	}
	out.queue <- batch
}

func (out *stdoutFileOutput) writeQueued(writer io.WriteCloser) {
	defer close(out.done)
	for batch := range out.queue {
		start := 0
		for _, end := range batch.ends[:batch.lines] {
			if _, err := writer.Write(batch.data[start:end]); err != nil {
				out.writeError(err)
			}
			start = end
		}
		out.recycle(batch)
	}
	if err := writer.Close(); err != nil {
		out.writeError(err)
	}
}

func (out *stdoutFileOutput) close() {
	out.mu.Lock()
	if !out.closed {
		out.closed = true
		close(out.queue)
	}
	out.mu.Unlock()
	<-out.done
}
