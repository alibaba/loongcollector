// Copyright 2023 iLogtail Authors
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

package async

import (
	"fmt"
	"sync/atomic"
	"time"
)

type UnittestCounter interface {
	Begin(callback func()) bool
	End(timeout time.Duration, expectNum int64) error
	AddDelta(num int64)
}

type UnitTestCounterHelper struct {
	signal   chan struct{}
	num      int64
	callback func()
}

func (a *UnitTestCounterHelper) Begin(callback func()) bool {
	a.signal = make(chan struct{})
	a.callback = callback
	go func() {
		<-a.signal
		a.callback()
	}()
	return true
}

func (a *UnitTestCounterHelper) End(timeout time.Duration, expectNum int64) error {
	begin := time.Now()
	for {
		if time.Since(begin).Nanoseconds() > timeout.Nanoseconds() {
			return fmt.Errorf("timeout because unable to go enough events, expect: %d, current: %d", expectNum, atomic.LoadInt64(&a.num))
		}
		if atomic.LoadInt64(&a.num) == expectNum {
			close(a.signal)
			break
		}
		time.Sleep(time.Millisecond)
	}
	return nil
}

func (a *UnitTestCounterHelper) AddDelta(num int64) {
	atomic.AddInt64(&a.num, num)
}
