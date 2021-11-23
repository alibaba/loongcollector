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

package inputsyslog

import (
	"time"
)

type backoff interface {
	// Next returns the duration to wait before next operation.
	Next() time.Duration
	// Reset resets the state of backoff.
	Reset()
	// CanQuit returns to indicate if the retry count is too many.
	// It returns true when it think it's time to stop backoff and quit, otherwise false.
	CanQuit() bool
}

type simpleBackoff struct {
	initial uint
	max     uint
	cur     uint
	canQuit bool
}

func newSimpleBackoff() backoff {
	b := &simpleBackoff{
		initial: 1,
		max:     120,
	}
	b.Reset()
	return b
}

func (b *simpleBackoff) Next() time.Duration {
	if b.cur >= b.max {
		b.canQuit = true
		return time.Duration(b.cur) * time.Second
	}

	duration := time.Duration(b.cur) * time.Second
	b.cur *= 2
	if b.cur >= b.max {
		b.cur = b.max
	}
	return duration
}

func (b *simpleBackoff) Reset() {
	b.cur = b.initial
	b.canQuit = false
}

func (b *simpleBackoff) CanQuit() bool {
	return b.canQuit
}
