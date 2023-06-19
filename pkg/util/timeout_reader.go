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

package util

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"runtime"
	"time"
)

const BufferSize = 4096

var ErrReaderTimeout = errors.New("timeout")

type TimeoutReader struct {
	b  *bufio.Reader
	t  time.Duration
	ch <-chan error
}

func NewTimeoutReader(r io.Reader, to time.Duration) *TimeoutReader {
	return &TimeoutReader{b: bufio.NewReaderSize(r, BufferSize), t: to}
}

func (r *TimeoutReader) Read(b []byte) (n int, err error) {
	if r.ch == nil {
		if r.t < 0 || r.b.Buffered() > 0 {
			return r.b.Read(b)
		}
		ch := make(chan error, 1)
		r.ch = ch
		go func() {
			_, err = r.b.Peek(1)
			ch <- err
		}()
		runtime.Gosched()
	}
	if r.t < 0 {
		err = <-r.ch // Block
	} else {
		select {
		case err = <-r.ch: // Poll
		default:
			if r.t == 0 {
				return 0, ErrReaderTimeout
			}
			select {
			case err = <-r.ch: // Timeout
			case <-time.After(r.t):
				return 0, ErrReaderTimeout
			}
		}
	}
	r.ch = nil
	if r.b.Buffered() > 0 {
		n, _ = r.b.Read(b)
	}
	return
}

func DoFuncWithTimeout(timeout time.Duration, workFunc func() error) error {
	ch := make(chan error, 1)
	go func() {
		ch <- workFunc()
	}()
	select {
	case err := <-ch:
		return err
	case <-time.After(timeout):
		return fmt.Errorf("operation timed out after %v", timeout)
	}
}
